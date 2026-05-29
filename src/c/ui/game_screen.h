#ifndef GAME_SCREEN_H
#define GAME_SCREEN_H

#include "../core_types.h"
typedef void (*GameScreenCallback)(GameScreen screen);

void game_screen_window_push(GameScreenCallback callback);
void game_screen_window_pop(void);

// Update the display after game state changes
void game_screen_refresh(void);

// Advance from combat result to next state
void game_screen_finish_combat(void);

// Advance from room complete to next room
void game_screen_advance_room(void);

#endif // GAME_SCREEN_H
