#ifndef OPENCHECKERS_RENDER_H
#define OPENCHECKERS_RENDER_H

#include "game.h"
#include <stdbool.h>

// Fixed board metrics — the board NEVER scales, regardless of window size.
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

// Scenes -------------------------------------------------------------------
// Draw the board. `sel_r/sel_c` is the selected piece (-1 if none); `targets`
// are its highlighted legal destinations.
void render_frame(const Game* g, int sel_r, int sel_c,
                  const Pt* targets, int n_targets);
void render_menu(const char* title, const char** labels, int count,
                 int selected, int gap_before);
void render_gameover(const Game* g, int sel_r, int sel_c);

// Hit test (live window): pixel -> board square. Returns true if on the board.
bool render_board_at(int mx, int my, int* r, int* c);

#endif
