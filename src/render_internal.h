#ifndef OPENCHECKERS_RENDER_INTERNAL_H
#define OPENCHECKERS_RENDER_INTERNAL_H

// Private interface shared between the renderer translation units:
//   render.c           — colours, shared draw helpers, lifecycle, dispatch
//   render_portrait.c  — the portrait (touch) renderer    (body under OC_PORTRAIT)
//   render_landscape.c — the landscape (desktop) renderer (body under OC_LANDSCAPE)
// render_portrait/landscape.c compile to empty objects on platforms that don't
// use them, so all three can sit in the build's source list unconditionally.

#include "render.h"
#include "gfx.h"
#include "menu.h"

// Palette (defined in render.c), shared by both renderers.
extern const Color FELT;        // background felt
extern const Color FELT_DARK;   // title / status bars
extern const Color BOARD_EDGE;
extern const Color SEL_RING;
extern const Color MENU_BG;
extern const Color TEXT_LIGHT;
extern const Color TEXT_DIM;

extern const char* const DIFF_NAME[3];

// Where the board sits on screen and how large a square is.
typedef struct { int left, top, sq; } BoardView;

// Squares, last-move tint, selection ring, pieces, and target dots, all scaled
// from v.sq (the landscape renderer passes SQUARE).
void draw_board(const Game* g, BoardView v, int sel_r, int sel_c,
                const Pt* targets, int n_targets);

// Map a pixel to a board square for a given view. Returns true if on the board.
bool board_view_hit(BoardView v, int mx, int my, int* r, int* c);

// "Your move" / "Thinking..." / "You win" / "You lose" for the status bar.
const char* status_state_text(const Game* g);

// The family menu (menu.c) in this game's colours.
MenuTheme menu_theme(void);

// The game-over notice (menu_draw_notice) over the board: the verdict and a
// hint line.
void draw_verdict_panel(const Game* g, int view_w, int view_h, const char* sub);

// Per-renderer entry points (defined in render_portrait/landscape.c), called by
// the OC_DISPATCH macro in render.c.
#ifdef OC_PORTRAIT
void render_frame_portrait(const Game* g, int sel_r, int sel_c,
                           const Pt* targets, int n_targets);
void render_gameover_portrait(const Game* g, int sel_r, int sel_c);
bool render_board_at_portrait(int mx, int my, int* r, int* c);
#endif
#ifdef OC_LANDSCAPE
void render_frame_landscape(const Game* g, int sel_r, int sel_c,
                            const Pt* targets, int n_targets);
void render_gameover_landscape(const Game* g, int sel_r, int sel_c);
bool render_board_at_landscape(int mx, int my, int* r, int* c);
#endif

#endif // OPENCHECKERS_RENDER_INTERNAL_H
