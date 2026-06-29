#include "game.h"
#include <stdlib.h>
#include <string.h>

#define MAX_MOVES 96
#define INF       1000000
#define MATE       100000

// Search depth per difficulty.
static const int DEPTH[3] = { 2, 5, 7 };

// --------------------------------------------------------------------------
// Small board helpers
// --------------------------------------------------------------------------
static bool in_bounds(int r, int c) { return r >= 0 && r < 8 && c >= 0 && c < 8; }

static bool is_enemy(Square s, Side side) {
    return s != EMPTY && square_color(s) != side;
}

// Diagonal directions a piece may travel (men forward only, kings all four).
static int piece_dirs(Square s, int dirs[4][2]) {
    int n = 0;
    if (square_is_king(s)) {
        dirs[n][0] = -1; dirs[n][1] = -1; n++;
        dirs[n][0] = -1; dirs[n][1] =  1; n++;
        dirs[n][0] =  1; dirs[n][1] = -1; n++;
        dirs[n][0] =  1; dirs[n][1] =  1; n++;
    } else if (square_color(s) == SIDE_RED) {        // red moves up (toward row 0)
        dirs[n][0] = -1; dirs[n][1] = -1; n++;
        dirs[n][0] = -1; dirs[n][1] =  1; n++;
    } else {                                     // black moves down
        dirs[n][0] =  1; dirs[n][1] = -1; n++;
        dirs[n][0] =  1; dirs[n][1] =  1; n++;
    }
    return n;
}

static bool dir_ok(Square p, int dr, int dc) {
    if (dr == 0 || abs(dr) != abs(dc)) return false;
    if (square_is_king(p)) return true;
    return (square_color(p) == SIDE_RED) ? dr < 0 : dr > 0;
}

// Crown a man that has reached the far row; returns the (possibly new) piece.
static Square crown_if_needed(Square piece, int land_row, bool* crowned) {
    *crowned = false;
    if (piece == RED_MAN && land_row == 0)   { *crowned = true; return RED_KING; }
    if (piece == BLACK_MAN && land_row == 7) { *crowned = true; return BLACK_KING; }
    return piece;
}

static bool can_capture_from(const Square b[8][8], int r, int c) {
    Square piece = b[r][c];
    if (piece == EMPTY) return false;
    int dirs[4][2], nd = piece_dirs(piece, dirs);
    for (int i = 0; i < nd; i++) {
        int mr = r + dirs[i][0],     mc = c + dirs[i][1];
        int lr = r + 2 * dirs[i][0], lc = c + 2 * dirs[i][1];
        if (in_bounds(lr, lc) && b[lr][lc] == EMPTY
            && is_enemy(b[mr][mc], square_color(piece)))
            return true;
    }
    return false;
}

static bool any_capture_for(const Square b[8][8], Side side) {
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++)
            if (b[r][c] != EMPTY && square_color(b[r][c]) == side
                && can_capture_from(b, r, c))
                return true;
    return false;
}

// --------------------------------------------------------------------------
// Move generation (mutates a scratch board, backtracking)
// --------------------------------------------------------------------------
static void emit(Move* out, int* n, int max, const Move* m) {
    if (*n < max) out[(*n)++] = *m;
}

// Recurse over jump chains from (r,c); records only MAXIMAL sequences (English
// draughts: you must keep jumping until you cannot, but may pick which chain).
static void gen_caps_from(Square b[8][8], int r, int c,
                          Move* work, Move* out, int* n, int max) {
    Square piece = b[r][c];
    int dirs[4][2], nd = piece_dirs(piece, dirs);
    for (int i = 0; i < nd; i++) {
        int dr = dirs[i][0], dc = dirs[i][1];
        int mr = r + dr, mc = c + dc, lr = r + 2 * dr, lc = c + 2 * dc;
        if (!in_bounds(lr, lc) || b[lr][lc] != EMPTY) continue;
        if (!is_enemy(b[mr][mc], square_color(piece))) continue;

        Square cap = b[mr][mc];
        bool crowned;
        Square np = crown_if_needed(piece, lr, &crowned);
        b[r][c] = EMPTY; b[mr][mc] = EMPTY; b[lr][lc] = np;
        work->path[work->path_len++] = (Pt){ lr, lc };
        work->captured[work->n_captured++] = (Pt){ mr, mc };

        int before = *n;
        if (!crowned) gen_caps_from(b, lr, lc, work, out, n, max);
        if (crowned || *n == before) emit(out, n, max, work);   // maximal chain

        work->path_len--; work->n_captured--;
        b[lr][lc] = EMPTY; b[mr][mc] = cap; b[r][c] = piece;
    }
}

// All legal moves for `side`. Captures are mandatory: if any exist, only
// captures are returned.
static int gen_moves(const Square src[8][8], Side side, Move* out, int max) {
    Square b[8][8];
    memcpy(b, src, sizeof b);
    int n = 0;

    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++) {
            if (b[r][c] == EMPTY || square_color(b[r][c]) != side) continue;
            Move work = { .path = {{ r, c }}, .path_len = 1, .n_captured = 0 };
            gen_caps_from(b, r, c, &work, out, &n, max);
        }
    if (n > 0) return n;

    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++) {
            Square piece = src[r][c];
            if (piece == EMPTY || square_color(piece) != side) continue;
            int dirs[4][2], nd = piece_dirs(piece, dirs);
            for (int i = 0; i < nd; i++) {
                int tr = r + dirs[i][0], tc = c + dirs[i][1];
                if (!in_bounds(tr, tc) || src[tr][tc] != EMPTY) continue;
                Move m = { .path = {{ r, c }, { tr, tc }}, .path_len = 2, .n_captured = 0 };
                emit(out, &n, max, &m);
            }
        }
    return n;
}

static void board_apply(Square b[8][8], const Move* m, bool* crowned) {
    Pt s = m->path[0], e = m->path[m->path_len - 1];
    Square piece = b[s.r][s.c];
    b[s.r][s.c] = EMPTY;
    for (int i = 0; i < m->n_captured; i++)
        b[m->captured[i].r][m->captured[i].c] = EMPTY;
    b[e.r][e.c] = crown_if_needed(piece, e.r, crowned);
}

// --------------------------------------------------------------------------
// Lifecycle
// --------------------------------------------------------------------------
Game* game_create(Difficulty diff, Side human) {
    Game* g = calloc(1, sizeof(Game));
    if (!g) return NULL;
    g->difficulty = diff;
    g->human = human;
    g->turn = SIDE_RED;                 // red always moves first
    g->phase = PHASE_PLAYING;
    g->lock_r = g->lock_c = -1;
    g->has_last = false;
    g->rng = (unsigned)rand() * 2654435761u + 1u;

    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++) {
            g->board[r][c] = EMPTY;
            if (((r + c) & 1) == 1) {           // dark squares only
                if (r < 3)      g->board[r][c] = BLACK_MAN;
                else if (r > 4) g->board[r][c] = RED_MAN;
            }
        }
    return g;
}

void game_destroy(Game* g) { free(g); }

void game_frame_begin(Game* g) { g->events = 0; }

int game_piece_count(const Game* g, Side side) {
    int n = 0;
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++)
            if (g->board[r][c] != EMPTY && square_color(g->board[r][c]) == side) n++;
    return n;
}

// Flip the side to move and resolve a win/loss (the new side having no move).
static void end_turn(Game* g) {
    g->lock_r = g->lock_c = -1;
    g->turn = (g->turn == SIDE_RED) ? SIDE_BLACK : SIDE_RED;
    Move tmp[MAX_MOVES];
    if (gen_moves(g->board, g->turn, tmp, MAX_MOVES) == 0) {
        Side winner = (g->turn == SIDE_RED) ? SIDE_BLACK : SIDE_RED;
        if (winner == g->human) { g->phase = PHASE_HUMAN_WON;  g->events |= EV_WIN; }
        else                    { g->phase = PHASE_HUMAN_LOST; g->events |= EV_LOSE; }
    }
}

// --------------------------------------------------------------------------
// Human interaction (click-to-move)
// --------------------------------------------------------------------------
int game_targets(const Game* g, int r, int c, Pt* out, int max) {
    if (g->phase != PHASE_PLAYING || !in_bounds(r, c)) return 0;
    Square piece = g->board[r][c];
    if (piece == EMPTY || square_color(piece) != g->turn) return 0;
    if (g->lock_r >= 0 && (r != g->lock_r || c != g->lock_c)) return 0;

    bool must_cap = (g->lock_r >= 0) || any_capture_for(g->board, g->turn);
    int dirs[4][2], nd = piece_dirs(piece, dirs), n = 0;
    for (int i = 0; i < nd; i++) {
        int dr = dirs[i][0], dc = dirs[i][1];
        if (must_cap) {
            int mr = r + dr, mc = c + dc, lr = r + 2 * dr, lc = c + 2 * dc;
            if (in_bounds(lr, lc) && g->board[lr][lc] == EMPTY
                && is_enemy(g->board[mr][mc], g->turn) && n < max)
                out[n++] = (Pt){ lr, lc };
        } else {
            int tr = r + dr, tc = c + dc;
            if (in_bounds(tr, tc) && g->board[tr][tc] == EMPTY && n < max)
                out[n++] = (Pt){ tr, tc };
        }
    }
    return n;
}

int game_try_step(Game* g, int fr, int fc, int tr, int tc) {
    if (g->phase != PHASE_PLAYING || !in_bounds(fr, fc) || !in_bounds(tr, tc)) {
        g->events |= EV_ILLEGAL; return 0;
    }
    Square piece = g->board[fr][fc];
    if (piece == EMPTY || square_color(piece) != g->turn
        || (g->lock_r >= 0 && (fr != g->lock_r || fc != g->lock_c))
        || !dir_ok(piece, tr - fr, tc - fc)) {
        g->events |= EV_ILLEGAL; return 0;
    }
    int dr = tr - fr;

    if (abs(dr) == 2) {                                   // jump
        int mr = (fr + tr) / 2, mc = (fc + tc) / 2;
        if (g->board[tr][tc] != EMPTY || !is_enemy(g->board[mr][mc], g->turn)) {
            g->events |= EV_ILLEGAL; return 0;
        }
        bool crowned;
        Square np = crown_if_needed(piece, tr, &crowned);
        g->board[fr][fc] = EMPTY; g->board[mr][mc] = EMPTY; g->board[tr][tc] = np;
        g->events |= EV_CAPTURE;
        if (crowned) g->events |= EV_KING;
        if (g->lock_r < 0) g->last_from = (Pt){ fr, fc };
        g->last_to = (Pt){ tr, tc };
        g->has_last = true;

        if (!crowned && can_capture_from(g->board, tr, tc)) {
            g->lock_r = tr; g->lock_c = tc;              // same piece must continue
            return 2;
        }
        end_turn(g);
        return 1;
    }

    // plain step
    if (g->lock_r >= 0 || any_capture_for(g->board, g->turn) || g->board[tr][tc] != EMPTY) {
        g->events |= EV_ILLEGAL; return 0;               // captures are mandatory
    }
    bool crowned;
    Square np = crown_if_needed(piece, tr, &crowned);
    g->board[fr][fc] = EMPTY; g->board[tr][tc] = np;
    g->events |= EV_MOVE;
    if (crowned) g->events |= EV_KING;
    g->last_from = (Pt){ fr, fc };
    g->last_to = (Pt){ tr, tc };
    g->has_last = true;
    end_turn(g);
    return 1;
}

void game_apply_move(Game* g, const Move* m) {
    bool crowned;
    board_apply(g->board, m, &crowned);
    g->events |= (m->n_captured > 0) ? EV_CAPTURE : EV_MOVE;
    if (crowned) g->events |= EV_KING;
    g->last_from = m->path[0];
    g->last_to = m->path[m->path_len - 1];
    g->has_last = true;
    end_turn(g);
}

// --------------------------------------------------------------------------
// AI: alpha-beta minimax (evaluation from SIDE_RED's perspective)
// --------------------------------------------------------------------------
static int evaluate(const Square b[8][8]) {
    int score = 0;
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++) {
            Square s = b[r][c];
            if (s == EMPTY) continue;
            bool center = (c >= 2 && c <= 5);
            switch (s) {
                case RED_MAN:
                    score += 100 + (7 - r) * 2 + (r == 7 ? 6 : 0) + (center ? 1 : 0);
                    break;
                case RED_KING:   score += 175 + (center ? 2 : 0); break;
                case BLACK_MAN:
                    score -= 100 + r * 2 + (r == 0 ? 6 : 0) + (center ? 1 : 0);
                    break;
                case BLACK_KING: score -= 175 + (center ? 2 : 0); break;
                default: break;
            }
        }
    return score;
}

static int alphabeta(Square b[8][8], Side turn, int depth, int alpha, int beta) {
    Move mv[MAX_MOVES];
    int n = gen_moves(b, turn, mv, MAX_MOVES);
    if (n == 0) return (turn == SIDE_RED) ? -(MATE - depth) : (MATE - depth);  // turn loses
    if (depth <= 0) return evaluate(b);

    if (turn == SIDE_RED) {
        int best = -INF;
        for (int i = 0; i < n; i++) {
            Square nb[8][8]; memcpy(nb, b, sizeof nb);
            bool cr; board_apply(nb, &mv[i], &cr);
            int v = alphabeta(nb, SIDE_BLACK, depth - 1, alpha, beta);
            if (v > best) best = v;
            if (best > alpha) alpha = best;
            if (alpha >= beta) break;
        }
        return best;
    }
    int best = INF;
    for (int i = 0; i < n; i++) {
        Square nb[8][8]; memcpy(nb, b, sizeof nb);
        bool cr; board_apply(nb, &mv[i], &cr);
        int v = alphabeta(nb, SIDE_RED, depth - 1, alpha, beta);
        if (v < best) best = v;
        if (best < beta) beta = best;
        if (alpha >= beta) break;
    }
    return best;
}

bool game_ai_move(const Game* g, Move* out) {
    Move mv[MAX_MOVES];
    int n = gen_moves(g->board, g->turn, mv, MAX_MOVES);
    if (n == 0) return false;

    int depth = DEPTH[g->difficulty];
    Side me = g->turn, opp = (me == SIDE_RED) ? SIDE_BLACK : SIDE_RED;
    unsigned rng = g->rng ? g->rng : 1u;

    int vals[MAX_MOVES];
    int best = (me == SIDE_RED) ? -INF : INF, best_i = 0;
    for (int i = 0; i < n; i++) {
        Square nb[8][8]; memcpy(nb, g->board, sizeof nb);
        bool cr; board_apply(nb, &mv[i], &cr);
        int v = alphabeta(nb, opp, depth - 1, -INF, INF);
        rng = rng * 1103515245u + 12345u;                // small jitter for variety
        v += ((me == SIDE_RED) ? 1 : -1) * (((int)((rng >> 16) & 7)) - 3);
        vals[i] = v;
        if ((me == SIDE_RED && v > best) || (me == SIDE_BLACK && v < best)) { best = v; best_i = i; }
    }

    if (g->difficulty == DIFF_EASY) {                    // pick loosely among near-best
        int margin = 80, cand[MAX_MOVES], nc = 0;
        for (int i = 0; i < n; i++) {
            bool near = (me == SIDE_RED) ? (vals[i] >= best - margin) : (vals[i] <= best + margin);
            if (near) cand[nc++] = i;
        }
        rng = rng * 1103515245u + 12345u;
        best_i = cand[(rng >> 16) % nc];
    }
    *out = mv[best_i];
    return true;
}
