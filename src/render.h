#ifndef OPENCHECKERS_RENDER_H
#define OPENCHECKERS_RENDER_H

#include "game.h"
#include "platform.h"
#include "oc_types.h"
#include <stdbool.h>

// Landscape (desktop) metrics. The landscape board is a fixed size and NEVER
// scales, regardless of window size; the portrait renderer sizes its board from
// the live screen instead and does not use these.
#define SQUARE   72
#define BOARD_PX (8 * SQUARE)        // 576

// Minimum window size, just big enough for the board plus bars and margins.
// Both are multiples of 16 so the recorder can capture them.
#define MIN_W    624                 // 39 * 16
#define MIN_H    704                 // 44 * 16

void render_init(void);
void render_cleanup(void);
bool render_window_should_close(void);
void render_toggle_fullscreen(void);
// True while the app window holds input focus. Used to leave play for the menu
// when the app is sent to the background (touch platforms).
bool render_window_focused(void);

// Scenes -------------------------------------------------------------------
// Draw the board. `sel_r/sel_c` is the selected piece (-1 if none); `targets`
// are its highlighted legal destinations.
void render_frame(const Game* g, int sel_r, int sel_c,
                  const Pt* targets, int n_targets);
// Floating menu: title plus a list of items, one highlighted. gap_before, if
// >= 0, inserts a blank line before that item index.
void render_menu(const char* title, const char* const* labels, int count,
                 int selected, int gap_before);
void render_gameover(const Game* g, int sel_r, int sel_c);

// Hit test (live window): pixel -> board square, using the active renderer's
// layout. Returns true if on the board.
bool render_board_at(int mx, int my, int* r, int* c);

// Return the menu item index at screen point `p`, or -1 if none. Uses the item
// rectangles captured by the last render_menu() call (touch menus).
int render_menu_hit_test(Vector2 p);

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
