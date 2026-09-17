// Landscape (desktop) renderer: the fixed-size board centred in a freely
// resizable window, with a title bar above and a status bar below. When
// recording, each scene is also re-rendered at the fixed MIN_W x MIN_H size
// (supersampled) for the video. Compiles to an empty object off OC_LANDSCAPE
// (i.e. on Android / iOS).
#include "render_internal.h"

#ifdef OC_LANDSCAPE

#include "recorder.h"
#include <raylib.h>
#include <rlgl.h>
#include <stdio.h>

#define TITLEBAR_H 44
#define STATUS_H   28
#define MARGIN     24
#define SS         2     // recorder supersampling factor

// --------------------------------------------------------------------------
// Layout — board centered in the window, fixed pixel size.
// --------------------------------------------------------------------------
static BoardView layout_for(int view_w, int view_h) {
    BoardView v = { .sq = SQUARE };
    v.left = (view_w - BOARD_PX) / 2;
    if (v.left < MARGIN) v.left = MARGIN;
    int region = view_h - TITLEBAR_H - STATUS_H;
    v.top = TITLEBAR_H + (region - BOARD_PX) / 2;
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
// Presentation: window + (when recording) an SSAA-supersampled fixed canvas.
// --------------------------------------------------------------------------
typedef void (*SceneFn)(void* ctx, int w, int h);

static RenderTexture2D rec_canvas, rec_super;
static bool rec_ready = false;

static void emit(SceneFn fn, void* ctx) {
    gfx_begin_frame();
    fn(ctx, GetScreenWidth(), GetScreenHeight());
    gfx_end_frame();

    if (recorder_active() && rec_ready) {
        BeginTextureMode(rec_super);
        // Blend colour normally but keep the target opaque. With the default
        // blend, every translucent draw (last-move tint, target dots, the
        // verdict panel) also lowered the texture's alpha, and the downsample
        // below then composited those pixels over nothing: the video showed
        // them faded.
        rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA,
                                  RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
        BeginBlendMode(BLEND_CUSTOM_SEPARATE);
        rlPushMatrix();
        rlScalef((float)SS, (float)SS, 1.0f);
        fn(ctx, MIN_W, MIN_H);
        rlPopMatrix();
        EndBlendMode();
        EndTextureMode();

        BeginTextureMode(rec_canvas);
        Rectangle src = {0, 0, (float)(SS * MIN_W), -(float)(SS * MIN_H)};
        Rectangle dst = {0, 0, (float)MIN_W, (float)MIN_H};
        DrawTexturePro(rec_super.texture, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
        EndTextureMode();

        recorder_capture(&rec_canvas);
    }
}

// The recorder is desktop-only (stubbed out on web), so the capture targets are
// only allocated where it can actually run.
void render_landscape_init(void) {
#ifndef OC_TOUCH
    rec_canvas = LoadRenderTexture(MIN_W, MIN_H);
    rec_super  = LoadRenderTexture(SS * MIN_W, SS * MIN_H);
    SetTextureFilter(rec_super.texture, TEXTURE_FILTER_BILINEAR);
    rec_ready = true;
#endif
}

void render_landscape_cleanup(void) {
    if (rec_ready) {
        UnloadRenderTexture(rec_canvas);
        UnloadRenderTexture(rec_super);
        rec_ready = false;
    }
}

// --------------------------------------------------------------------------
// Public scenes
// --------------------------------------------------------------------------
void render_frame_landscape(const Game* g, int sel_r, int sel_c,
                            const Pt* targets, int n_targets) {
    BoardCtx ctx = { g, sel_r, sel_c, targets, n_targets };
    emit(draw_board_scene, &ctx);
}

typedef struct {
    const char* title; const char* const* labels;
    int count; int selected; int gap_before;
} MenuCtx;

static void draw_menu_scene(void* vctx, int view_w, int view_h) {
    MenuCtx* c = (MenuCtx*)vctx;
    gfx_clear(FELT);

    int line_h = 32, title_size = 46;
    int extra = (c->gap_before >= 0) ? 1 : 0;
    int panel_w = 420;
    int panel_h = title_size + 40 + (c->count + extra) * line_h + 60;
    int px = view_w / 2 - panel_w / 2, py = (view_h - panel_h) / 2;
    int short_side = (panel_w < panel_h) ? panel_w : panel_h;
    MenuLayout m = { .cx = view_w / 2, .px = px, .py = py, .panel_w = panel_w,
                     .panel_h = panel_h, .radius = short_side * 5 / 200,
                     .title_size = title_size, .title_y = py + 26,
                     .items_y = py + 26 + title_size + 26,
                     .line_h = line_h, .item_fs = 22 };
    draw_menu_panel(m, c->title, c->labels, c->count, c->selected, c->gap_before, false);
}

void render_menu_landscape(const char* title, const char* const* labels, int count,
                           int selected, int gap_before) {
    MenuCtx ctx = { title, labels, count, selected, gap_before };
    emit(draw_menu_scene, &ctx);
}

static void draw_gameover_scene(void* vctx, int view_w, int view_h) {
    draw_board_scene(vctx, view_w, view_h);
    BoardCtx* ctx = (BoardCtx*)vctx;
#ifdef OC_TOUCH
    const char* sub = "Click to continue";   // web in a desktop browser: mouse
#else
    const char* sub = "Press any key";
#endif
    draw_verdict_panel(ctx->g, view_w / 2, view_h / 2, 420, 150, 44, 18, sub);
}

void render_gameover_landscape(const Game* g, int sel_r, int sel_c) {
    BoardCtx ctx = { g, sel_r, sel_c, NULL, 0 };
    emit(draw_gameover_scene, &ctx);
}

// --------------------------------------------------------------------------
// Hit test
// --------------------------------------------------------------------------
bool render_board_at_landscape(int mx, int my, int* r, int* c) {
    return board_view_hit(layout_for(GetScreenWidth(), GetScreenHeight()), mx, my, r, c);
}

#endif // OC_LANDSCAPE
