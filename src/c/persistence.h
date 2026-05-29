#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include "core_types.h"

bool save_exists(void);
void save_game(const GameState *state);
bool load_game(GameState *state);
void save_clear(void);

// High score
void save_high_score(int16_t score);
int16_t load_high_score(void);

#endif // PERSISTENCE_H
