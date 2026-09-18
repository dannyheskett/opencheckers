// Landscape (desktop) renderer: the fixed-size board centred in a freely
// resizable window, with a title bar above and a status bar below. Frames go
// out through present() (present.c), which also feeds the recorder. Compiles to
// an empty object off OC_LANDSCAPE (i.e. on Android / iOS).
#include "render_internal.h"

#ifdef OC_LANDSCAPE

#include "present.h"
#include <raylib.h>
#include <stdio.h>

#define TITLEBAR_H 44
#define STATUS_H   28
#define MARGIN     24

// --------------------------------------------------------------------------
// Layout — board centered in the window, fixed pixel size. A view too small to
// hold it (the 640x480 window minimum, a small browser window) shrinks the
// squares to fit instead of running the board off the edge.
// --------------------------------------------------------------------------
static BoardView layout_for(int view_w, int view_h) {
    BoardView v = { .sq = SQUARE };
    int fit_h = (view_h - TITLEBAR_H - STATUS_H - MARGIN) / 8;
    int fit_w = (view_w - 2 * MARGIN) / 8;
    if (fit_h < v.sq) v.sq = fit_h;
    if (fit_w < v.sq) v.sq = fit_w;
    if (v.sq < 8) v.sq = 8;
    int board_px = 8 * v.sq;
    v.left = (view_w - board_px) / 2;
    if (v.left < MARGIN) v.left = MARGIN;
    int region = view_h - TITLEBAR_H - STATUS_H;
    v.top = TITLEBAR_H + (region - board_px) / 2;
    if (v.top < TITLEBAR_H + MARGIN / 2) v.top = TITLEBAR_H + MARGIN / 2;
    return v;
}

// --------------------------------------------------------------------------
// Board scene
// --------------------------------------------------------------------------
typedef struct {
    const Game* g;
    int sel_r, sel_c;
    const Pt* targets;
    int n_targets;
} BoardCtx;

static void draw_titlebar(int view_w) {
    gfx_rect(0, 0, view_w, TITLEBAR_H, FELT_DARK);
    gfx_line(0, TITLEBAR_H, view_w, TITLEBAR_H, BOARD_EDGE);
    const char* title = "OPENCHECKERS";
    int fs = 22, tw = gfx_measure_text(title, fs);
    gfx_text(title, view_w / 2 - tw / 2, (TITLEBAR_H - fs) / 2, fs, TEXT_LIGHT);
}

static void draw_status(const Game* g, int view_w, int view_h) {
    int y = view_h - STATUS_H + 4;
    gfx_rect(0, view_h - STATUS_H, view_w, STATUS_H, FELT_DARK);
    char buf[64];

    gfx_text(status_state_text(g), MARGIN, y, 18, TEXT_LIGHT);

    snprintf(buf, sizeof buf, "%s", DIFF_NAME[g->difficulty]);
    int tw = gfx_measure_text(buf, 18);
    gfx_text(buf, view_w / 2 - tw / 2, y, 18, TEXT_DIM);

    snprintf(buf, sizeof buf, "Red %d   Black %d",
             game_piece_count(g, SIDE_RED), game_piece_count(g, SIDE_BLACK));
    tw = gfx_measure_text(buf, 18);
    gfx_text(buf, view_w - MARGIN - tw, y, 18, TEXT_LIGHT);
}

static void draw_board_scene(void* vctx, int view_w, int view_h) {
    BoardCtx* ctx = (BoardCtx*)vctx;
    gfx_clear(FELT);
    draw_titlebar(view_w);
    draw_board(ctx->g, layout_for(view_w, view_h), ctx->sel_r, ctx->sel_c,
               ctx->targets, ctx->n_targets);
    draw_status(ctx->g, view_w, view_h);
}

// --------------------------------------------------------------------------
// Public scenes
// --------------------------------------------------------------------------
void render_frame_landscape(const Game* g, int sel_r, int sel_c,
                            const Pt* targets, int n_targets) {
    BoardCtx ctx = { g, sel_r, sel_c, targets, n_targets };
    present(draw_board_scene, &ctx);
}

static void draw_gameover_scene(void* vctx, int view_w, int view_h) {
    draw_board_scene(vctx, view_w, view_h);
    BoardCtx* ctx = (BoardCtx*)vctx;
    draw_verdict_panel(ctx->g, view_w, view_h, "Press any key");
}

void render_gameover_landscape(const Game* g, int sel_r, int sel_c) {
    BoardCtx ctx = { g, sel_r, sel_c, NULL, 0 };
    present(draw_gameover_scene, &ctx);
}

// --------------------------------------------------------------------------
// Hit test
// --------------------------------------------------------------------------
bool render_board_at_landscape(int mx, int my, int* r, int* c) {
    return board_view_hit(layout_for(GetScreenWidth(), GetScreenHeight()), mx, my, r, c);
}

#endif // OC_LANDSCAPE
