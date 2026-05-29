#include "deck.h"
#include <stdlib.h>

// ──────────────────────────────────────────────
// xorshift32 PRNG
// ──────────────────────────────────────────────

static uint32_t s_rng_state = 0;

static uint32_t rng_next(void) {
  uint32_t x = s_rng_state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  s_rng_state = x;
  return x;
}

static void rng_seed(void) {
  if (s_rng_state == 0) {
    uint16_t ms;
    time_ms(NULL, &ms);
    s_rng_state = (uint32_t)time(NULL) ^ ((uint32_t)ms << 16);
    // Ensure non-zero
    if (s_rng_state == 0) s_rng_state = 1;
    // Warm up
    for (int i = 0; i < 8; i++) rng_next();
  }
}

// ──────────────────────────────────────────────
// Deck creation
// ──────────────────────────────────────────────

void deck_create(Card deck[DECK_SIZE]) {
  int idx = 0;

  // Hearts 2-10: 9 potions
  for (uint8_t v = 2; v <= 10; v++) {
    deck[idx++] = (Card){.value = v, .suit = SUIT_HEARTS};
  }

  // Diamonds 2-10: 9 weapons
  for (uint8_t v = 2; v <= 10; v++) {
    deck[idx++] = (Card){.value = v, .suit = SUIT_DIAMONDS};
  }

  // Spades 2-14 (2 through Ace): 13 monsters
  for (uint8_t v = 2; v <= 14; v++) {
    deck[idx++] = (Card){.value = v, .suit = SUIT_SPADES};
  }

  // Clubs 2-14 (2 through Ace): 13 monsters
  for (uint8_t v = 2; v <= 14; v++) {
    deck[idx++] = (Card){.value = v, .suit = SUIT_CLUBS};
  }
}

// ──────────────────────────────────────────────
// Shuffle (Fisher-Yates)
// ──────────────────────────────────────────────

void deck_shuffle(Card deck[DECK_SIZE]) {
  rng_seed();

  for (int i = DECK_SIZE - 1; i > 0; i--) {
    uint32_t r = rng_next();
    int j = (int)(r % (uint32_t)(i + 1));
    Card tmp = deck[i];
    deck[i] = deck[j];
    deck[j] = tmp;
  }
}

// ──────────────────────────────────────────────
// Deal
// ──────────────────────────────────────────────

bool deck_deal(GameState *state, Card *out) {
  if (state->dungeon_idx >= DECK_SIZE) {
    return false;
  }
  *out = state->dungeon[state->dungeon_idx++];
  return true;
}
