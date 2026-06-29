#include "game.h"
#include "render.h"
#include "input.h"
#include "sound.h"
#include "recorder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum { STATE_MENU, STATE_PLAYING, STATE_GAMEOVER } AppState;

typedef enum {
    ACT_RESUME, ACT_NEW, ACT_DIFF, ACT_SIDE, ACT_SOUND, ACT_RECORD, ACT_EXIT,
} MenuAction;

#define MAX_MENU_ITEMS 8
#define AI_DELAY       24   // frames the AI "thinks" before replying

static const char* DIFF_NAME[3] = { "Easy", "Medium", "Hard" };

static void play_event_sounds(unsigned ev) {
    if (ev & EV_WIN)  { sound_play(SFX_WIN);  return; }
    if (ev & EV_LOSE) { sound_play(SFX_LOSE); return; }
    if (ev & EV_KING)         sound_play(SFX_KING);
    else if (ev & EV_CAPTURE) sound_play(SFX_CAPTURE);
    else if (ev & EV_MOVE)    sound_play(SFX_MOVE);
    if (ev & EV_ILLEGAL)      sound_play(SFX_ILLEGAL);
}

static int build_menu(bool resumable, Difficulty diff, Side human,
                      const char** labels, MenuAction* actions) {
    static char diff_label[32], side_label[32];
    snprintf(diff_label, sizeof diff_label, "Difficulty: %s", DIFF_NAME[diff]);
    snprintf(side_label, sizeof side_label, "You play: %s", human == SIDE_RED ? "Red" : "Black");
    int n = 0;
    if (resumable) { labels[n] = "Resume Game"; actions[n++] = ACT_RESUME; }
    labels[n] = "New Game";                               actions[n++] = ACT_NEW;
    labels[n] = diff_label;                               actions[n++] = ACT_DIFF;
    labels[n] = side_label;                               actions[n++] = ACT_SIDE;
    labels[n] = sound_is_enabled() ? "Sound: On" : "Sound: Off"; actions[n++] = ACT_SOUND;
    labels[n] = recorder_active()  ? "Record: On" : "Record: Off"; actions[n++] = ACT_RECORD;
    labels[n] = "Exit";                                   actions[n++] = ACT_EXIT;
    return n;
}

static bool is_target(const Pt* t, int n, int r, int c) {
    for (int i = 0; i < n; i++) if (t[i].r == r && t[i].c == c) return true;
    return false;
}

int main(int argc, char** argv) {
    srand((unsigned int)time(NULL));

    bool cli_record = false;
    const char* cli_record_path = NULL;
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "--record") == 0) {
            cli_record = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') cli_record_path = argv[++i];
        }

    Difficulty diff = DIFF_MEDIUM;
    Side human = SIDE_RED;

    render_init();
    sound_init();
    if (cli_record) recorder_start(cli_record_path);

    Game* game = NULL;
    AppState state = STATE_MENU;
    int selected = 0;
    int sel_r = -1, sel_c = -1;     // selected piece
    int ai_delay = AI_DELAY;

    while (!render_window_should_close()) {
        Input in = input_poll();
        if (in.fullscreen_toggle) render_toggle_fullscreen();

        bool resumable = (game != NULL && game->phase == PHASE_PLAYING);
        const char* labels[MAX_MENU_ITEMS];
        MenuAction actions[MAX_MENU_ITEMS];
        int menu_count = build_menu(resumable, diff, human, labels, actions);
        if (selected >= menu_count) selected = 0;

        switch (state) {
        case STATE_MENU:
            if (in.escape_pressed) {
                if (resumable) { state = STATE_PLAYING; break; }
                goto quit;
            }
            if (in.menu_up)   { selected = (selected + menu_count - 1) % menu_count; sound_play(SFX_MENU_MOVE); }
            if (in.menu_down) { selected = (selected + 1) % menu_count;             sound_play(SFX_MENU_MOVE); }
            if (in.select_pressed) {
                sound_play(SFX_MENU_SELECT);
                switch (actions[selected]) {
                case ACT_RESUME: state = STATE_PLAYING; break;
                case ACT_NEW:
                    if (game) game_destroy(game);
                    game = game_create(diff, human);
                    if (recorder_active()) { recorder_stop(); recorder_start(NULL); }
                    sel_r = sel_c = -1; ai_delay = AI_DELAY;
                    state = STATE_PLAYING;
                    break;
                case ACT_DIFF: diff = (Difficulty)((diff + 1) % 3); break;
                case ACT_SIDE: human = (human == SIDE_RED) ? SIDE_BLACK : SIDE_RED; break;
                case ACT_SOUND: sound_toggle(); sound_play(SFX_MENU_SELECT); break;
                case ACT_RECORD: recorder_toggle(); break;
                case ACT_EXIT: goto quit;
                }
            }
            break;

        case STATE_PLAYING: {
            if (!game) { state = STATE_MENU; break; }
            if (in.escape_pressed) { state = STATE_MENU; selected = 0; sel_r = sel_c = -1; break; }

            game_frame_begin(game);

            if (game->phase == PHASE_PLAYING && game->turn != game->human) {
                // AI turn (brief "thinking" delay, then a full move)
                sel_r = sel_c = -1;
                if (ai_delay > 0) ai_delay--;
                else {
                    Move m;
                    if (game_ai_move(game, &m)) game_apply_move(game, &m);
                    ai_delay = AI_DELAY;
                }
            } else if (game->phase == PHASE_PLAYING) {
                ai_delay = AI_DELAY;   // re-arm for the AI's next turn
                if (in.left_pressed) {
                    int r, c;
                    if (render_board_at(in.mouse_x, in.mouse_y, &r, &c)) {
                        Pt tg[16];
                        int ntg = (sel_r >= 0) ? game_targets(game, sel_r, sel_c, tg, 16) : 0;
                        if (sel_r >= 0 && is_target(tg, ntg, r, c)) {
                            int res = game_try_step(game, sel_r, sel_c, r, c);
                            if (res == 2) { sel_r = game->lock_r; sel_c = game->lock_c; }
                            else          { sel_r = sel_c = -1; }
                        } else if (game->lock_r < 0) {
                            // (re)select a piece that has legal moves
                            Pt t2[16];
                            if (game_targets(game, r, c, t2, 16) > 0) { sel_r = r; sel_c = c; }
                            else { sel_r = sel_c = -1; }
                        }
                    } else if (game->lock_r < 0) {
                        sel_r = sel_c = -1;
                    }
                }
            }

            play_event_sounds(game->events);
            if (game->phase != PHASE_PLAYING) { state = STATE_GAMEOVER; sel_r = sel_c = -1; }
            break;
        }

        case STATE_GAMEOVER:
            if (in.any_pressed && !in.fullscreen_toggle) { state = STATE_MENU; selected = 0; }
            break;
        }

        // Render once per frame, after the update (every frame reaches EndDrawing
        // so input polls correctly).
        if (state == STATE_MENU) {
            render_menu("OPENCHECKERS", labels, menu_count, selected, menu_count - 1);
        } else if (state == STATE_GAMEOVER) {
            render_gameover(game, sel_r, sel_c);
        } else {
            Pt tg[16];
            int ntg = (sel_r >= 0) ? game_targets(game, sel_r, sel_c, tg, 16) : 0;
            render_frame(game, sel_r, sel_c, tg, ntg);
        }
    }

quit:
    recorder_stop();
    if (game) game_destroy(game);
    sound_shutdown();
    render_cleanup();
    return 0;
}
