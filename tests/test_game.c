// Unit tests for opencheckers' game logic — no raylib, no window. game.c is
// included directly so its file-static helpers are visible.
// Built and run by `make test`; a non-zero exit means a failure.
#include "../src/game.c"
#include <stdio.h>
#include <assert.h>

#define PASS(name) printf("PASS: %s\n", name)
#define FAIL(name, msg) do { fprintf(stderr, "FAIL: %s — %s\n", name, msg); exit(1); } while (0)

static void clear(Game* g) {
    memset(g, 0, sizeof *g);
    g->turn = SIDE_RED;
    g->human = SIDE_RED;
    g->phase = PHASE_PLAYING;
    g->lock_r = g->lock_c = -1;
}

static bool has_target(const Game* g, int r, int c, int tr, int tc) {
    Pt t[16];
    int n = game_targets(g, r, c, t, 16);
    for (int i = 0; i < n; i++) if (t[i].r == tr && t[i].c == tc) return true;
    return false;
}

// --------------------------------------------------------------------------
static void test_setup(void) {
    srand(1);
    Game* g = game_create(DIFF_MEDIUM, SIDE_RED);
    if (game_piece_count(g, SIDE_RED) != 12)   FAIL("setup", "red != 12");
    if (game_piece_count(g, SIDE_BLACK) != 12) FAIL("setup", "black != 12");
    if (g->turn != SIDE_RED) FAIL("setup", "red should move first");
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++) {
            Square s = g->board[r][c];
            if (s != EMPTY && ((r + c) & 1) == 0) FAIL("setup", "piece on light square");
            if (s == RED_MAN && r < 5)   FAIL("setup", "red not at bottom");
            if (s == BLACK_MAN && r > 2) FAIL("setup", "black not at top");
        }
    game_destroy(g);
    PASS("setup");
}

// Men step diagonally forward only; kings step all four diagonals.
static void test_step_dirs(void) {
    Game g; clear(&g);
    g.board[5][2] = RED_MAN;
    if (!has_target(&g, 5, 2, 4, 1) || !has_target(&g, 5, 2, 4, 3))
        FAIL("step_dirs", "red man should step up");
    if (has_target(&g, 5, 2, 6, 1) || has_target(&g, 5, 2, 6, 3))
        FAIL("step_dirs", "red man must not step down");

    clear(&g); g.turn = SIDE_BLACK; g.board[2][3] = BLACK_MAN;
    if (!has_target(&g, 2, 3, 3, 2) || !has_target(&g, 2, 3, 3, 4))
        FAIL("step_dirs", "black man should step down");

    clear(&g); g.board[4][3] = RED_KING;
    Pt t[16]; int n = game_targets(&g, 4, 3, t, 16);
    if (n != 4) FAIL("step_dirs", "king should have 4 steps");
    PASS("step_dirs");
}

// Captures are mandatory: a plain step is illegal while any capture exists.
static void test_mandatory_capture(void) {
    Game g; clear(&g);
    g.board[5][0] = RED_MAN;                 // could only step to (4,1)
    g.board[5][4] = RED_MAN;                 // can capture (4,3)->(3,2)
    g.board[4][3] = BLACK_MAN;
    if (has_target(&g, 5, 0, 4, 1))
        FAIL("mandatory", "plain step offered while a capture exists");
    if (!has_target(&g, 5, 4, 3, 2))
        FAIL("mandatory", "the capture should be offered");
    if (game_try_step(&g, 5, 0, 4, 1) != 0)
        FAIL("mandatory", "plain step should be rejected");
    if (game_try_step(&g, 5, 4, 3, 2) != 1)
        FAIL("mandatory", "capture should apply and end the turn");
    if (g.board[4][3] != EMPTY) FAIL("mandatory", "captured piece not removed");
    PASS("mandatory");
}

// A multi-jump continues with the same piece, then ends the turn.
static void test_multijump(void) {
    Game g; clear(&g);
    g.board[6][1] = RED_MAN;
    g.board[5][2] = BLACK_MAN;
    g.board[3][2] = BLACK_MAN;
    int r1 = game_try_step(&g, 6, 1, 4, 3);   // jump (5,2)
    if (r1 != 2) FAIL("multijump", "first jump should require continuation");
    if (g.lock_r != 4 || g.lock_c != 3) FAIL("multijump", "piece not locked for continuation");
    int r2 = game_try_step(&g, 4, 3, 2, 1);   // jump (3,2)
    if (r2 != 1) FAIL("multijump", "second jump should end the turn");
    if (g.board[2][1] != RED_MAN) FAIL("multijump", "piece not at final square");
    if (g.board[5][2] != EMPTY || g.board[3][2] != EMPTY) FAIL("multijump", "captures remain");
    if (g.turn != SIDE_BLACK) FAIL("multijump", "turn did not pass");
    PASS("multijump");
}

// Reaching the back row crowns and ENDS the turn, even if another jump exists.
static void test_crown_ends_turn(void) {
    Game g; clear(&g);
    g.board[2][1] = RED_MAN;
    g.board[1][2] = BLACK_MAN;     // jumped: (2,1)->(0,3), crowns
    g.board[1][4] = BLACK_MAN;     // a king at (0,3) could jump this, but must NOT
    int r = game_try_step(&g, 2, 1, 0, 3);
    if (r != 1) FAIL("crown", "crowning should end the turn");
    if (g.board[0][3] != RED_KING) FAIL("crown", "man not crowned");
    if (g.board[1][4] != BLACK_MAN) FAIL("crown", "continued past crowning");
    if (g.turn != SIDE_BLACK) FAIL("crown", "turn did not pass after crowning");
    PASS("crown");
}

// Losing all pieces (hence no moves) ends the game in favour of the other side.
static void test_win_on_capture(void) {
    Game g; clear(&g);
    g.human = SIDE_RED;
    g.board[5][2] = RED_MAN;       // red's only piece, about to capture
    g.board[4][3] = BLACK_MAN;     // black's only piece
    int r = game_try_step(&g, 5, 2, 3, 4);   // jump (4,3)
    if (r != 1) FAIL("win", "capture should apply");
    if (g.phase != PHASE_HUMAN_WON) FAIL("win", "human should have won");
    PASS("win");
}

// AI returns a legal move, and is forced into a capture when one exists.
static void test_ai(void) {
    srand(2);
    Game* g = game_create(DIFF_MEDIUM, SIDE_RED);
    Move m;
    if (!game_ai_move(g, &m)) FAIL("ai", "no move on fresh board");
    game_apply_move(g, &m);
    if (game_piece_count(g, SIDE_RED) != 12) FAIL("ai", "fresh move shouldn't capture");
    game_destroy(g);

    Game h; clear(&h); h.turn = SIDE_BLACK; h.difficulty = DIFF_MEDIUM;
    h.board[2][3] = BLACK_MAN;
    h.board[3][4] = RED_MAN;       // black must jump (3,4)->(4,5)
    Move cap;
    if (!game_ai_move(&h, &cap)) FAIL("ai", "no move when a capture is forced");
    if (cap.n_captured < 1) FAIL("ai", "AI ignored a mandatory capture");
    PASS("ai");
}

int main(void) {
    test_setup();
    test_step_dirs();
    test_mandatory_capture();
    test_multijump();
    test_crown_ends_turn();
    test_win_on_capture();
    test_ai();
    printf("\nAll tests passed.\n");
    return 0;
}
