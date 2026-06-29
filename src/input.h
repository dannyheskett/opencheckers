#ifndef OPENCHECKERS_INPUT_H
#define OPENCHECKERS_INPUT_H

#include <stdbool.h>

typedef struct {
    int  mouse_x, mouse_y;
    bool left_pressed;        // left button just went down

    bool escape_pressed;
    bool fullscreen_toggle;   // Alt+Enter

    bool menu_up, menu_down;
    bool select_pressed;
    bool any_pressed;
} Input;

Input input_poll(void);

#endif
