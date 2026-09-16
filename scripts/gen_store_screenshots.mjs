// Capture the Google Play and App Store screenshots from the web build.
//
// The store listings need screenshots of the PORTRAIT touch layout, which is the
// same renderer Android and iOS run (src/render_portrait.c) -- the web build
// picks it whenever the primary pointer is coarse, so a headless browser with
// touch emulation produces pixel-equivalent frames without a device or a farm.
//
// Regenerate whenever the portrait UI changes. The committed PNGs under
// android/play-assets/screenshots/ and ios/app-store-assets/screenshots/ are what
// scripts/play_release.py and scripts/asc_release.py push to the stores.
//
// Usage:
//   npm i playwright-core                    # not a repo dependency; dev-only
//   make web   (or: gh release download release-N -p '*-web-wasm.zip' && unzip it)
//   node scripts/gen_store_screenshots.mjs --src build/web
//
// Options:
//   --src <dir>      web bundle directory (contains opencheckers.html) [required]
//   --out <dir>      repo root to write screenshots under         [default: cwd]
//   --chrome <path>  Chromium binary  [default: $CHROME, else the Playwright cache]
//   --only <name>    capture a single target (play-phone|play-tablet|ios-6.9)
//
// Why a browser and not the real app: the maintainer has no Android device and no
// Mac, and Device Farm returns video, not clean full-resolution stills.

import { chromium } from 'playwright-core';
import { createServer } from 'node:http';
import { readFile, mkdir } from 'node:fs/promises';
import { existsSync, readdirSync } from 'node:fs';
import { extname, join, resolve } from 'node:path';
import { homedir } from 'node:os';

// ---------------------------------------------------------------------------
// Targets. Play's tablet slot takes the same 9:16 frames at 2x, which is why one
// capture pass fills both the 7-inch and 10-inch slots (see play-assets/LISTING.md).
// The App Store's 6.9" slot is the only iPhone size that covers every device.
// ---------------------------------------------------------------------------
const TARGETS = [
  { name: 'play-phone',  w: 1080, h: 1920, out: 'android/play-assets/screenshots/phone' },
  { name: 'play-tablet', w: 2160, h: 3840, out: 'android/play-assets/screenshots/tablet' },
  { name: 'ios-6.9',     w: 1290, h: 2796, out: 'ios/app-store-assets/screenshots/iphone-6.9' },
];

// The mid-game frame is taken once this many pieces have come off the board (or
// a king appears), so it shows a game that has actually developed. MAX_TURNS is
// a safety stop, not the normal exit.
const MIDGAME_CAPTURED = 5;
const MAX_TURNS = 40;

// Board colours from src/render.c, as read back from the canvas.
const SQ_LIGHT = [232, 212, 170];

const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm',
               '.data': 'application/octet-stream', '.png': 'image/png' };

function arg(flag, fallback) {
  const i = process.argv.indexOf(flag);
  return i > -1 && process.argv[i + 1] ? process.argv[i + 1] : fallback;
}

function findChrome() {
  const explicit = arg('--chrome', process.env.CHROME);
  if (explicit) return explicit;
  const cache = join(homedir(), '.cache/ms-playwright');
  if (!existsSync(cache)) return null;
  // Highest chromium-<rev> wins; Playwright keeps several revisions side by side.
  const dirs = readdirSync(cache)
    .filter(d => /^chromium-\d+$/.test(d))
    .sort((a, b) => +b.split('-')[1] - +a.split('-')[1]);
  for (const d of dirs) {
    for (const exe of ['chrome-linux64/chrome', 'chrome-linux/chrome', 'chrome-mac/Chromium.app/Contents/MacOS/Chromium']) {
      const p = join(cache, d, exe);
      if (existsSync(p)) return p;
    }
  }
  return null;
}

async function serve(dir) {
  const server = createServer(async (req, res) => {
    const rel = decodeURIComponent(req.url.split('?')[0]).replace(/^\/+/, '') || 'opencheckers.html';
    try {
      const body = await readFile(join(dir, rel));
      res.writeHead(200, { 'Content-Type': MIME[extname(rel)] || 'application/octet-stream' });
      res.end(body);
    } catch {
      res.writeHead(404).end('not found');
    }
  });
  await new Promise(r => server.listen(0, '127.0.0.1', r));
  return { server, port: server.address().port };
}

// Store listings reject screenshots with an alpha channel; the browser writes
// RGBA. Re-encode as plain RGB PNG (colour type 2) with zlib, no dependencies.
// The same decoder reads the board back from screenshots while playing.
import { deflateSync, inflateSync } from 'node:zlib';
function crc32(buf) {
  let c, crc = 0xffffffff;
  for (let n = 0; n < buf.length; n++) {
    c = (crc ^ buf[n]) & 0xff;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    crc = (crc >>> 8) ^ c;
  }
  return (crc ^ 0xffffffff) >>> 0;
}
function chunk(type, data) {
  const len = Buffer.alloc(4); len.writeUInt32BE(data.length);
  const td = Buffer.concat([Buffer.from(type), data]);
  const crc = Buffer.alloc(4); crc.writeUInt32BE(crc32(td));
  return Buffer.concat([len, td, crc]);
}
// Decode an 8-bit RGB or RGBA PNG (as the browser writes) to RGB rows.
function decodePng(png) {
  let off = 8, w = 0, h = 0, ct = 0;
  const idat = [];
  while (off < png.length) {
    const len = png.readUInt32BE(off), type = png.toString('ascii', off + 4, off + 8);
    const data = png.subarray(off + 8, off + 8 + len);
    if (type === 'IHDR') { w = data.readUInt32BE(0); h = data.readUInt32BE(4); ct = data[9]; }
    if (type === 'IDAT') idat.push(data);
    off += 12 + len;
  }
  if (ct !== 2 && ct !== 6) throw new Error(`unexpected PNG colour type ${ct}`);
  const bpp = ct === 6 ? 4 : 3, stride = w * bpp;
  const raw = inflateSync(Buffer.concat(idat));
  const rgb = Buffer.alloc(w * h * 3);
  const prev = Buffer.alloc(stride), cur = Buffer.alloc(stride);
  for (let y = 0; y < h; y++) {
    const f = raw[y * (stride + 1)];
    raw.copy(cur, 0, y * (stride + 1) + 1, (y + 1) * (stride + 1));
    for (let i = 0; i < stride; i++) {          // undo the PNG row filter
      const a = i >= bpp ? cur[i - bpp] : 0, b = prev[i], c = i >= bpp ? prev[i - bpp] : 0;
      let v = cur[i];
      if (f === 1) v += a; else if (f === 2) v += b; else if (f === 3) v += (a + b) >> 1;
      else if (f === 4) { const p = a + b - c, pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c);
                          v += pa <= pb && pa <= pc ? a : pb <= pc ? b : c; }
      cur[i] = v & 0xff;
    }
    for (let x = 0; x < w; x++) cur.copy(rgb, (y * w + x) * 3, x * bpp, x * bpp + 3);
    cur.copy(prev);
  }
  return { w, h, rgb };
}

function encodeRgbPng({ w, h, rgb }) {
  const out = Buffer.alloc(h * (1 + w * 3));
  for (let y = 0; y < h; y++) {
    out[y * (1 + w * 3)] = 0;
    rgb.copy(out, y * (1 + w * 3) + 1, y * w * 3, (y + 1) * w * 3);
  }
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4); ihdr[8] = 8; ihdr[9] = 2;
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr),
                        chunk('IDAT', deflateSync(out)), chunk('IEND', Buffer.alloc(0))]);
}

async function capture(target, url, chrome) {
  const { w, h, out } = target;
  const browser = await chromium.launch({
    executablePath: chrome,
    // SwiftShader: the runner has no GPU, and raylib needs a real WebGL context.
    args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'],
  });
  // hasTouch is the whole trick: Chromium's touch emulation makes
  // matchMedia('(pointer: coarse)') match, which is exactly what src/main.c reads
  // to choose the portrait renderer. No page-script shim is needed.
  const ctx = await browser.newContext({
    viewport: { width: w, height: h }, deviceScaleFactor: 1, hasTouch: true,
  });
  const page = await ctx.newPage();
  await page.goto(url, { waitUntil: 'load' });
  await page.waitForTimeout(6000);   // WASM boot + font upload

  const wait = ms => page.waitForTimeout(ms);
  const { writeFile } = await import('node:fs/promises');
  const shot = async name => writeFile(join(out, `${name}.png`), encodeRgbPng(decodePng(await page.screenshot())));

  // The input layer samples the pointer once a frame and decides a tap on
  // release. A move+press inside a single frame records the origin at the
  // PREVIOUS position, so the tap reads as a drag and never fires -- every press
  // below therefore settles first. A press also has to be held for more than a
  // couple of frames; 60ms is silently dropped, 120ms is reliable. SwiftShader
  // renders a 4K tablet frame slowly, so the canvas is only read back once the
  // frame after the tap has certainly been drawn.
  const settle = async (x, y) => { await page.mouse.move(x, y); await wait(80); };
  async function tap(x, y) {
    await settle(x, y);
    await page.mouse.down(); await wait(120); await page.mouse.up(); await wait(450);
  }

  // Read pixels back from a screenshot of the page. (Reading the WebGL canvas
  // from page script returned the previous frame under SwiftShader; a screenshot
  // always waits for the current one.) Returns RGB triples at the given points.
  const pixels = async pts => {
    const img = decodePng(await page.screenshot());
    return pts.map(([px, py]) => {
      const i = (Math.round(py) * img.w + Math.round(px)) * 3;
      return [img.rgb[i], img.rgb[i + 1], img.rgb[i + 2]];
    });
  };
  const near = (p, q, tol = 24) => p.every((v, i) => Math.abs(v - q[i]) <= tol);

  // The menu's selected row is drawn in the SEL_RING yellow; find its y by
  // scanning the centre column.
  async function selectedMenuRowY() {
    const ys = [];
    for (let y = 0; y < h; y += 2) ys.push(y);
    const cols = [];
    for (let dx = -w * 0.2; dx <= w * 0.2; dx += w * 0.01) cols.push(w / 2 + dx);
    const pts = ys.flatMap(y => cols.map(x => [x, y]));
    const px = await pixels(pts);
    const hits = [];
    px.forEach((p, i) => { if (p[0] > 200 && p[1] > 190 && p[2] < 160 && p[2] > 60) hits.push(pts[i][1]); });
    if (!hits.length) throw new Error(`${target.name}: no highlighted menu row`);
    return (Math.min(...hits) + Math.max(...hits)) / 2;
  }

  // Board geometry, mirroring band_layout() in src/render_portrait.c: the board
  // is width-limited on every target, so the square size follows from the width;
  // the top edge is found by scanning down column 0 for the first light square.
  const m = Math.max(6, Math.floor(Math.min(w, h) / 28));
  const sq = Math.floor((w - 2 * m) / 8);
  const left = Math.floor((w - 8 * sq) / 2);
  async function boardTop() {
    const x = left + sq / 2, ys = [];
    for (let y = Math.floor(h * 0.05); y < h * 0.6; y++) ys.push(y);
    const px = await pixels(ys.map(y => [x, y]));
    const i = px.findIndex(p => near(p, SQ_LIGHT, 6));
    if (i < 0) throw new Error(`${target.name}: board not found`);
    return ys[i];
  }

  // Classify every dark square: 'r' / 'b' piece (upper-case for a king), '.'
  // empty, '*' empty with a legal-target dot.
  async function readBoard(top) {
    const r = sq * 0.38, pts = [];
    for (let row = 0; row < 8; row++)
      for (let col = 0; col < 8; col++) {
        const cx = left + col * sq + sq / 2, cy = top + row * sq + sq / 2;
        pts.push([cx, cy + r * 0.74], [cx, cy]);
      }
    const px = await pixels(pts);
    const b = [];
    for (let row = 0; row < 8; row++) {
      b.push([]);
      for (let col = 0; col < 8; col++) {
        const [face, centre] = [px[(row * 8 + col) * 2], px[(row * 8 + col) * 2 + 1]];
        let s = ' ';
        if ((row + col) % 2 === 1) {
          const king = centre[0] > 200 && centre[1] > 170 && centre[2] < 110;
          if (face[0] > 150 && face[1] < 110) s = king ? 'R' : 'r';
          else if (face[0] < 90 && face[1] < 90 && face[2] < 100) s = king ? 'B' : 'b';
          else if (centre[0] > 190 && centre[1] > 170 && centre[2] < 140) s = '*';
          else s = '.';
        }
        b[row].push(s);
      }
    }
    return b;
  }
  const count = (b, re) => b.flat().filter(s => re.test(s)).length;
  const at = (top, row, col) => [left + col * sq + sq / 2, top + row * sq + sq / 2];

  let seed = 11;                         // fixed: a rerun plays the same way
  const rnd = () => (seed = (seed * 1103515245 + 12345) & 0x7fffffff) / 0x7fffffff;

  // One human turn as Red: tap pieces until one shows target dots, take a
  // capture if the dots offer one (captures are mandatory, so the game only
  // dots capturing pieces when one exists), and keep jumping while the piece
  // stays locked. `onSelect` sees the board with a piece selected, before moving.
  async function playTurn(top, onSelect) {
    let b = await readBoard(top);
    const reds = [];
    b.forEach((row, r) => row.forEach((s, c) => { if (s === 'r' || s === 'R') reds.push([r, c]); }));
    reds.sort(() => rnd() - 0.5);
    for (const [r, c] of reds) {
      await tap(...at(top, r, c));
      b = await readBoard(top);
      const targets = [];
      b.forEach((row, tr) => row.forEach((s, tc) => { if (s === '*') targets.push([tr, tc]); }));
      if (!targets.length) continue;
      if (onSelect) await onSelect(targets);
      let pr = r;
      for (let hop = 0; hop < 8 && targets.length; hop++) {
        const jump = targets.find(([tr]) => Math.abs(tr - pr) === 2);
        const [tr, tc] = jump || targets.sort((p, q) => p[0] - q[0])[0];
        await tap(...at(top, tr, tc));
        if (!jump) break;
        pr = tr;
        b = await readBoard(top);
        targets.length = 0;
        b.forEach((row, rr) => row.forEach((s, cc) => { if (s === '*') targets.push([rr, cc]); }));
      }
      return true;
    }
    return false;
  }

  await mkdir(out, { recursive: true });

  // 1. Title menu, untouched. Sound reads "Off" because it genuinely is off by
  //    default on every platform (src/sound.c) -- not a capture artifact.
  await shot('01-menu');

  await tap(w / 2, await selectedMenuRowY());   // New Game
  await wait(1200);
  const top = await boardTop();
  if (count(await readBoard(top), /[rR]/) !== 12) throw new Error(`${target.name}: not a fresh board after New Game`);

  const waitForReply = () => wait(2500);        // AI thinking delay + search
  let selectDone = false;
  for (let turn = 0; turn < MAX_TURNS; turn++) {
    const onSelect = async targets => {
      // 3. A selected piece with its legal targets dotted, a few turns in.
      if (!selectDone && turn >= 3 && targets.length >= 1) { await shot('03-select'); selectDone = true; }
    };
    if (!await playTurn(top, onSelect)) throw new Error(`${target.name}: no legal move found (game over?)`);
    await waitForReply();

    // 2. The opening after two exchanges, with the last-move highlight.
    if (turn === 1) await shot('02-opening');

    const b = await readBoard(top);
    const pieces = count(b, /[rRbB]/);
    if (process.env.OC_DEBUG) console.log(`  ${target.name} turn ${turn}: ${pieces} pieces\n${b.map(r => r.join('')).join('\n')}`);
    if (pieces === 0 || count(b, /[rR]/) === 0) throw new Error(`${target.name}: game ended at turn ${turn}`);
    if (selectDone && (24 - pieces >= MIDGAME_CAPTURED || /[RB]/.test(b.flat().join('')))) {
      // 4. A developed game: captures made, maybe a king.
      await shot('04-midgame');
      await browser.close();
      console.log(`${target.name}: 4 frames -> ${out}`);
      return;
    }
  }
  throw new Error(`${target.name}: game never developed in ${MAX_TURNS} turns`);
}

const src = arg('--src');
if (!src) { console.error('--src <web bundle dir> is required'); process.exit(2); }
const root = resolve(arg('--out', process.cwd()));
const chrome = findChrome();
if (!chrome) { console.error('no Chromium found; pass --chrome or set $CHROME'); process.exit(2); }

const { server, port } = await serve(resolve(src));
const url = `http://127.0.0.1:${port}/opencheckers.html`;
const only = arg('--only');

// Play is emergent (the AI breaks ties randomly), so a run can occasionally lose
// or stall before the game develops. The board is worth replaying rather than
// failing the batch.
for (const t of TARGETS) {
  if (only && t.name !== only) continue;
  const target = { ...t, out: join(root, t.out) };
  for (let attempt = 1; ; attempt++) {
    try { await capture(target, url, chrome); break; }
    catch (e) {
      if (attempt === 3) throw e;
      console.log(`${t.name}: attempt ${attempt} failed (${e.message}); retrying`);
    }
  }
}
server.close();
