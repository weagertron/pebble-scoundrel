#ifndef CORE_TYPES_H
#define CORE_TYPES_H

#include <pebble.h>
#include <stdint.h>
#include <stdbool.h>

// ──────────────────────────────────────────────
// Constants
// ──────────────────────────────────────────────

#define MAX_HP            20
#define DECK_SIZE         44
#define ROOM_SIZE         4
#define CARDS_PER_ROOM    3   // player resolves 3 of 4 per room
#define WEAPON_ANY        0xFF // weapon can hit any monster value

// Pebble Time 2 (Emery) display
#define SCREEN_W          200
#define SCREEN_H          228

// ──────────────────────────────────────────────
// Enums
// ──────────────────────────────────────────────

typedef enum {
  SUIT_HEARTS,
  SUIT_DIAMONDS,
  SUIT_SPADES,
  SUIT_CLUBS,
  SUIT_COUNT
} Suit;

typedef enum {
  CARD_POTION,    // ♥ Hearts
  CARD_WEAPON,    // ♦ Diamonds
  CARD_MONSTER    // ♠ Spades, ♣ Clubs
} CardRole;

typedef enum {
  PHASE_TITLE,
  PHASE_ROOM_DEALT,     // 4 cards showing; can avoid or start resolving
  PHASE_SELECT_CARD,    // picking a card to resolve
  PHASE_RESOLVE_ACTION, // card selected, choosing action (fight/equip/drink/skip)
  PHASE_COMBAT_RESULT,  // brief combat outcome flash
  PHASE_ROOM_COMPLETE,  // resolved 3 cards, transitioning to next room
  PHASE_GAME_OVER,
  PHASE_VICTORY
} GamePhase;

typedef enum {
  SCREEN_TITLE,
  SCREEN_GAME,
  SCREEN_RULES,
  SCREEN_GAME_OVER,
  SCREEN_QUIT
} GameScreen;

typedef enum {
  ACTION_NONE,
  ACTION_FIGHT_WEAPON,
  ACTION_FIGHT_BARE,
  ACTION_EQUIP,
  ACTION_DRINK,
  ACTION_SKIP_POTION,
  ACTION_SKIP_MONSTER,
  ACTION_AVOID_ROOM
} ActionType;

// ──────────────────────────────────────────────
// Structs
// ──────────────────────────────────────────────

// A single card (2 bytes)
typedef struct {
  uint8_t value;  // 2–14 (J=11, Q=12, K=13, A=14)
  uint8_t suit;   // Suit enum
} Card;

// Weapon state
typedef struct {
  uint8_t value;        // weapon card value (0 = unequipped)
  uint8_t suit;         // weapon card suit
  uint8_t max_monster;  // highest monster value it can still hit (WEAPON_ANY = any)
  bool equipped;
} WeaponState;

// Full game state — fits in ~80 bytes
typedef struct {
  // Player
  uint8_t hp;
  WeaponState weapon;

  // Dungeon
  Card dungeon[DECK_SIZE];
  uint8_t dungeon_count;   // total cards remaining in dungeon pile
  uint8_t dungeon_idx;     // next card index to deal (0 = top)

  // Current room
  Card room[ROOM_SIZE];
  bool room_active[ROOM_SIZE]; // true = card present in slot
  uint8_t carryover_slot;      // which slot has the carried-over card (0xFF = none)

  // Room state
  uint8_t cards_resolved;  // 0–3 resolved this room
  bool potion_used;        // potion already used this room
  bool avoided_last;       // avoided previous room (can't avoid twice in a row)

  // Game state
  GamePhase phase;
  int16_t score;
} GameState;

// Packed save data for persistence
#define PERSIST_KEY_SAVE       10
#define PERSIST_KEY_EXISTS     11
#define PERSIST_KEY_HIGH_SCORE 12

typedef struct __attribute__((__packed__)) {
  uint8_t hp;
  uint8_t weapon_value;
  uint8_t weapon_suit;
  uint8_t weapon_max_monster;
  uint8_t weapon_equipped;
  uint8_t dungeon_count;
  uint8_t dungeon_idx;
  uint8_t room_values[ROOM_SIZE];
  uint8_t room_suits[ROOM_SIZE];
  uint8_t room_active[ROOM_SIZE];
  uint8_t carryover_slot;
  uint8_t cards_resolved;
  uint8_t potion_used;
  uint8_t avoided_last;
  uint8_t phase;
  Card dungeon_order[DECK_SIZE];
} SaveData;

// ──────────────────────────────────────────────
// Public API — core_types.c
// ──────────────────────────────────────────────

// Get the role of a card based on its suit
CardRole card_get_role(const Card *card);

// Get the suit name character (for rendering)
const char *card_suit_char(const Card *card);

// Get the value display string (e.g., "2"–"10", "J", "Q", "K", "A")
const char *card_value_str(const Card *card);

// Is this card a red suit?
bool card_is_red(const Card *card);

#endif // CORE_TYPES_H
