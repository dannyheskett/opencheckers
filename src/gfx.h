#ifndef OPENCHECKERS_GFX_H
#define OPENCHECKERS_GFX_H

#include "oc_types.h"

// Immediate-mode 2D drawing primitives — the entire drawing surface the shared
// game renderer needs. Two backends implement this identically-behaving API:
//   gfx_raylib.c — wraps raylib (desktop / web / android). Behaviour-identical
//                  to the direct raylib calls it replaces.
//   gfx_metal.mm — native Metal (iOS), no raylib.
// The portrait/shared code in render.c calls these instead of raylib directly,
// so the layout logic is shared and only the primitives are swapped per platform.
// Render textures (the recorder's supersampled capture) stay raylib-only in the
// landscape renderer, which never compiles on iOS.

#ifdef __cplusplus
extern "C" {
#endif

// Load the bundled UI font. Must be called once after the window / GL context
// exists (the backends also lazy-load on first text draw as a fallback).
void gfx_font_init(void);

void gfx_begin_frame(void);
void gfx_end_frame(void);
void gfx_clear(Color color);

void gfx_rect(int x, int y, int w, int h, Color color);
void gfx_rect_lines(int x, int y, int w, int h, Color color);
void gfx_line(int x1, int y1, int x2, int y2, Color color);

// Checker pieces and the crown glyph. Vertices of gfx_triangle go
// counter-clockwise, as raylib's DrawTriangle requires.
void gfx_circle(int cx, int cy, float radius, Color color);
void gfx_circle_lines(int cx, int cy, float radius, Color color);
void gfx_triangle(float x1, float y1, float x2, float y2, float x3, float y3, Color color);

// Rounded menu panel. `radius` is the corner radius in pixels.
void gfx_rect_rounded(int x, int y, int w, int h, int radius, Color color);
void gfx_rect_rounded_lines(int x, int y, int w, int h, int radius, Color color);

void gfx_text(const char* text, int x, int y, int font_size, Color color);
int  gfx_measure_text(const char* text, int font_size);

#ifdef __cplusplus
}
#endif

#endif // OPENCHECKERS_GFX_H
