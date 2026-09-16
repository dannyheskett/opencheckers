// raylib backend for the gfx primitive layer: each entry point is a 1:1 wrapper
// over the raylib call it replaces, so desktop / web / android rendering is
// identical across those platforms. Text is the one exception: it is drawn with
// the bundled Nunito font (loaded from the embedded TTF) via DrawTextEx rather
// than raylib's built-in pixel font. iOS uses gfx_metal.mm instead and never
// compiles this file.
#include "gfx.h"
#include "font_nunito.h"
#include <raylib.h>
#include <stddef.h>  // NULL

// Bake the glyph atlas well above the largest on-screen size (menu titles reach
// ~40px) so every draw downsamples — crisp with bilinear filtering.
#define GFX_FONT_BAKE 64

static Font s_font;
static bool s_font_ready = false;

// A hair of inter-glyph tracking, proportional to size, improves legibility
// without looking loose. gfx_text and gfx_measure_text MUST agree so centering
// stays correct. The iOS backend uses the same ratio.
static float text_spacing(float fs) { return fs * 0.05f; }

static void ensure_font(void) {
    if (s_font_ready || !IsWindowReady()) return;
    s_font = LoadFontFromMemory(".ttf", nunito_ttf, (int)nunito_ttf_len,
                                GFX_FONT_BAKE, NULL, 0);
    SetTextureFilter(s_font.texture, TEXTURE_FILTER_BILINEAR);
    s_font_ready = true;
}

void gfx_font_init(void) { ensure_font(); }

void gfx_begin_frame(void) { BeginDrawing(); }
void gfx_end_frame(void)   { EndDrawing(); }
void gfx_clear(Color color) { ClearBackground(color); }

void gfx_rect(int x, int y, int w, int h, Color color) {
    DrawRectangle(x, y, w, h, color);
}
void gfx_rect_lines(int x, int y, int w, int h, Color color) {
    DrawRectangleLines(x, y, w, h, color);
}
void gfx_line(int x1, int y1, int x2, int y2, Color color) {
    DrawLine(x1, y1, x2, y2, color);
}

void gfx_circle(int cx, int cy, float radius, Color color) {
    DrawCircle(cx, cy, radius, color);
}
void gfx_circle_lines(int cx, int cy, float radius, Color color) {
    DrawCircleLines(cx, cy, radius, color);
}
void gfx_triangle(float x1, float y1, float x2, float y2, float x3, float y3, Color color) {
    DrawTriangle((Vector2){x1, y1}, (Vector2){x2, y2}, (Vector2){x3, y3}, color);
}

// raylib expresses corner rounding as a fraction of the shorter side; convert
// from the pixel radius the shared renderer works in.
#define GFX_ROUNDED_SEGMENTS 8
static float roundness(int w, int h, int radius) {
    int m = (w < h) ? w : h;
    return (m > 0) ? (2.0f * (float)radius) / (float)m : 0.0f;
}
void gfx_rect_rounded(int x, int y, int w, int h, int radius, Color color) {
    DrawRectangleRounded((Rectangle){(float)x, (float)y, (float)w, (float)h},
                         roundness(w, h, radius), GFX_ROUNDED_SEGMENTS, color);
}
void gfx_rect_rounded_lines(int x, int y, int w, int h, int radius, Color color) {
    DrawRectangleRoundedLines((Rectangle){(float)x, (float)y, (float)w, (float)h},
                              roundness(w, h, radius), GFX_ROUNDED_SEGMENTS, color);
}

void gfx_text(const char* text, int x, int y, int font_size, Color color) {
    ensure_font();
    if (!s_font_ready) { DrawText(text, x, y, font_size, color); return; }
    DrawTextEx(s_font, text, (Vector2){(float)x, (float)y},
               (float)font_size, text_spacing((float)font_size), color);
}
int gfx_measure_text(const char* text, int font_size) {
    ensure_font();
    if (!s_font_ready) return MeasureText(text, font_size);
    return (int)(MeasureTextEx(s_font, text, (float)font_size,
                               text_spacing((float)font_size)).x + 0.5f);
}
