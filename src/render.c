#include "render.h"
#include "recorder.h"
#include <raylib.h>
#include <rlgl.h>
#include <stdio.h>
#include <string.h>

#define TITLEBAR_H 44
#define STATUS_H   28
#define MARGIN     24
#define SS         2     // recorder supersampling factor

// --------------------------------------------------------------------------
// Colors
// --------------------------------------------------------------------------
static const Color FELT        = { 12,  92,  52, 255};   // background felt
static const Color FELT_DARK   = { 10,  76,  44, 255};   // title / status bars
static const Color SQ_LIGHT    = {232, 212, 170, 255};   // light board square
static const Color SQ_DARK     = {120,  82,  54, 255};   // dark board square (playable)
static const Color BOARD_EDGE  = { 60,  40,  26, 255};
static const Color RED_PIECE   = {198,  52,  48, 255};
static const Color RED_HI      = {236, 110, 100, 255};
static const Color RED_LO      = {140,  30,  30, 255};
static const Color BLK_PIECE   = { 48,  48,  56, 255};
static const Color BLK_HI      = {110, 112, 124, 255};
static const Color BLK_LO      = { 20,  20,  26, 255};
static const Color CROWN_GOLD  = {235, 200,  60, 255};
static const Color SEL_RING    = {255, 235, 120, 255};
static const Color TARGET_DOT  = {255, 235, 120, 180};
static const Color LASTMOVE    = {255, 235, 120,  70};
static const Color MENU_BG     = { 16,  40,  28, 255};
static const Color TEXT_LIGHT  = {235, 235, 225, 255};
static const Color TEXT_DIM    = {170, 190, 175, 255};

static const char* DIFF_NAME[3] = { "Easy", "Medium", "Hard" };

// --------------------------------------------------------------------------
// Layout — board centered in the window, fixed pixel size.
// --------------------------------------------------------------------------
typedef struct { int left, top, view_h, view_w; } Layout;

static Layout layout_for(int view_w, int view_h) {
    Layout L;
    L.left = (view_w - BOARD_PX) / 2;
    if (L.left < MARGIN) L.left = MARGIN;
    int region = view_h - TITLEBAR_H - STATUS_H;
    L.top = TITLEBAR_H + (region - BOARD_PX) / 2;
    if (L.top < TITLEBAR_H + MARGIN / 2) L.top = TITLEBAR_H + MARGIN / 2;
    L.view_h = view_h;
    L.view_w = view_w;
    return L;
}

// --------------------------------------------------------------------------
// Pieces (vector art)
// --------------------------------------------------------------------------
static void draw_crown(int cx, int cy, int s, Color col) {
    // five points joined to a base bar — a compact crown glyph
    float h = s * 0.5f, w = s * 0.8f;
    float bot = cy + h * 0.45f, top = cy - h * 0.55f, mid = cy - h * 0.05f;
    DrawTriangle((Vector2){cx - w/2, bot}, (Vector2){cx - w/2, mid},
                 (Vector2){cx - w/4, top}, col);
    DrawTriangle((Vector2){cx,       bot}, (Vector2){cx - w/4, top},
                 (Vector2){cx + w/4, top}, col);
    DrawTriangle((Vector2){cx + w/2, mid}, (Vector2){cx + w/2, bot},
                 (Vector2){cx + w/4, top}, col);
    DrawTriangle((Vector2){cx - w/2, bot}, (Vector2){cx - w/4, top},
                 (Vector2){cx,       bot}, col);
    DrawTriangle((Vector2){cx,       bot}, (Vector2){cx + w/4, top},
                 (Vector2){cx + w/2, bot}, col);
    DrawRectangle(cx - (int)(w/2), (int)(bot - 2), (int)w, 4, col);
}

static void draw_piece(int cx, int cy, Square s) {
    float r = SQUARE * 0.38f;
    Color base = (square_color(s) == SIDE_RED) ? RED_PIECE : BLK_PIECE;
    Color hi   = (square_color(s) == SIDE_RED) ? RED_HI    : BLK_HI;
    Color lo   = (square_color(s) == SIDE_RED) ? RED_LO    : BLK_LO;

    DrawCircle(cx, cy + 2, r, (Color){0, 0, 0, 60});     // soft shadow
    DrawCircle(cx, cy, r, lo);                            // rim
    DrawCircle(cx, cy, r * 0.86f, base);                 // face
    DrawCircleLines(cx, cy, r * 0.6f, hi);               // ridge
    DrawCircleLines(cx, cy, r * 0.6f + 1, hi);
    if (square_is_king(s)) draw_crown(cx, cy, (int)(r * 1.1f), CROWN_GOLD);
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
    DrawRectangle(0, 0, view_w, TITLEBAR_H, FELT_DARK);
    DrawLine(0, TITLEBAR_H, view_w, TITLEBAR_H, BOARD_EDGE);
    const char* title = "OPENCHECKERS";
    int fs = 22, tw = MeasureText(title, fs);
    DrawText(title, view_w / 2 - tw / 2, (TITLEBAR_H - fs) / 2, fs, TEXT_LIGHT);
}

static void draw_status(const Game* g, int view_w, int view_h) {
    int y = view_h - STATUS_H + 4;
    DrawRectangle(0, view_h - STATUS_H, view_w, STATUS_H, FELT_DARK);
    char buf[64];

    const char* state;
    if (g->phase == PHASE_HUMAN_WON)  state = "You win";
    else if (g->phase == PHASE_HUMAN_LOST) state = "You lose";
    else state = (g->turn == g->human) ? "Your move" : "Thinking...";
    DrawText(state, MARGIN, y, 18, TEXT_LIGHT);

    snprintf(buf, sizeof buf, "%s", DIFF_NAME[g->difficulty]);
    int tw = MeasureText(buf, 18);
    DrawText(buf, view_w / 2 - tw / 2, y, 18, TEXT_DIM);

    snprintf(buf, sizeof buf, "Red %d   Black %d",
             game_piece_count(g, SIDE_RED), game_piece_count(g, SIDE_BLACK));
    tw = MeasureText(buf, 18);
    DrawText(buf, view_w - MARGIN - tw, y, 18, TEXT_LIGHT);
}

static void draw_board(void* vctx, int view_w, int view_h) {
    BoardCtx* ctx = (BoardCtx*)vctx;
    const Game* g = ctx->g;
    ClearBackground(FELT);
    Layout L = layout_for(view_w, view_h);

    draw_titlebar(view_w);

    DrawRectangleLines(L.left - 3, L.top - 3, BOARD_PX + 6, BOARD_PX + 6, BOARD_EDGE);

    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++) {
            int x = L.left + c * SQUARE, y = L.top + r * SQUARE;
            bool dark = ((r + c) & 1) == 1;
            DrawRectangle(x, y, SQUARE, SQUARE, dark ? SQ_DARK : SQ_LIGHT);
            if (g->has_last && ((r == g->last_from.r && c == g->last_from.c)
                             || (r == g->last_to.r   && c == g->last_to.c)))
                DrawRectangle(x, y, SQUARE, SQUARE, LASTMOVE);
        }

    // selected square ring
    if (ctx->sel_r >= 0) {
        int x = L.left + ctx->sel_c * SQUARE, y = L.top + ctx->sel_r * SQUARE;
        for (int t = 0; t < 3; t++)
            DrawRectangleLines(x + t, y + t, SQUARE - 2 * t, SQUARE - 2 * t, SEL_RING);
    }

    // pieces
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++) {
            Square s = g->board[r][c];
            if (s == EMPTY) continue;
            draw_piece(L.left + c * SQUARE + SQUARE / 2,
                       L.top + r * SQUARE + SQUARE / 2, s);
        }

    // target dots
    for (int i = 0; i < ctx->n_targets; i++) {
        int x = L.left + ctx->targets[i].c * SQUARE + SQUARE / 2;
        int y = L.top + ctx->targets[i].r * SQUARE + SQUARE / 2;
        DrawCircle(x, y, SQUARE * 0.14f, TARGET_DOT);
    }

    draw_status(g, view_w, view_h);
}

// --------------------------------------------------------------------------
// Presentation: window + (when recording) an SSAA-supersampled fixed canvas.
// --------------------------------------------------------------------------
typedef void (*SceneFn)(void* ctx, int w, int h);

static RenderTexture2D rec_canvas, rec_super;
static bool rec_ready = false;

static void emit(SceneFn fn, void* ctx) {
    BeginDrawing();
    fn(ctx, GetScreenWidth(), GetScreenHeight());
    EndDrawing();

    if (recorder_active() && rec_ready) {
        BeginTextureMode(rec_super);
        rlPushMatrix();
        rlScalef((float)SS, (float)SS, 1.0f);
        fn(ctx, MIN_W, MIN_H);
        rlPopMatrix();
        EndTextureMode();

        BeginTextureMode(rec_canvas);
        Rectangle src = {0, 0, (float)(SS * MIN_W), -(float)(SS * MIN_H)};
        Rectangle dst = {0, 0, (float)MIN_W, (float)MIN_H};
        DrawTexturePro(rec_super.texture, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
        EndTextureMode();

        recorder_capture(&rec_canvas);
    }
}

// --------------------------------------------------------------------------
// Lifecycle
// --------------------------------------------------------------------------
void render_init(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(MIN_W, MIN_H, "opencheckers");
    SetWindowMinSize(MIN_W, MIN_H);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);
    rec_canvas = LoadRenderTexture(MIN_W, MIN_H);
    rec_super  = LoadRenderTexture(SS * MIN_W, SS * MIN_H);
    SetTextureFilter(rec_super.texture, TEXTURE_FILTER_BILINEAR);
    rec_ready = true;
}

void render_cleanup(void) {
    if (rec_ready) {
        UnloadRenderTexture(rec_canvas);
        UnloadRenderTexture(rec_super);
    }
    CloseWindow();
}

bool render_window_should_close(void) { return WindowShouldClose(); }

void render_toggle_fullscreen(void) {
    if (IsWindowFullscreen()) {
        ToggleFullscreen();
        SetWindowSize(MIN_W, MIN_H);
    } else {
        int m = GetCurrentMonitor();
        SetWindowSize(GetMonitorWidth(m), GetMonitorHeight(m));
        ToggleFullscreen();
    }
}

// --------------------------------------------------------------------------
// Public scenes
// --------------------------------------------------------------------------
void render_frame(const Game* g, int sel_r, int sel_c,
                  const Pt* targets, int n_targets) {
    BoardCtx ctx = { g, sel_r, sel_c, targets, n_targets };
    emit(draw_board, &ctx);
}

typedef struct {
    const char* title; const char** labels;
    int count; int selected; int gap_before;
} MenuCtx;

static void draw_menu(void* vctx, int view_w, int view_h) {
    MenuCtx* m = (MenuCtx*)vctx;
    ClearBackground(FELT);

    int cx = view_w / 2, line_h = 32, title_size = 46;
    int extra = (m->gap_before >= 0) ? 1 : 0;
    int panel_w = 420;
    int panel_h = title_size + 40 + (m->count + extra) * line_h + 60;
    int px = cx - panel_w / 2, py = (view_h - panel_h) / 2;

    DrawRectangleRounded((Rectangle){px, py, panel_w, panel_h}, 0.05f, 8, MENU_BG);
    DrawRectangleRoundedLines((Rectangle){px, py, panel_w, panel_h}, 0.05f, 8, TEXT_DIM);
    DrawText(m->title, cx - MeasureText(m->title, title_size) / 2,
             py + 26, title_size, TEXT_LIGHT);

    int y = py + 26 + title_size + 26;
    for (int i = 0; i < m->count; i++) {
        if (m->gap_before == i) y += line_h;
        int size = 22, lw = MeasureText(m->labels[i], size);
        Color col = (i == m->selected) ? SEL_RING : TEXT_DIM;
        if (i == m->selected) {
            DrawText(">", cx - lw / 2 - 28, y, size, SEL_RING);
            DrawText("<", cx + lw / 2 + 14, y, size, SEL_RING);
        }
        DrawText(m->labels[i], cx - lw / 2, y, size, col);
        y += line_h;
    }
}

void render_menu(const char* title, const char** labels, int count,
                 int selected, int gap_before) {
    MenuCtx ctx = { title, labels, count, selected, gap_before };
    emit(draw_menu, &ctx);
}

static void draw_gameover(void* vctx, int view_w, int view_h) {
    BoardCtx* ctx = (BoardCtx*)vctx;
    draw_board(vctx, view_w, view_h);
    int pw = 420, ph = 150, px = (view_w - pw) / 2, py = (view_h - ph) / 2;
    DrawRectangle(px, py, pw, ph, (Color){0, 0, 0, 170});
    DrawRectangleLines(px, py, pw, ph, TEXT_LIGHT);
    const char* line = (ctx->g->phase == PHASE_HUMAN_WON) ? "YOU WIN" : "YOU LOSE";
    DrawText(line, view_w / 2 - MeasureText(line, 44) / 2, py + 34, 44, SEL_RING);
    const char* sub = "Press any key";
    DrawText(sub, view_w / 2 - MeasureText(sub, 18) / 2, py + 96, 18, TEXT_DIM);
}

void render_gameover(const Game* g, int sel_r, int sel_c) {
    BoardCtx ctx = { g, sel_r, sel_c, NULL, 0 };
    emit(draw_gameover, &ctx);
}

// --------------------------------------------------------------------------
// Hit test
// --------------------------------------------------------------------------
bool render_board_at(int mx, int my, int* r, int* c) {
    Layout L = layout_for(GetScreenWidth(), GetScreenHeight());
    if (mx < L.left || my < L.top) return false;
    int col = (mx - L.left) / SQUARE, row = (my - L.top) / SQUARE;
    if (row < 0 || row >= 8 || col < 0 || col >= 8) return false;
    *r = row; *c = col;
    return true;
}
