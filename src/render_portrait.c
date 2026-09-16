// Portrait (touch) renderer: adaptive layout drawn straight to the screen at the
// device's real resolution — a thin OPENCHECKERS title bar, a status band, and
// the board sized to the screen width, all controlled by taps (no on-screen
// buttons). Compiles to an empty object off OC_PORTRAIT.
#include "render_internal.h"
#include "safe_area.h"
#include <stdio.h>

#ifdef OC_PORTRAIT

// Thin full-width title bar across the very top, matching the landscape
// renderer's proportions.
static int title_fs(int h)    { int fs = h / 45; return (fs < 10) ? 10 : fs; }
static int title_bar_h(int h) { int fs = title_fs(h); return fs + fs / 2; }

// Effective top-bar height: the thin title bar, grown to clear the display
// cutout (front camera) when the surface draws under it, so neither the
// wordmark nor the status band below ever sits beneath the camera.
static int top_bar_h(int h) {
    int top, cl, cr;
    safe_area_get(&top, &cl, &cr);
    int tb = title_bar_h(h);
    return (top > tb) ? top : tb;
}

// Global margin: the breathing room applied on every edge (board left / right /
// bottom, below the title bar) and as the gap between the status band and the
// board. Scales with the short screen dimension so it stays proportional across
// resolutions and aspect ratios.
static int outer_margin(void) {
    int w = GetScreenWidth(), h = GetScreenHeight();
    int m = ((w < h) ? w : h) / 28;
    return (m < 6) ? 6 : m;
}

// --- Shared portrait layout -------------------------------------------------
// Title bar, then one status band above the board: whose move on the left,
// difficulty in the middle, piece counts on the right. There is no on-screen
// menu key — a two-finger tap opens the menu (see input.c); draw_menu_hint
// names that gesture under the board early in a game.
typedef struct {
    int fs, band_h, band_y;      // uniform status font size; band height and top y
    BoardView board;
} BandLayout;

static void band_layout(BandLayout* L) {
    int w = GetScreenWidth(), h = GetScreenHeight();
    int m       = outer_margin();
    int tb_h    = top_bar_h(h);
    int avail_h = h - tb_h - 2 * m;   // margin below the bar and at the bottom

    // Uniform font: start at the approved size, shrink only if the three status
    // texts can't fit across the board width at their widest. The difficulty is
    // centred, so each side needs room for the wider of the two edge texts.
    int fs = h / 38, sq;
    for (;; fs--) {
        int band_h = fs + fs / 3;
        int sw = (w - 2 * m) / 8;
        int sh = (avail_h - band_h - m) / 8;
        sq = (sw < sh) ? sw : sh;
        if (sq < 1) sq = 1;
        int left  = gfx_measure_text("Thinking...", fs);
        int right = gfx_measure_text("Red 12  Black 12", fs);
        int side  = (left > right) ? left : right;
        int cols  = 2 * side + gfx_measure_text("Medium", fs);
        if (fs <= 8 || cols + 2 * fs <= 8 * sq) break;
    }
    L->fs     = fs;
    L->band_h = fs + fs / 3;
    L->board.sq   = sq;
    L->board.left = (w - 8 * sq) / 2;
    // The band -> board gap is the global margin; leftover height centers the
    // whole block between the title bar and the bottom margin.
    int block = L->band_h + m + 8 * sq;
    L->band_y    = tb_h + m + (avail_h - block) / 2;
    L->board.top = L->band_y + L->band_h + m;
}

int render_portrait_square(void) {
    BandLayout L;
    band_layout(&L);
    return L.board.sq;
}

bool render_board_at_portrait(int mx, int my, int* r, int* c) {
    BandLayout L;
    band_layout(&L);
    return board_view_hit(L.board, mx, my, r, c);
}

// Full-width title bar at the very top. When a display cutout (front camera)
// sits in the bar, the "OPENCHECKERS" wordmark is laid out around it:
//   - cutout absent            -> centered full word (the desktop/web look).
//   - room both sides          -> "OPEN" left of the camera, "CHECKERS" right.
//   - lopsided (corner camera) -> whole word on the roomier side, if it fits.
//   - wide notch, nothing fits -> bar only, no wordmark.
static void draw_title_bar(void) {
    int w = GetScreenWidth(), h = GetScreenHeight();
    int fs = title_fs(h), tb_h = top_bar_h(h);
    int ty = (tb_h - fs) / 2;   // wordmark vertically centered in the bar
    gfx_rect(0, 0, w, tb_h, FELT_DARK);
    gfx_line(0, tb_h, w, tb_h, BOARD_EDGE);

    int top, cl, cr;
    safe_area_get(&top, &cl, &cr);
    int full = gfx_measure_text("OPENCHECKERS", fs);

    // No horizontal extent reported. With no top inset either, there is no
    // cutout at all -> original centered wordmark. If there IS a top inset we
    // just couldn't localize, leave the bar bare rather than risk centering the
    // word under the camera.
    if (cr <= cl) {
        if (top <= 0)
            gfx_text("OPENCHECKERS", (w - full) / 2, ty, fs, TEXT_LIGHT);
        return;
    }

    int pad        = fs / 2;        // clearance kept between text and the camera
    int left_room  = cl;            // px available left of the cutout
    int right_room = w - cr;        // px available right of the cutout
    int open_w     = gfx_measure_text("OPEN", fs);
    int checkers_w = gfx_measure_text("CHECKERS", fs);

    if (left_room >= open_w + pad && right_room >= checkers_w + pad) {
        // Split the word around the camera.
        gfx_text("OPEN",     cl - pad - open_w, ty, fs, TEXT_LIGHT);
        gfx_text("CHECKERS", cr + pad,          ty, fs, TEXT_LIGHT);
    } else if (right_room >= full + pad || left_room >= full + pad) {
        // Corner camera: keep the word whole on whichever side has more room.
        if (right_room >= left_room)
            gfx_text("OPENCHECKERS", cr + pad, ty, fs, TEXT_LIGHT);
        else
            gfx_text("OPENCHECKERS", cl - pad - full, ty, fs, TEXT_LIGHT);
    }
    // else: wide notch — leave the bar bare.
}

static void draw_game_portrait(const Game* g, int sel_r, int sel_c,
                               const Pt* targets, int n_targets) {
    gfx_clear(FELT);
    draw_title_bar();
    BandLayout L;
    band_layout(&L);
    int fs = L.fs, x0 = L.board.left, x1 = L.board.left + 8 * L.board.sq;
    char buf[64];

    gfx_text(status_state_text(g), x0, L.band_y, fs, TEXT_LIGHT);

    snprintf(buf, sizeof buf, "%s", DIFF_NAME[g->difficulty]);
    gfx_text(buf, (x0 + x1) / 2 - gfx_measure_text(buf, fs) / 2, L.band_y, fs, TEXT_DIM);

    snprintf(buf, sizeof buf, "Red %d  Black %d",
             game_piece_count(g, SIDE_RED), game_piece_count(g, SIDE_BLACK));
    gfx_text(buf, x1 - gfx_measure_text(buf, fs), L.band_y, fs, TEXT_LIGHT);

    draw_board(g, L.board, sel_r, sel_c, targets, n_targets);
}

// Discoverability aid for the menu gesture. Opening the menu is a two-finger
// tap (input.c) with no on-screen key, and iOS has no Back button to fall back
// on, so a player has no way to learn the gesture exists. Name it under the
// board until the first capture, then get out of the way. Keyed on the board
// rather than a timer, so it follows how far the game has actually got.
static void draw_menu_hint(const Game* g) {
    if (game_piece_count(g, SIDE_RED) + game_piece_count(g, SIDE_BLACK) < 24) return;

    BandLayout L;
    band_layout(&L);
    int w = GetScreenWidth(), h = GetScreenHeight();
    int board_bottom = L.board.top + 8 * L.board.sq;
    int room = h - board_bottom;   // bottom margin band_layout left below the board
    int fs = L.fs * 2 / 3;
    if (fs > room - 4) fs = room - 4;   // stay inside the margin, never over the board
    if (fs < 8) return;                // no room on this layout: skip it entirely

    const char* msg = "Two-finger tap for menu";
    int tw = gfx_measure_text(msg, fs);
    if (tw > w) return;
    gfx_text(msg, (w - tw) / 2, board_bottom + (room - fs) / 2, fs, TEXT_DIM);
}

void render_frame_portrait(const Game* g, int sel_r, int sel_c,
                           const Pt* targets, int n_targets) {
    gfx_begin_frame();
    draw_game_portrait(g, sel_r, sel_c, targets, n_targets);
    draw_menu_hint(g);
    gfx_end_frame();
}

void render_gameover_portrait(const Game* g, int sel_r, int sel_c) {
    int w = GetScreenWidth(), h = GetScreenHeight();
    int base = (w < h) ? w : h;   // keep the dialog compact even in a wide window
    int pw = base * 82 / 100;
    int ph = base * 30 / 100;
    gfx_begin_frame();
    draw_game_portrait(g, sel_r, sel_c, NULL, 0);
    draw_verdict_panel(g, w / 2, h / 2, pw, ph, ph * 29 / 100, ph * 12 / 100,
                       "Tap to continue");
    gfx_end_frame();
}

void render_menu_portrait(const char* title, const char* const* labels, int count,
                          int selected, int gap_before) {
    int w = GetScreenWidth(), h = GetScreenHeight();
    int line_h = h / 20, item_fs = h / 28;
    int extra = (gap_before >= 0) ? 1 : 0;
    int base = (w < h) ? w : h;              // keep the panel compact in a wide window
    int panel_w = base * 82 / 100;

    // Shrink the title if it would overrun the panel (narrow phones).
    int title_size = h / 16;
    while (title_size > 12 && gfx_measure_text(title, title_size) > panel_w - line_h) title_size -= 2;

    int panel_h = title_size + line_h + (count + extra) * line_h + line_h * 2;
    int px = w / 2 - panel_w / 2, py = (h - panel_h) / 2;
    MenuLayout m = { .cx = w / 2, .px = px, .py = py, .panel_w = panel_w, .panel_h = panel_h,
                     .radius = base / 40,
                     .title_size = title_size, .title_y = py + line_h,
                     .items_y = py + line_h + title_size + line_h,
                     .line_h = line_h, .item_fs = item_fs };

    gfx_begin_frame();
    gfx_clear(FELT);
    draw_menu_panel(m, title, labels, count, selected, gap_before, true);
    gfx_end_frame();
}

#endif // OC_PORTRAIT
