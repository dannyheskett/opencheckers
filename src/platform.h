#ifndef OPENCHECKERS_PLATFORM_H
#define OPENCHECKERS_PLATFORM_H

// The game's name: window title and recording file prefix.
#define GAME_NAME "opencheckers"

// Recording size (recorder.c): the landscape board with its bars and margins.
// Both are multiples of 16 for the H.264 encoder.
#define REC_W 624
#define REC_H 704

// OC_TOUCH selects the touch-first mobile frontend: the adaptive portrait
// layout and tap-driven board and menus. It is enabled on Android, iOS, and the
// WebAssembly build (which targets mobile browsers but also accepts mouse and
// keyboard for desktop browsers). Desktop native builds leave it unset and use
// the fixed-board landscape layout with mouse and keyboard input.
//
// raylib defines PLATFORM_ANDROID / PLATFORM_WEB for its own sources; our build
// passes the matching -D for the game translation units.
#if defined(PLATFORM_ANDROID) || defined(PLATFORM_WEB) || defined(PLATFORM_IOS)
#define OC_TOUCH 1
#endif

// The two renderers, selected by availability:
//   OC_PORTRAIT  — the touch-first portrait renderer (board sized to the screen
//                  width). Available on Android, iOS, and web.
//   OC_LANDSCAPE — the desktop renderer (fixed-size board centred in a freely
//                  resizable window, shrunk to fit a smaller one). Available on desktop native and web.
// Native desktop compiles only landscape; Android and iOS only portrait; the web build
// compiles BOTH and chooses at runtime (desktop browser -> landscape, phone ->
// portrait), so a laptop browser gets the same look as the native desktop app.
#ifdef OC_TOUCH
#define OC_PORTRAIT 1
#endif
#if !defined(PLATFORM_ANDROID) && !defined(PLATFORM_IOS)
#define OC_LANDSCAPE 1
#endif

// True only on the build that has both renderers and must pick at runtime (web).
#if defined(OC_PORTRAIT) && defined(OC_LANDSCAPE)
#define OC_RUNTIME_RENDERER 1
#endif

#endif // OPENCHECKERS_PLATFORM_H
