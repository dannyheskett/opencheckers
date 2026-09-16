# App Store assets

Artwork and listing copy for the iOS App Store, mirroring `android/play-assets/`
for Google Play. Nothing here is compiled into the app — the in-bundle app icon
lives in `ios/Assets.xcassets`, not in this folder.

Until the App Store Connect record exists and the `IOS_*` / `ASC_*` secrets are
set, the release workflow produces an unsigned `.ipa` and skips the TestFlight
upload. **[TESTFLIGHT.md](TESTFLIGHT.md) is the step-by-step for turning it on**
— no Mac required.

## Required

| Asset | Size | Notes |
| --- | --- | --- |
| `icon-1024.png` | 1024x1024 | **No alpha channel, no transparency, no rounded corners.** Apple masks the corners itself; a submitted icon with an alpha channel is rejected outright. sRGB, flattened. `scripts/gen_icons.py` flattens it to RGB for exactly this reason. |
| `screenshots/iphone-6.9/` | 1290x2796 | Required. 1-10 images, portrait. Covers every current iPhone; Apple scales this set down for older devices, so no other iPhone size is needed. |

opencheckers ships **iPhone only** (`UIDeviceFamily = [1]` in `ios/Info.plist`), so
no iPad screenshots are needed. iPads can still install and run it scaled. If
iPad is ever declared, add `2` to `UIDeviceFamily`, set `UIRequiresFullScreen`
to opt out of Split View, and add a `screenshots/ipad-13/` set at 2064x2752 —
Apple requires that set whenever iPad is supported.

Screenshots must be PNG or JPEG, sRGB, with no alpha channel. They are ordered
in the listing by filename, hence the `01-`/`02-` prefixes.

## Generating screenshots

`scripts/gen_store_screenshots.mjs` captures this set and the two Play sets in
one run, straight from the web build at the exact target size, and writes them as
RGB PNGs:

    python3 scripts/gen_icons.py                  # icon-1024.png (and Play icons)
    npm i playwright-core
    ./scripts/build_raylib_web.sh && make web
    node scripts/gen_store_screenshots.mjs --src build/web

No resize step and no `matchMedia` shim: the browser context is created with
`hasTouch`, which makes `matchMedia('(pointer: coarse)')` match natively, and
that is exactly what `src/main.c` reads to select the portrait renderer. The
script plays Red by reading the board back from each frame and tapping pieces
and their dotted targets.

Traps the script already handles, worth knowing if it is ever rewritten:

- A pointer move and press inside one frame records the gesture origin at the
  PREVIOUS position, so the delta reads as a drag and the tap never fires.
  Settle after moving, before pressing.
- A press has to be held past a couple of frames. 60ms is silently dropped;
  120ms is reliable.
- Reading the WebGL canvas back from page script returned the previous frame
  under SwiftShader, so the board is read from page screenshots instead.
- The menu is a two-finger tap, which would need CDP `Input.dispatchTouchEvent`
  (Playwright's mouse and touchscreen APIs are both single-pointer); the capture
  never needs it.
