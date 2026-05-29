#ifndef GAME_OVER_H
#define GAME_OVER_H

#include "../core_types.h"
typedef void (*GameOverCallback)(GameScreen screen);

void game_over_window_push(GameOverCallback callback, bool victory, int16_t score, uint8_t hp);
void game_over_window_pop(void);

#endif // GAME_OVER_H
