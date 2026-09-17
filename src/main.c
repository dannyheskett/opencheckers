#include "game.h"
#include "render.h"
#include "input.h"
#include "sound.h"
#include "recorder.h"
#include "app.h"
#include "tick.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef PLATFORM_WEB
#include <emscripten/emscripten.h>
#endif

typedef enum { STATE_MENU, STATE_OPTIONS, STATE_PLAYING, STATE_GAMEOVER } AppState;

typedef enum {
    ACT_RESUME, ACT_NEW, ACT_OPTIONS, ACT_SOUND, ACT_RECORD, ACT_EXIT,
} MenuAction;

#define MAX_MENU_ITEMS 6
#define AI_DELAY       24   // fixed 60 Hz steps the AI "thinks" before replying

static const char* DIFF_LABEL[3] = { "Easy", "Medium", "Hard" };

static void play_event_sounds(unsigned ev) {
    if (ev & EV_WIN)  { sound_play(SFX_WIN);  return; }
    if (ev & EV_LOSE) { sound_play(SFX_LOSE); return; }
    if (ev & EV_KING)         sound_play(SFX_KING);
    else if (ev & EV_CAPTURE) sound_play(SFX_CAPTURE);
    else if (ev & EV_MOVE)    sound_play(SFX_MOVE);
    if (ev & EV_ILLEGAL)      sound_play(SFX_ILLEGAL);
}

// Fill labels[]/actions[] with the current menu. Returns the item count and
// sets *gap_before to the index that should have a blank line above it -- Exit,
// which is set apart from the rest -- or -1 when this build has no Exit item at
// all (mobile and web, where the OS or the browser tab owns the lifecycle).
static int build_menu(bool resumable,
                      const char** labels, MenuAction* actions, int* gap_before) {
    int n = 0;
    *gap_before = -1;
    if (resumable) { labels[n] = "Resume Game";                     actions[n++] = ACT_RESUME; }
    labels[n] = "New Game";                                         actions[n++] = ACT_NEW;
    labels[n] = "Options";                                          actions[n++] = ACT_OPTIONS;
    labels[n] = sound_is_enabled() ? "Sound: On" : "Sound: Off";    actions[n++] = ACT_SOUND;
#ifndef OC_TOUCH
    // The mp4 recorder is a desktop-only feature (stubbed out on mobile/web), so
    // the toggle would do nothing there -- omit it.
    labels[n] = recorder_active()  ? "Record: On" : "Record: Off";  actions[n++] = ACT_RECORD;
#endif
#if defined(PLATFORM_WEB)
    // A browser tab can't be closed from code, so no Exit on web. (The renderer —
    // portrait touch vs desktop landscape — is auto-detected by pointer type.)
#elif !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID)
    // Mobile apps don't self-terminate (the OS owns the lifecycle: home gesture /
    // back button on Android, Apple guidelines on iOS), so no Exit on either.
    *gap_before = n;
    labels[n] = "Exit";                                             actions[n++] = ACT_EXIT;
#endif
    return n;
}

// The Options screen: the two choices that shape a game -- the computer's
// strength and which colour you play -- plus Back. Values cycle with
// Left/Right, or by selecting the row, which is how a touch player changes
// them; the last item returns to the menu. Changes apply from the next New
// Game. Labels are rebuilt every frame from the live settings. Sound and Record
// stay on the main menu, where a player expects to find them.
#define OPT_ITEMS 3
enum { OPT_DIFFICULTY, OPT_SIDE, OPT_BACK };

static int build_options(Difficulty diff, Side human, const char** labels) {
    static char buf[OPT_ITEMS][32];
    snprintf(buf[OPT_DIFFICULTY], sizeof buf[0], "Difficulty: %s", DIFF_LABEL[diff]);
    snprintf(buf[OPT_SIDE],       sizeof buf[0], "You Play: %s", human == SIDE_RED ? "Red" : "Black");
    snprintf(buf[OPT_BACK],       sizeof buf[0], "Back");
    for (int i = 0; i < OPT_ITEMS; i++) labels[i] = buf[i];
    return OPT_ITEMS;
}

// Cycle one Options value by `dir` (+1 / -1).
static void cycle_option(Difficulty* diff, Side* human, int item, int dir) {
    switch (item) {
    case OPT_DIFFICULTY:
        *diff = (Difficulty)((*diff + 3 + dir) % 3);
        break;
    case OPT_SIDE:          // two values, so either direction toggles it
        *human = (*human == SIDE_RED) ? SIDE_BLACK : SIDE_RED;
        break;
    default:
        break;
    }
}

static bool is_target(const Pt* t, int n, int r, int c) {
    for (int i = 0; i < n; i++) if (t[i].r == r && t[i].c == c) return true;
    return false;
}

#ifdef OC_SIMSTATS
// Real-device validation instrumentation (SIMSTATS=1 builds; compiles on
// desktop too for local smoke tests). Once per second of continuous play, log
// how many frames were rendered vs how many fixed 60 Hz sim steps ran, so
// scripts/devicefarm_run.py can assert from the device log that the
// fixed-timestep accumulator holds ~60 steps/s at whatever refresh rate the
// display actually delivers (the frames count is the evidence of that rate).
// Any gap between calls (menu, a stall past the spiral clamp) starts a fresh
// window, so every logged line covers uninterrupted play.
#if defined(PLATFORM_ANDROID)
#include <android/log.h>
#define SIMSTATS_LOG(...) __android_log_print(ANDROID_LOG_INFO, "opencheckers", __VA_ARGS__)
#else
#define SIMSTATS_LOG(...) do { printf(__VA_ARGS__); printf("\n"); fflush(stdout); } while (0)
#endif

static void simstats_count(double now, int steps) {
    static double win_start, last_call;
    static int frames, sim_steps;
    if (last_call == 0.0 || now - last_call > 0.25) { // gap: not continuous play
        win_start = now;
        frames = 0;
        sim_steps = 0;
    }
    last_call = now;
    frames++;
    sim_steps += steps;
    double span = now - win_start;
    if (span >= 1.0) {
        SIMSTATS_LOG("SIMSTATS window=%.3f frames=%d steps=%d", span, frames, sim_steps);
        win_start = now;
        frames = 0;
        sim_steps = 0;
    }
}
#endif // OC_SIMSTATS

// App state carried across frames. Kept in one struct so the web build can drive
// the loop from an emscripten per-frame callback (browsers can't block) and iOS
// from a CADisplayLink.
typedef struct {
    Game* game;
    AppState state;
    int selected;
    bool quit;
    Difficulty diff;
    Side human;
    int sel_r, sel_c;   // selected piece (-1 if none)
    int ai_wait;        // fixed steps left before the AI replies
    SimClock clock;     // fixed-timestep accumulator (only advanced while playing)
    double prev_time;   // GetTime() at the previous frame; 0 before the first frame
} AppCtx;

static void app_ctx_init(AppCtx* c) {
    c->game = NULL;
    c->state = STATE_MENU;
    c->selected = 0;
    c->quit = false;
    c->diff = DIFF_MEDIUM;
    c->human = SIDE_RED;
    c->sel_r = c->sel_c = -1;
    c->ai_wait = AI_DELAY;
    sim_clock_reset(&c->clock);
    c->prev_time = 0.0;
#ifdef OC_AUTOPLAY
    // Validation builds skip the menu and start playing at boot (see SIMSTATS
    // in the Makefile); the AI plays both sides.
    c->game = game_create(c->diff, c->human);
    c->state = STATE_PLAYING;
#endif
}

static void start_new_game(AppCtx* c) {
    if (c->game) game_destroy(c->game);
    c->game = game_create(c->diff, c->human);
    c->sel_r = c->sel_c = -1;
    c->ai_wait = AI_DELAY;
    c->state = STATE_PLAYING;
}

// A press on the board by the human: select a piece, move the selected one, or
// continue a multi-jump.
static void handle_board_press(AppCtx* c, int x, int y) {
    Game* g = c->game;
    int r, col;
    if (render_board_at(x, y, &r, &col)) {
        Pt tg[16];
        int ntg = (c->sel_r >= 0) ? game_targets(g, c->sel_r, c->sel_c, tg, 16) : 0;
        if (c->sel_r >= 0 && is_target(tg, ntg, r, col)) {
            int res = game_try_step(g, c->sel_r, c->sel_c, r, col);
            if (res == 2) { c->sel_r = g->lock_r; c->sel_c = g->lock_c; }
            else          { c->sel_r = c->sel_c = -1; }
        } else if (g->lock_r < 0) {
            // (re)select a piece that has legal moves
            Pt t2[16];
            if (game_targets(g, r, col, t2, 16) > 0) { c->sel_r = r; c->sel_c = col; }
            else { c->sel_r = c->sel_c = -1; }
        }
    } else if (g->lock_r < 0) {
        c->sel_r = c->sel_c = -1;
    }
}

// One iteration of the game loop. `arg` is an AppCtx* (void* to match the
// emscripten_set_main_loop callback signature).
static void frame_step(void* arg) {
    AppCtx* c = (AppCtx*)arg;

    // Real seconds since the previous frame, feeding the fixed-timestep
    // accumulator so the AI's thinking delay lasts the same on any display
    // refresh. The first frame (prev_time == 0) is treated as exactly one step.
    // The clock only banks time while actually playing.
    double now = GetTime();
    double dt = (c->prev_time > 0.0) ? now - c->prev_time : SIM_DT;
    c->prev_time = now;
    if (c->state != STATE_PLAYING) sim_clock_reset(&c->clock);

    Input in = input_poll();
    if (in.fullscreen_toggle) render_toggle_fullscreen();

    bool resumable = (c->game != NULL && c->game->phase == PHASE_PLAYING);
    const char* labels[MAX_MENU_ITEMS];
    MenuAction actions[MAX_MENU_ITEMS];
    int gap_before = -1;
    int menu_count = build_menu(resumable, labels, actions, &gap_before);

    switch (c->state) {
    case STATE_MENU: {
        if (c->selected >= menu_count) c->selected = 0;
        if (in.escape_pressed) {
            // Escape backs out: resume a game in progress, else quit (native).
            if (resumable) { c->state = STATE_PLAYING; break; }
            c->quit = true; return;
        }
        if (in.menu_up)   { c->selected = (c->selected + menu_count - 1) % menu_count; sound_play(SFX_MENU_MOVE); }
        if (in.menu_down) { c->selected = (c->selected + 1) % menu_count;             sound_play(SFX_MENU_MOVE); }
        // Touch: a tap directly on a menu item selects it. Keyboard select
        // activates the highlighted item.
        bool do_select = in.select_pressed;
        if (in.touch_tap) {
            int hit = render_menu_hit_test((Vector2){in.tap_x, in.tap_y});
            if (hit >= 0 && hit < menu_count) { c->selected = hit; do_select = true; }
        }
        if (do_select) {
            sound_play(SFX_MENU_SELECT);
            switch (actions[c->selected]) {
            case ACT_RESUME: c->state = STATE_PLAYING; break;
            case ACT_NEW:
                start_new_game(c);
                if (recorder_active()) { recorder_stop(); recorder_start(NULL); }
                break;
            case ACT_OPTIONS: c->state = STATE_OPTIONS; c->selected = 0; break;
            case ACT_SOUND: sound_toggle(); sound_play(SFX_MENU_SELECT); break;
            case ACT_RECORD: recorder_toggle(); break;
            case ACT_EXIT: c->quit = true; return;
            }
        }
        break;
    }

    case STATE_OPTIONS: {
        const char* opt_labels[OPT_ITEMS];
        int opt_count = build_options(c->diff, c->human, opt_labels);
        if (c->selected >= opt_count) c->selected = 0;
        if (in.escape_pressed) {
            c->state = STATE_MENU;
            c->selected = 0;
            break;
        }
        if (in.menu_up) {
            c->selected = (c->selected + opt_count - 1) % opt_count;
            sound_play(SFX_MENU_MOVE);
        }
        if (in.menu_down) {
            c->selected = (c->selected + 1) % opt_count;
            sound_play(SFX_MENU_MOVE);
        }
        int dir = (in.menu_right ? 1 : 0) - (in.menu_left ? 1 : 0);
        bool do_select = in.select_pressed;
        if (in.touch_tap) {
            int hit = render_menu_hit_test((Vector2){in.tap_x, in.tap_y});
            if (hit >= 0 && hit < opt_count) { c->selected = hit; do_select = true; }
        }
        if (do_select && c->selected == OPT_BACK) {
            c->state = STATE_MENU;
            c->selected = 0;
            sound_play(SFX_MENU_SELECT);
        } else if (dir != 0 || do_select) {
            cycle_option(&c->diff, &c->human, c->selected, dir ? dir : 1);
            sound_play(SFX_MENU_SELECT);
        }
        break;
    }

    case STATE_PLAYING: {
        Game* g = c->game;
        if (!g) { c->state = STATE_MENU; break; }
#ifdef OC_TOUCH
        // Leave for the menu when the app is backgrounded (Android / iOS) or the
        // browser tab loses focus (web); the game stays resumable.
        if (!render_window_focused()) {
            c->state = STATE_MENU; c->selected = 0; c->sel_r = c->sel_c = -1;
            break;
        }
#endif
        if (in.escape_pressed) { c->state = STATE_MENU; c->selected = 0; c->sel_r = c->sel_c = -1; break; }

        game_frame_begin(g);
        int steps = sim_clock_advance(&c->clock, dt);
#ifdef OC_SIMSTATS
        simstats_count(now, steps);
#endif

#ifdef OC_AUTOPLAY
        bool ai_turn = true;   // validation builds: the AI plays both sides
#else
        bool ai_turn = (g->turn != g->human);
#endif
        if (g->phase == PHASE_PLAYING && ai_turn) {
            // AI turn (brief "thinking" delay, then a full move)
            c->sel_r = c->sel_c = -1;
            c->ai_wait -= steps;
            if (c->ai_wait <= 0) {
                Move m;
                if (game_ai_move(g, &m)) game_apply_move(g, &m);
                c->ai_wait = AI_DELAY;
            }
        } else if (g->phase == PHASE_PLAYING) {
            c->ai_wait = AI_DELAY;   // re-arm for the AI's next turn
            if (in.touch_tap)         handle_board_press(c, (int)in.tap_x, (int)in.tap_y);
            else if (in.left_pressed) handle_board_press(c, in.mouse_x, in.mouse_y);
        }

        play_event_sounds(g->events);
        if (g->phase != PHASE_PLAYING) { c->state = STATE_GAMEOVER; c->sel_r = c->sel_c = -1; }
        break;
    }

    case STATE_GAMEOVER:
#ifdef OC_AUTOPLAY
        // Validation builds restart immediately so an unattended device run
        // measures play for its whole duration.
        start_new_game(c);
        break;
#else
        if (in.any_pressed && !in.fullscreen_toggle) { c->state = STATE_MENU; c->selected = 0; }
        break;
#endif
    }

    // Render once per frame, after the update (every frame reaches the end of
    // drawing so input polls correctly).
    if (c->state == STATE_MENU) {
        render_menu("OPENCHECKERS", labels, menu_count, c->selected, gap_before);
    } else if (c->state == STATE_OPTIONS) {
        const char* opt_labels[OPT_ITEMS];
        int opt_count = build_options(c->diff, c->human, opt_labels);
        render_menu("OPTIONS", opt_labels, opt_count, c->selected, OPT_BACK);
    } else if (c->state == STATE_GAMEOVER) {
        render_gameover(c->game, c->sel_r, c->sel_c);
    } else {
        Pt tg[16];
        int ntg = (c->sel_r >= 0) ? game_targets(c->game, c->sel_r, c->sel_c, tg, 16) : 0;
        render_frame(c->game, c->sel_r, c->sel_c, tg, ntg);
    }
}

#if defined(PLATFORM_IOS)

// iOS: UIKit provides main() and the run loop, so the normal main() below is
// compiled out. The app shell (ios_main.mm) sets up the Metal layer, calls
// oc_app_init() once, then oc_app_frame() from a CADisplayLink each frame.
static AppCtx ios_ctx;

void oc_app_init(void) {
    srand((unsigned int)time(NULL));
    render_init();   // no-op on iOS (UIKit owns the window)
    sound_init();
    app_ctx_init(&ios_ctx);
}

void oc_app_frame(void) { frame_step(&ios_ctx); }

#else

int main(int argc, char** argv) {
    srand((unsigned int)time(NULL));

    // CLI: --record [path] starts recording immediately (auto-named if no path).
    bool cli_record = false;
    const char* cli_record_path = NULL;
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "--record") == 0) {
            cli_record = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') cli_record_path = argv[++i];
        }

    render_init();
    sound_init();

#ifdef PLATFORM_WEB
    // Pick the renderer by the primary pointer: coarse (phone/tablet) -> portrait
    // touch layout; fine (desktop / 2-in-1 laptop) -> desktop landscape layout +
    // mouse, matching the native desktop app.
    render_set_portrait(emscripten_run_script_int(
        "(window.matchMedia && window.matchMedia('(pointer: coarse)').matches) ? 1 : 0"));
#endif

    if (cli_record) recorder_start(cli_record_path);

    // Static so the pointer handed to emscripten stays valid after main()'s stack
    // is unwound on the web build (see the PLATFORM_WEB branch below).
    static AppCtx ctx;
    app_ctx_init(&ctx);

#ifdef PLATFORM_WEB
    // Browsers drive the loop via a per-frame callback; with the infinite-loop
    // flag this call does not return, so the native cleanup below never runs on
    // web (the browser tab owns the lifetime).
    emscripten_set_main_loop_arg(frame_step, &ctx, 0, 1);
#else
    while (!render_window_should_close() && !ctx.quit) {
        frame_step(&ctx);
    }
    recorder_stop(); // finalize the .mp4 if recording
    if (ctx.game) game_destroy(ctx.game);
    sound_shutdown();
    render_cleanup();
#endif
    return 0;
}

#endif // PLATFORM_IOS (main() is compiled out on iOS)
