#include "game.h"
#include "deck.h"
#include <string.h>

// ──────────────────────────────────────────────
// Internal state
// ──────────────────────────────────────────────

static uint8_t s_selected = 0xFF;  // currently selected card slot (0xFF = none)

// ──────────────────────────────────────────────
// Game lifecycle
// ──────────────────────────────────────────────

void game_init(GameState *state) {
  memset(state, 0, sizeof(GameState));

  state->hp = MAX_HP;
  state->phase = PHASE_TITLE;
  state->weapon = (WeaponState){.value = 0, .suit = 0, .max_monster = WEAPON_ANY, .equipped = false};
  state->carryover_slot = 0xFF;

  deck_create(state->dungeon);
  deck_shuffle(state->dungeon);
  state->dungeon_count = DECK_SIZE;
  state->dungeon_idx = 0;

  game_deal_room(state);
}

// ──────────────────────────────────────────────
// Room management
// ──────────────────────────────────────────────

void game_deal_room(GameState *state) {
  // Reset room state
  state->cards_resolved = 0;
  state->potion_used = false;

  // If we have a carryover card, keep it; otherwise clear all slots
  if (state->carryover_slot != 0xFF) {
    // Keep the carryover card in its slot
    for (int i = 0; i < ROOM_SIZE; i++) {
      if (i != state->carryover_slot) {
        // Deal a new card into this slot
        if (!deck_deal(state, &state->room[i])) {
          state->room_active[i] = false;
        } else {
          state->room_active[i] = true;
        }
      }
      // carryover slot is already active with its card
    }
  } else {
    // Fresh room: deal 4 cards
    for (int i = 0; i < ROOM_SIZE; i++) {
      if (!deck_deal(state, &state->room[i])) {
        state->room_active[i] = false;
      } else {
        state->room_active[i] = true;
      }
    }
  }

  // Check if all cards are resolved (deck empty and room empty)
  if (!game_total_remaining(state)) {
    state->phase = PHASE_VICTORY;
    return;
  }

  state->phase = PHASE_ROOM_DEALT;
}

bool game_avoid_room(GameState *state) {
  if (state->avoided_last) return false;
  if (state->cards_resolved > 0) return false; // can't avoid mid-room

  // Collect remaining dungeon cards, then append room cards at the end.
  // Copy back to front of dungeon array and reset dungeon_idx.

  Card temp[DECK_SIZE];
  int temp_idx = 0;

  // Copy remaining dungeon cards
  for (int i = state->dungeon_idx; i < DECK_SIZE; i++) {
    temp[temp_idx++] = state->dungeon[i];
  }

  // Append room cards
  for (int i = 0; i < ROOM_SIZE; i++) {
    if (state->room_active[i]) {
      temp[temp_idx++] = state->room[i];
    }
  }

  // Copy back
  memcpy(state->dungeon, temp, temp_idx * sizeof(Card));
  state->dungeon_idx = 0;

  state->avoided_last = true;
  state->carryover_slot = 0xFF;

  // Deal new room
  game_deal_room(state);
  return true;
}

// ──────────────────────────────────────────────
// Card selection
// ──────────────────────────────────────────────

bool game_select_card(GameState *state, uint8_t index) {
  if (index >= ROOM_SIZE) return false;
  if (!state->room_active[index]) return false;

  s_selected = index;
  state->phase = PHASE_RESOLVE_ACTION;
  return true;
}

void game_deselect(GameState *state) {
  s_selected = 0xFF;
  state->phase = PHASE_SELECT_CARD;
}

uint8_t game_selected_index(const GameState *state) {
  return s_selected;
}

// ──────────────────────────────────────────────
// Combat helpers
// ──────────────────────────────────────────────

bool game_weapon_can_hit(const GameState *state, uint8_t monster_value) {
  if (!state->weapon.equipped) return false;
  return monster_value < state->weapon.max_monster;
}

int16_t game_calc_combat_damage(const GameState *state, bool use_weapon) {
  if (s_selected >= ROOM_SIZE) return 0;
  const Card *monster = &state->room[s_selected];

  if (use_weapon && state->weapon.equipped) {
    int16_t dmg = (int16_t)monster->value - (int16_t)state->weapon.value;
    return (dmg > 0) ? dmg : 0;
  } else {
    return (int16_t)monster->value;
  }
}

// ──────────────────────────────────────────────
// Action availability
// ──────────────────────────────────────────────

uint8_t game_get_available_actions(const GameState *state) {
  if (s_selected >= ROOM_SIZE || !state->room_active[s_selected]) {
    return 0;
  }

  uint8_t actions = 0;
  const Card *card = &state->room[s_selected];
  CardRole role = card_get_role(card);

  switch (role) {
    case CARD_MONSTER: {
      // Can always fight barehanded
      actions |= (1 << ACTION_FIGHT_BARE);
      // Can fight with weapon if equipped and can hit this monster
      if (state->weapon.equipped &&
          game_weapon_can_hit(state, card->value)) {
        actions |= (1 << ACTION_FIGHT_WEAPON);
      }
      break;
    }
    case CARD_WEAPON: {
      actions |= (1 << ACTION_EQUIP);
      break;
    }
    case CARD_POTION: {
      if (!state->potion_used) {
        actions |= (1 << ACTION_DRINK);
      } else {
        actions |= (1 << ACTION_SKIP_POTION);
      }
      break;
    }
  }

  return actions;
}

// ──────────────────────────────────────────────
// Card resolution
// ──────────────────────────────────────────────

static void resolve_card_done(GameState *state) {
  // Remove card from room
  state->room_active[s_selected] = false;
  state->cards_resolved++;
  s_selected = 0xFF;

  // Check if 3 cards resolved
  if (state->cards_resolved >= CARDS_PER_ROOM) {
    // Find carryover card
    state->carryover_slot = 0xFF;
    for (int i = 0; i < ROOM_SIZE; i++) {
      if (state->room_active[i]) {
        state->carryover_slot = i;
        break;
      }
    }

    state->avoided_last = false;
    state->phase = PHASE_ROOM_COMPLETE;
  } else {
    state->phase = PHASE_SELECT_CARD;
  }
}

bool game_resolve(GameState *state, ActionType action) {
  if (s_selected >= ROOM_SIZE || !state->room_active[s_selected]) {
    return false;
  }

  uint8_t available = game_get_available_actions(state);
  if (!(available & (1 << action))) return false;

  const Card *card = &state->room[s_selected];

  switch (action) {
    case ACTION_FIGHT_WEAPON: {
      int16_t dmg = game_calc_combat_damage(state, true);
      int16_t new_hp = (int16_t)state->hp - dmg;
      state->hp = (new_hp <= 0) ? 0 : (uint8_t)new_hp;
      // Degrade weapon: can only hit monsters strictly weaker than this one
      state->weapon.max_monster = card->value;
      state->phase = PHASE_COMBAT_RESULT;
      return true;
    }

    case ACTION_FIGHT_BARE: {
      int16_t dmg = (int16_t)card->value;
      int16_t new_hp = (int16_t)state->hp - dmg;
      state->hp = (new_hp <= 0) ? 0 : (uint8_t)new_hp;
      state->phase = PHASE_COMBAT_RESULT;
      return true;
    }

    case ACTION_EQUIP: {
      // Equip the weapon, replacing any previous
      state->weapon.value = card->value;
      state->weapon.suit = card->suit;
      state->weapon.max_monster = WEAPON_ANY; // fresh weapon can hit anything
      state->weapon.equipped = true;
      resolve_card_done(state);
      return true;
    }

    case ACTION_DRINK: {
      if (state->potion_used) return false;
      uint8_t heal = card->value;
      uint8_t new_hp = state->hp + heal;
      state->hp = (new_hp > MAX_HP) ? MAX_HP : new_hp;
      state->potion_used = true;
      resolve_card_done(state);
      return true;
    }

    case ACTION_SKIP_POTION: {
      // Discard potion with no effect
      resolve_card_done(state);
      return true;
    }

    default:
      return false;
  }
}

// Called by UI after displaying combat result
void game_finish_combat(GameState *state) {
  if (state->phase != PHASE_COMBAT_RESULT) return;
  resolve_card_done(state);
}

// ──────────────────────────────────────────────
// State queries
// ──────────────────────────────────────────────

uint8_t game_dungeon_remaining(const GameState *state) {
  return DECK_SIZE - state->dungeon_idx;
}

uint8_t game_total_remaining(const GameState *state) {
  uint8_t room_count = 0;
  for (int i = 0; i < ROOM_SIZE; i++) {
    if (state->room_active[i]) room_count++;
  }
  return game_dungeon_remaining(state) + room_count;
}

bool game_is_over(const GameState *state) {
  return state->hp == 0 || state->phase == PHASE_GAME_OVER ||
         state->phase == PHASE_VICTORY;
}

int16_t game_calculate_score(GameState *state) {
  if (state->hp > 0) {
    // Victory: score = remaining HP
    // Bonus: if HP is 20 and last card was a potion, add potion value
    state->score = (int16_t)state->hp;
    return state->score;
  }

  // Death: score = HP (0) - sum of remaining monster values
  int16_t remaining_monsters = 0;

  // Count monsters in remaining dungeon
  for (int i = state->dungeon_idx; i < DECK_SIZE; i++) {
    if (card_get_role(&state->dungeon[i]) == CARD_MONSTER) {
      remaining_monsters += state->dungeon[i].value;
    }
  }

  // Count monsters in current room
  for (int i = 0; i < ROOM_SIZE; i++) {
    if (state->room_active[i] &&
        card_get_role(&state->room[i]) == CARD_MONSTER) {
      remaining_monsters += state->room[i].value;
    }
  }

  state->score = -(int16_t)remaining_monsters;
  return state->score;
}
