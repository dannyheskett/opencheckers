#include "input.h"
#include <raylib.h>

Input input_poll(void) {
    Input in = {0};

    Vector2 mp = GetMousePosition();
    in.mouse_x = (int)mp.x;
    in.mouse_y = (int)mp.y;
    in.left_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    in.escape_pressed = IsKeyPressed(KEY_ESCAPE);

    bool alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    in.fullscreen_toggle = alt && IsKeyPressed(KEY_ENTER);

    in.menu_up   = IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W);
    in.menu_down = IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S);
    in.select_pressed = (IsKeyPressed(KEY_ENTER) && !in.fullscreen_toggle)
                      || IsKeyPressed(KEY_SPACE);

    in.any_pressed = in.left_pressed || in.escape_pressed || in.menu_up
                  || in.menu_down || in.select_pressed;

    return in;
}
