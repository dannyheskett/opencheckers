#ifndef OPENCHECKERS_GAME_H
#define OPENCHECKERS_GAME_H

#include <stdint.h>
#include <stdbool.h>

// --------------------------------------------------------------------------
// American / English draughts on an 8x8 board.
//   - 12 pieces each, on the dark squares ((r + c) is odd).
//   - Red sits at the bottom (rows 5-7) and moves up (decreasing row).
//   - Black sits at the top (rows 0-2) and moves down.
//   - Men move/capture diagonally forward only; kings one square any diagonal.
//   - Captures are mandatory; a multi-jump must continue with the same piece.
//   - Reaching the far row crowns the piece and ends the turn.
// --------------------------------------------------------------------------
typedef enum {
    EMPTY = 0,
    RED_MAN,
    RED_KING,
    BLACK_MAN,
    BLACK_KING,
} Square;

typedef enum { SIDE_RED = 0, SIDE_BLACK = 1 } Side;

typedef enum { DIFF_EASY = 0, DIFF_MEDIUM, DIFF_HARD } Difficulty;

typedef enum {
    PHASE_PLAYING = 0,
    PHASE_HUMAN_WON,
    PHASE_HUMAN_LOST,
} GamePhase;

typedef struct { int r, c; } Pt;

// A full move: the landing path (path[0] = origin) plus the captured squares.
// A plain step has path_len 2 and n_captured 0; a jump chain grows both.
typedef struct {
    Pt  path[10];
    int path_len;
    Pt  captured[9];
    int n_captured;
} Move;

// Per-frame event flags consumed by main.c to drive sound.
enum {
    EV_MOVE      = 1 << 0,
    EV_CAPTURE   = 1 << 1,
    EV_KING      = 1 << 2,
    EV_WIN       = 1 << 3,
    EV_LOSE      = 1 << 4,
    EV_ILLEGAL   = 1 << 5,
    EV_MENU_MOVE = 1 << 6,
    EV_MENU_SEL  = 1 << 7,
};

typedef struct {
    Square     board[8][8];
    Side      turn;        // side to move
    Side      human;       // colour the human controls
    Difficulty difficulty;
    GamePhase  phase;

    int  lock_r, lock_c;    // mid-multi-jump: only this piece may continue (-1 = none)
    Pt   last_from, last_to;
    bool has_last;

    unsigned events;        // EV_* set this frame, cleared by game_frame_begin
    unsigned rng;           // AI tie-breaks / easy-mode noise
} Game;

// Inline classifiers (used by the renderer too).
static inline Side square_color(Square s) {
    return (s == RED_MAN || s == RED_KING) ? SIDE_RED : SIDE_BLACK;
}
static inline bool square_is_king(Square s) {
    return s == RED_KING || s == BLACK_KING;
}
static inline bool square_empty(Square s) { return s == EMPTY; }

// Lifecycle ----------------------------------------------------------------
Game* game_create(Difficulty diff, Side human);
void  game_destroy(Game* g);
void  game_frame_begin(Game* g);     // clear per-frame events

// Interaction (human, click-to-move) --------------------------------------
// Legal immediate destination squares for the piece at (r,c), honouring whose
// turn it is, an in-progress multi-jump lock, and the mandatory-capture rule.
int  game_targets(const Game* g, int r, int c, Pt* out, int max);
// Attempt to move the piece at (fr,fc) to (tr,tc). Returns:
//   0 = illegal (no state change, EV_ILLEGAL set)
//   1 = applied, turn is over
//   2 = applied a jump, the same piece must keep jumping (stays locked)
int  game_try_step(Game* g, int fr, int fc, int tr, int tc);

// AI -----------------------------------------------------------------------
// Compute the best full move for the side to move. Returns false if none.
bool game_ai_move(const Game* g, Move* out);
// Apply a full move (used by the AI and tests): relocates, captures, crowns,
// scores events, flips the turn, and resolves any win/loss.
void game_apply_move(Game* g, const Move* m);

// Count a colour's remaining pieces (for the status bar / tests).
int  game_piece_count(const Game* g, Side side);

#endif
