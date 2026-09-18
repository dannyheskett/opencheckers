#ifndef OPENCHECKERS_RENDER_H
#define OPENCHECKERS_RENDER_H

#include "game.h"
#include "platform.h"
#include "oc_types.h"
#include <stdbool.h>

// Landscape (desktop) metrics. The landscape board is this fixed size in any
// view that holds it, and shrinks to fit a smaller one; the portrait renderer
// sizes its board from the live screen instead and does not use these.
#define SQUARE   72
#define BOARD_PX (8 * SQUARE)        // 576

// Window setup and teardown (window.c) plus the recorder's capture canvas.
void render_init(void);
void render_cleanup(void);

// Scenes -------------------------------------------------------------------
// Draw the board. `sel_r/sel_c` is the selected piece (-1 if none); `targets`
// are its highlighted legal destinations.
void render_frame(const Game* g, int sel_r, int sel_c,
                  const Pt* targets, int n_targets);
// The family menu (menu.c) on the felt. gap_before, if >= 0, inserts a blank
// line before that item index. Hit-test its rows with menu_hit_test().
void render_menu(const char* title, const char* const* labels, int count,
                 int selected, int gap_before);
void render_gameover(const Game* g, int sel_r, int sel_c);

// Hit test (live window): pixel -> board square, using the active renderer's
// layout. Returns true if on the board.
bool render_board_at(int mx, int my, int* r, int* c);

// Active renderer selection. Native builds have exactly one renderer, so
// render_use_portrait() is a compile-time constant there (true on Android and
// iOS, false on desktop). The web build compiles both and picks at runtime:
// render_set_portrait(true) = portrait touch layout, false = desktop landscape.
void render_set_portrait(bool portrait);
bool render_use_portrait(void);

// Board square size in pixels for the current portrait layout. The touch input
// layer scales its tap-movement tolerance from it.
int render_portrait_square(void);

#endif
