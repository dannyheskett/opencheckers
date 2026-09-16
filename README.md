# opencheckers

Checkers (American / English draughts) written in C: classic, familiar controls,
rules, layout, and visuals. Play the computer at three difficulty levels. It runs
natively on Windows, macOS, Linux, Android, and iOS, and in the browser via
WebAssembly.

Rendering, input, and audio go through raylib 6.0 on every platform except iOS,
which uses a native Metal backend with no raylib (see
[Architecture](#architecture)). The game logic (`src/game.c`) is
platform-independent and shared unchanged.

## Platforms

| Platform | Build | Renderer | Input |
|----------|-------|----------|-------|
| Linux / Windows / macOS | native (raylib) | landscape | mouse + keyboard |
| Web (WASM) | Emscripten (raylib) | landscape **or** portrait, chosen at runtime | mouse + keyboard + touch |
| Android | NativeActivity (raylib) | portrait | touch |
| iOS | native Metal (no raylib) | portrait | touch |

## Rendering modes

There are two renderers. Native desktop compiles only landscape; Android and iOS
compile only portrait; the web build compiles both and selects one at runtime.

- **Landscape** — the window is freely **resizable**, but the **board is a fixed
  size** with a minimum window just big enough to play (the openklondike model).
  A title bar sits above the board and a status bar below it.
- **Portrait** — adaptive to the live screen size. A thin OPENCHECKERS title bar
  is pinned to the top, a status band (whose move, difficulty, piece counts)
  sits below it, and the board fills the width at the largest square that fits,
  with a uniform margin on every edge. The game is controlled entirely by taps.

The web build picks the renderer from the pointer type
(`matchMedia('(pointer: coarse)')`): a phone or tablet gets portrait, a desktop
browser gets landscape.

## Rules

American / English draughts on an 8×8 board, 12 pieces each:

- Men move and capture one square diagonally **forward**; kings move one square
  in any diagonal direction.
- **Captures are mandatory.** A multi-jump must continue with the same piece
  until it can jump no more.
- Reaching the far row crowns the piece a king and **ends the turn**.
- You lose when you have no pieces, or no legal move.

## Controls

**Mouse + keyboard** (desktop, and desktop browsers):

- **Click** one of your pieces to select it; its legal destinations are dotted.
- **Click** a dotted square to move. During a multi-jump the piece stays
  selected until the chain ends.
- **Escape**: menu (your game is kept and can be resumed); on the menu, exit
- **Alt+Enter**: toggle fullscreen
- **Up / Down** (or W / S) + **Enter / Space**: menu navigation

**Touch** (Android, iOS, and mobile browsers):

- **Tap** a piece to select it, then **tap** a dotted square to move
- **Two-finger tap**: menu (the game stays resumable); Android **Back** does the same
- **Tap** a menu item to choose it; **swipe up / down** also moves the highlight

Choose **Easy / Medium / Hard** and which colour you play from the menu.

## Building

raylib is built once from source into a gitignored install directory (per
platform) before the game is built. Each `scripts/build_raylib_*.sh` clones
raylib (pinned via `RAYLIB_TAG`, default `6.0`) and installs its headers and
`libraylib.a`. CI runs these scripts before each build.

### Desktop

```bash
./scripts/build_raylib_linux.sh      # once, on a fresh clone
make                                 # -> build/opencheckers   (dev, -O2)
make run
make release                         # -> build/opencheckers-release (-O3)
```

Windows (mingw-w64 cross-compile) and macOS (universal arm64 + x86_64):

```bash
./scripts/build_raylib_windows.sh && make windows   # -> build/opencheckers-x64.exe, -x86.exe
./scripts/build_raylib_mac.sh     && make mac        # -> build/opencheckers-mac
```

### Android (needs the Android SDK + NDK)

```bash
./scripts/build_raylib_android.sh
make android        # -> build/opencheckers.apk   (debug-signed, sideloadable)
make android-play   # -> build/opencheckers.aab   (Play App Bundle; PLAY_* signing vars)
```

The app is a `NativeActivity` (no Gradle); a small `OpencheckersActivity` Java
class (compiled with `javac` + `d8`) enables immersive full-screen. arm64-v8a,
`targetSdk` 36, 16 KB-page aligned.

### iOS (needs macOS + Xcode; no raylib)

```bash
make ios-sim   # -> build/ios-sim/Opencheckers.app   (Simulator, arm64)
make ios       # -> build/opencheckers.ipa           (device arm64, unsigned)
```

The `.ipa` is unsigned unless `IOS_SIGN_IDENTITY` / `IOS_PROFILE` / `IOS_TEAM_ID`
are set; AWS Device Farm re-signs an unsigned one on upload. C sources build with
`clang`, the Objective-C++ backend (`ios/`) with `clang++`.

### Web (needs Emscripten)

```bash
./scripts/build_raylib_web.sh
make web        # -> build/web/opencheckers.{html,js,wasm}
make web-serve  # http://localhost:8080/opencheckers.html
```

Serve `build/web` over HTTP (not `file://`) and open `opencheckers.html`.

## Tests

Unit tests with no raylib or window required — the game rules and AI, the
fixed-timestep clock, and the touch tap recognizer (tap / drag rejection /
two-finger tap / swipes, driven frame-by-frame through a scripted touch
surface):

```bash
make test
```

There is also a real-device engine check: `make android SIMSTATS=1` builds an
APK that boots straight into autoplay (the AI plays both sides) and logs
rendered-frames vs sim-steps once per second; the manual
[`devicefarm`](.github/workflows/devicefarm.yml) workflow (with `simstats`
enabled and a 120 Hz `device_model`) runs it on AWS Device Farm and fails unless
the simulation held ~60 steps/s at the display's real refresh rate.

## Continuous integration and releases

Every pull request to `main` builds all platforms via GitHub Actions
([`ci.yml`](.github/workflows/ci.yml)) — Linux, Windows (x64/x86), macOS
(universal), Android (APK + a packaging check of the AAB), Web (WASM), and iOS
(a Metal app booted in the Simulator and screenshotted) — and runs `make test`.
All checks must pass to merge.

Pushing to `main` cuts the next `release-N` via
[`release.yml`](.github/workflows/release.yml), which attaches per-platform
archives, the Android APK, the iOS `.ipa`, and the WASM bundle to the GitHub
Release. When the store secrets are set it also uploads the AAB to the Play
internal track, uploads the `.ipa` to TestFlight, and submits it to App Review.
[`store-release.yml`](.github/workflows/store-release.yml) promotes a build to
Play's public tracks and pushes the listings. Setup:
[`android/play-assets/KEYSTORE.md`](android/play-assets/KEYSTORE.md) and
[`ios/app-store-assets/TESTFLIGHT.md`](ios/app-store-assets/TESTFLIGHT.md).

## Recording (desktop only)

Toggle **Record: On/Off** from the menu to capture your session to an H.264 MP4
(`opencheckers-YYYYMMDD-HHMMSS.mp4`). No external tools required. The capture is
supersampled so the video is as crisp as the live game. Recording is compiled
out of the mobile and web builds.

```bash
./build/opencheckers --record            # auto-named file
./build/opencheckers --record clip.mp4   # explicit path
```

## Architecture

- `src/game.c` — pure game logic and AI (no rendering/input/audio), shared by all
  platforms and covered by `make test`.
- `src/render.c` draws through a small immediate-mode primitive layer
  (`src/gfx.h`): `src/gfx_raylib.c` wraps raylib (desktop / web / android);
  `ios/gfx_metal.mm` is a native Metal implementation. `src/oc_types.h` supplies
  raylib-compatible types so the shared code compiles without raylib on iOS.
  `src/render_landscape.c` and `src/render_portrait.c` are the two layouts.
- Audio is a similar seam (`src/audio.h`): `src/audio_raylib.c` (raylib) vs
  `ios/audio_ios.mm` (AVAudioEngine). Effects are synthesized at startup (no
  audio files); sound is off by default.
- iOS backend: `ios/plat_ios.mm` (touch / screen / timing) and `ios/ios_main.mm`
  (UIKit app + `CAMetalLayer` view + `CADisplayLink` loop).
- All text is drawn in the bundled Nunito SemiBold typeface (SIL OFL, see
  `NOTICE`), embedded so there is no runtime asset file. The raylib backends
  `LoadFontFromMemory` the TTF from `src/font_nunito.h` (generated by
  `scripts/embed_ttf.py`); iOS samples a pre-baked glyph atlas in
  `src/font_atlas.h` (generated by `scripts/gen_font_atlas.c`). Both use the
  same proportional tracking, so every platform lays text out identically.
- Store artwork is generated: `scripts/gen_icons.py` (launcher / store icons,
  feature graphic) and `scripts/gen_store_screenshots.mjs` (Play and App Store
  screenshots, from the web build).

## Dependencies

- A C99 compiler (GCC or Clang); a C++ / Objective-C++ compiler for the iOS
  backend.
- [raylib](https://github.com/raysan5/raylib) 6.0 (static) on all platforms
  except iOS, built by the `scripts/build_raylib_*.sh` helpers.
- The MP4 recorder uses two vendored public-domain (CC0) single-header
  libraries: [minih264](third_party/minih264) (H.264 encoder) and
  [minimp4](third_party/minimp4) (MP4 muxer). No external tools or shared
  libraries.

## Project structure

```
opencheckers/
├── src/            # shared C sources + gfx/audio raylib backends
├── ios/            # native Metal / UIKit backend (Objective-C++) + App Store assets
├── android/        # NativeActivity manifest, resources, Java activity + Play assets
├── web/            # Emscripten HTML shell
├── scripts/        # raylib build scripts, asset/font generators, store tooling
├── third_party/    # vendored single-header libs (minih264, minimp4) + Nunito
├── tests/          # game-logic and touch-input unit tests
├── Makefile
├── LICENSE         # MIT (this project's own code)
└── NOTICE          # third-party attributions
```

## License

opencheckers' own code is released under the [MIT License](LICENSE). The vendored
`minih264` and `minimp4` libraries are public domain (CC0), and the Nunito font is
under the SIL Open Font License; see [NOTICE](NOTICE) for attributions.
