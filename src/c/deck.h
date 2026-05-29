#ifndef DECK_H
#define DECK_H

#include "core_types.h"

// Populate a deck with the correct 44-card Scoundrel composition:
//   Hearts 2-10 (9 potions), Diamonds 2-10 (9 weapons),
//   Spades 2-14 (13 monsters), Clubs 2-14 (13 monsters)
void deck_create(Card deck[DECK_SIZE]);

// Fisher-Yates shuffle using xorshift32 PRNG
// Seeds from time_ms() on first call
void deck_shuffle(Card deck[DECK_SIZE]);

// Deal the next card from the dungeon pile.
// Returns true if a card was dealt, false if dungeon is empty.
// decrement dungeon_count if you want to track remaining.
bool deck_deal(GameState *state, Card *out);

#endif // DECK_H
