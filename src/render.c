// Common renderer TU: palette and shared draw helpers (board, pieces, menu,
// verdict panel), window lifecycle, and the public entry points that dispatch to
// the active renderer. The two renderers live in render_portrait.c and
// render_landscape.c.
#include "render_internal.h"
#if !defined(PLATFORM_IOS)
#include <raylib.h>  // window/timing (InitWindow, …); absent on iOS
#endif

// render_use_portrait() reports the active renderer. Native builds have exactly
// one (compile-time constant); the web build has both and picks at runtime.
#if defined(OC_RUNTIME_RENDERER)
static bool s_portrait_mode = false;
void render_set_portrait(bool portrait) { s_portrait_mode = portrait; }
bool render_use_portrait(void) { return s_portrait_mode; }
#elif defined(OC_PORTRAIT)
void render_set_portrait(bool portrait) { (void)portrait; }
bool render_use_portrait(void) { return true; }   // Android / iOS: portrait only
#else
void render_set_portrait(bool portrait) { (void)portrait; }
bool render_use_portrait(void) { return false; }  // desktop native: landscape only
#endif

// --------------------------------------------------------------------------
// Colors
// --------------------------------------------------------------------------
const Color FELT               = { 12,  92,  52, 255};   // background felt
const Color FELT_DARK          = { 10,  76,  44, 255};   // title / status bars
static const Color SQ_LIGHT    = {232, 212, 170, 255};   // light board square
static const Color SQ_DARK     = {120,  82,  54, 255};   // dark board square (playable)
const Color BOARD_EDGE         = { 60,  40,  26, 255};
static const Color RED_PIECE   = {198,  52,  48, 255};
static const Color RED_HI      = {236, 110, 100, 255};
static const Color RED_LO      = {140,  30,  30, 255};
static const Color BLK_PIECE   = { 48,  48,  56, 255};
static const Color BLK_HI      = {110, 112, 124, 255};
static const Color BLK_LO      = { 20,  20,  26, 255};
static const Color CROWN_GOLD  = {235, 200,  60, 255};
static const Color CROWN_DARK  = {176, 132,  28, 255};   // crown band
static const Color CROWN_LIGHT = {255, 236, 150, 255};   // crown jewels
const Color SEL_RING           = {255, 235, 120, 255};
static const Color TARGET_DOT  = {255, 235, 120, 180};
static const Color LASTMOVE    = {255, 235, 120,  70};
const Color MENU_BG            = { 16,  40,  28, 255};
const Color TEXT_LIGHT         = {235, 235, 225, 255};
const Color TEXT_DIM           = {170, 190, 175, 255};

const char* const DIFF_NAME[3] = { "Easy", "Medium", "Hard" };

// Scale a detail sized for the 72px landscape square to square size `sq`,
// never going below its landscape value, so the desktop look is unchanged and
// large phone squares get proportionally heavier strokes.
static int scaled(int base_at_72, int sq) {
    int v = base_at_72 * sq / SQUARE;
    return (v < base_at_72) ? base_at_72 : v;
}

// --------------------------------------------------------------------------
// Pieces (vector art)
// --------------------------------------------------------------------------
// A three-point crown on a darker band, jewels on the tips and along the band.
// Sized from the piece radius `r`. Triangle vertices go apex, base-left,
// base-right: the counter-clockwise order raylib's DrawTriangle requires.
static void draw_crown(int cx, int cy, float r, Color gold, Color dark, Color light) {
    float w = r * 1.00f, h = r * 0.78f;
    float top = cy - h * 0.52f, bot = cy + h * 0.48f;
    float band = h * 0.22f, valley = top + h * 0.50f, tip = top + h * 0.14f;
    float L = cx - w / 2, R = cx + w / 2;

    gfx_rect((int)L, (int)valley, (int)(R - L + 0.5f), (int)(bot - band - valley + 0.5f), gold);
    gfx_triangle(L,  tip, L,              valley, cx - w * 0.17f, valley, gold);
    gfx_triangle(cx, top, cx - w * 0.22f, valley, cx + w * 0.22f, valley, gold);
    gfx_triangle(R,  tip, cx + w * 0.17f, valley, R,              valley, gold);
    gfx_rect((int)L, (int)(bot - band), (int)(R - L + 0.5f), (int)(band + 0.5f), dark);

    float jr = w * 0.075f;
    gfx_circle((int)L,  (int)tip, jr, light);
    gfx_circle(cx,      (int)top, jr, light);
    gfx_circle((int)R,  (int)tip, jr, light);
    for (int i = -1; i <= 1; i++)
        gfx_circle((int)(cx + i * w * 0.28f), (int)(bot - band / 2), jr * 0.8f, light);
}

static void draw_piece(int cx, int cy, int sq, Square s) {
    float r = sq * 0.38f;
    Color base = (square_color(s) == SIDE_RED) ? RED_PIECE : BLK_PIECE;
    Color hi   = (square_color(s) == SIDE_RED) ? RED_HI    : BLK_HI;
    Color lo   = (square_color(s) == SIDE_RED) ? RED_LO    : BLK_LO;

    gfx_circle(cx, cy + scaled(2, sq), r, (Color){0, 0, 0, 60});  // soft shadow
    gfx_circle(cx, cy, r, lo);                                     // rim
    gfx_circle(cx, cy, r * 0.86f, base);                           // face
    int ridge = scaled(2, sq);                                     // ridge
    for (int t = 0; t < ridge; t++) gfx_circle_lines(cx, cy, r * 0.6f + t, hi);
    if (square_is_king(s)) draw_crown(cx, cy, r, CROWN_GOLD, CROWN_DARK, CROWN_LIGHT);
}

// --------------------------------------------------------------------------
// Board
// --------------------------------------------------------------------------
void draw_board(const Game* g, BoardView v, int sel_r, int sel_c,
                const Pt* targets, int n_targets) {
    int sq = v.sq, px = 8 * sq;
    int edge = scaled(3, sq);
    for (int t = 0; t < scaled(1, sq); t++)
        gfx_rect_lines(v.left - edge + t, v.top - edge + t,
                       px + 2 * (edge - t), px + 2 * (edge - t), BOARD_EDGE);

    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++) {
            int x = v.left + c * sq, y = v.top + r * sq;
            bool dark = ((r + c) & 1) == 1;
            gfx_rect(x, y, sq, sq, dark ? SQ_DARK : SQ_LIGHT);
            if (g->has_last && ((r == g->last_from.r && c == g->last_from.c)
                             || (r == g->last_to.r   && c == g->last_to.c)))
                gfx_rect(x, y, sq, sq, LASTMOVE);
        }

    // selected square ring
    if (sel_r >= 0) {
        int x = v.left + sel_c * sq, y = v.top + sel_r * sq;
        int ring = scaled(3, sq);
        for (int t = 0; t < ring; t++)
            gfx_rect_lines(x + t, y + t, sq - 2 * t, sq - 2 * t, SEL_RING);
    }

    // pieces
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++) {
            Square s = g->board[r][c];
            if (s == EMPTY) continue;
            draw_piece(v.left + c * sq + sq / 2, v.top + r * sq + sq / 2, sq, s);
        }

    // target dots
    for (int i = 0; i < n_targets; i++) {
        int x = v.left + targets[i].c * sq + sq / 2;
        int y = v.top + targets[i].r * sq + sq / 2;
        gfx_circle(x, y, sq * 0.14f, TARGET_DOT);
    }
}

bool board_view_hit(BoardView v, int mx, int my, int* r, int* c) {
    if (v.sq <= 0 || mx < v.left || my < v.top) return false;
    int col = (mx - v.left) / v.sq, row = (my - v.top) / v.sq;
    if (row < 0 || row >= 8 || col < 0 || col >= 8) return false;
    *r = row; *c = col;
    return true;
}

const char* status_state_text(const Game* g) {
    if (g->phase == PHASE_HUMAN_WON)  return "You win";
    if (g->phase == PHASE_HUMAN_LOST) return "You lose";
    return (g->turn == g->human) ? "Your move" : "Thinking...";
}

// --------------------------------------------------------------------------
// Menu + verdict panels
// --------------------------------------------------------------------------
// Menu item rectangles captured by the last render_menu() (for touch hit-testing
// on portrait); written by draw_menu_panel, read by render_menu_hit_test.
static Rectangle s_menu_item_rects[8];
static int s_menu_item_count = 0;

// Draw the menu panel, centred title, and item list with selection markers.
// `capture` records each row's rectangle for touch hit-testing (portrait);
// landscape passes false (mouse and keyboard).
void draw_menu_panel(MenuLayout m, const char* title, const char* const* items,
                     int count, int selected, int gap_before, bool capture) {
    gfx_rect_rounded(m.px, m.py, m.panel_w, m.panel_h, m.radius, MENU_BG);
    gfx_rect_rounded_lines(m.px, m.py, m.panel_w, m.panel_h, m.radius, TEXT_DIM);
    gfx_text(title, m.cx - gfx_measure_text(title, m.title_size) / 2, m.title_y,
             m.title_size, TEXT_LIGHT);

    s_menu_item_count = capture ? ((count < 8) ? count : 8) : 0;
    int y = m.items_y;
    for (int i = 0; i < count; i++) {
        if (gap_before == i) y += m.line_h;
        const char* label = items[i];
        int lw = gfx_measure_text(label, m.item_fs);
        Color col = (i == selected) ? SEL_RING : TEXT_DIM;
        if (i == selected) {
            gfx_text(">", m.cx - lw / 2 - m.item_fs * 14 / 11, y, m.item_fs, SEL_RING);
            gfx_text("<", m.cx + lw / 2 + m.item_fs * 7 / 11, y, m.item_fs, SEL_RING);
        }
        gfx_text(label, m.cx - lw / 2, y, m.item_fs, col);
        if (capture && i < 8) {
            s_menu_item_rects[i] = (Rectangle){ (float)m.px, (float)(y - (m.line_h - m.item_fs) / 2),
                                                (float)m.panel_w, (float)m.line_h };
        }
        y += m.line_h;
    }
}

void draw_verdict_panel(const Game* g, int cx, int cy, int panel_w, int panel_h,
                        int title_fs, int sub_fs, const char* sub) {
    int px = cx - panel_w / 2, py = cy - panel_h / 2;
    gfx_rect(px, py, panel_w, panel_h, (Color){0, 0, 0, 170});
    gfx_rect_lines(px, py, panel_w, panel_h, TEXT_LIGHT);
    const char* line = (g->phase == PHASE_HUMAN_WON) ? "YOU WIN" : "YOU LOSE";
    gfx_text(line, cx - gfx_measure_text(line, title_fs) / 2,
             py + panel_h * 34 / 150, title_fs, SEL_RING);
    gfx_text(sub, cx - gfx_measure_text(sub, sub_fs) / 2,
             py + panel_h * 96 / 150, sub_fs, TEXT_DIM);
}

// --------------------------------------------------------------------------
// Lifecycle
// --------------------------------------------------------------------------
void render_init(void) {
#if defined(PLATFORM_IOS)
    // iOS: UIKit owns the window/surface and drives the loop (CADisplayLink); the
    // Metal layer is attached separately by the app shell. Nothing to do here.
#else
#if defined(PLATFORM_ANDROID)
    // Request immersive fullscreen so the app draws under the status bar / camera
    // cutout (paired with windowLayoutInDisplayCutoutMode=shortEdges in the theme)
    // — otherwise the surface is letterboxed below the status bar.
    SetConfigFlags(FLAG_FULLSCREEN_MODE | FLAG_MSAA_4X_HINT);
    // Request 0x0: raylib's Android backend then renders at the device's native
    // resolution. Any fixed size here gets aspect-letterboxed into the display
    // (with GetScreenWidth/Height reporting the request, not the device).
    InitWindow(0, 0, "opencheckers");
#elif defined(PLATFORM_WEB)
    // Let the GL canvas follow the browser viewport (the HTML shell sizes it);
    // GetScreenWidth/Height then track it so the layout re-fits on resize/rotate.
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(MIN_W, MIN_H, "opencheckers");
#else
    // Desktop native: a freely resizable window with a fixed-size board; the
    // minimum window is just big enough to play. MSAA keeps the round pieces clean.
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(MIN_W, MIN_H, "opencheckers");
    SetWindowMinSize(MIN_W, MIN_H);
#endif
    SetExitKey(KEY_NULL); // Escape is handled by the game, not the window
    SetTargetFPS(60);
    gfx_font_init();      // load the bundled UI font now that the GL context exists
#ifdef OC_LANDSCAPE
    render_landscape_init();
#endif
#endif // PLATFORM_IOS
}

void render_cleanup(void) {
#ifdef OC_LANDSCAPE
    render_landscape_cleanup();
#endif
#if !defined(PLATFORM_IOS)
    CloseWindow();
#endif
}

void render_toggle_fullscreen(void) {
#if defined(PLATFORM_ANDROID) || defined(PLATFORM_IOS)
    // Android / iOS apps are always fullscreen; nothing to toggle.
    (void)0;
}
#else
    if (IsWindowFullscreen()) {
        ToggleFullscreen();
        SetWindowSize(MIN_W, MIN_H);
    } else {
        int m = GetCurrentMonitor();
        SetWindowSize(GetMonitorWidth(m), GetMonitorHeight(m));
        ToggleFullscreen();
    }
}
#endif // PLATFORM_ANDROID / PLATFORM_IOS

// --- Public entry points ---------------------------------------------------
// Dispatch to the active renderer: compile-time on native builds that have only
// one, runtime on web, which has both.
#if defined(OC_RUNTIME_RENDERER)
  #define OC_DISPATCH(fn, ...) (s_portrait_mode ? fn##_portrait(__VA_ARGS__) : fn##_landscape(__VA_ARGS__))
#elif defined(OC_PORTRAIT)
  #define OC_DISPATCH(fn, ...) fn##_portrait(__VA_ARGS__)
#else
  #define OC_DISPATCH(fn, ...) fn##_landscape(__VA_ARGS__)
#endif

void render_frame(const Game* g, int sel_r, int sel_c, const Pt* targets, int n_targets) {
    OC_DISPATCH(render_frame, g, sel_r, sel_c, targets, n_targets);
}
void render_gameover(const Game* g, int sel_r, int sel_c) {
    OC_DISPATCH(render_gameover, g, sel_r, sel_c);
}
void render_menu(const char* title, const char* const* labels, int count,
                 int selected, int gap_before) {
    OC_DISPATCH(render_menu, title, labels, count, selected, gap_before);
}
bool render_board_at(int mx, int my, int* r, int* c) {
    return OC_DISPATCH(render_board_at, mx, my, r, c);
}

int render_menu_hit_test(Vector2 p) {
    for (int i = 0; i < s_menu_item_count; i++) {
        if (CheckCollisionPointRec(p, s_menu_item_rects[i])) return i;
    }
    return -1;
}

bool render_window_should_close(void) {
    return WindowShouldClose();
}

bool render_window_focused(void) {
    return IsWindowFocused();
}
