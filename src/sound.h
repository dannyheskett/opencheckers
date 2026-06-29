#ifndef OPENCHECKERS_SOUND_H
#define OPENCHECKERS_SOUND_H

#include <stdbool.h>

typedef enum {
    SFX_MOVE = 0,
    SFX_CAPTURE,
    SFX_KING,
    SFX_WIN,
    SFX_LOSE,
    SFX_ILLEGAL,
    SFX_MENU_MOVE,
    SFX_MENU_SELECT,
    SFX_COUNT,
} SfxId;

void sound_init(void);
void sound_shutdown(void);
bool sound_is_enabled(void);
void sound_toggle(void);
void sound_play(SfxId id);

#endif
