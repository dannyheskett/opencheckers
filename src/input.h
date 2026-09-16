#ifndef OPENCHECKERS_INPUT_H
#define OPENCHECKERS_INPUT_H

#include <stdbool.h>

typedef struct {
    // Mouse (desktop, and the web build's desktop layout)
    int  mouse_x, mouse_y;
    bool left_pressed;        // left button just went down

    // Menu / overlays
    bool escape_pressed;      // Escape, a two-finger tap, or Android Back
    bool menu_up, menu_down;  // arrow keys / W S, or a vertical swipe
    bool select_pressed;      // Enter or Space
    bool any_pressed;         // any of the above this frame (dismisses game over)

    // Touch: a completed one-finger tap. tap_x/tap_y are valid only when
    // touch_tap is set. Selects a piece, moves it, or picks a menu item.
    bool  touch_tap;
    float tap_x, tap_y;

    // Window
    bool fullscreen_toggle;   // Alt+Enter
} Input;

Input input_poll(void);

#endif
