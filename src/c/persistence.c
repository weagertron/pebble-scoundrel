#include "persistence.h"
#include <string.h>

bool save_exists(void) {
  return persist_exists(PERSIST_KEY_EXISTS) && persist_read_bool(PERSIST_KEY_EXISTS);
}

void save_game(const GameState *state) {
  // Pack game state into SaveData
  SaveData save;
  memset(&save, 0, sizeof(save));

  save.hp = state->hp;
  save.weapon_value = state->weapon.value;
  save.weapon_suit = state->weapon.suit;
  save.weapon_max_monster = state->weapon.max_monster;
  save.weapon_equipped = state->weapon.equipped ? 1 : 0;
  save.dungeon_count = state->dungeon_count;
  save.dungeon_idx = state->dungeon_idx;

  for (int i = 0; i < ROOM_SIZE; i++) {
    save.room_values[i] = state->room[i].value;
    save.room_suits[i] = state->room[i].suit;
    save.room_active[i] = state->room_active[i] ? 1 : 0;
  }

  save.carryover_slot = state->carryover_slot == 0xFF ? 0xFF : state->carryover_slot;
  save.cards_resolved = state->cards_resolved;
  save.potion_used = state->potion_used ? 1 : 0;
  save.avoided_last = state->avoided_last ? 1 : 0;
  save.phase = (uint8_t)state->phase;

  memcpy(save.dungeon_order, state->dungeon, sizeof(save.dungeon_order));

  persist_write_data(PERSIST_KEY_SAVE, &save, sizeof(save));
  persist_write_bool(PERSIST_KEY_EXISTS, true);
}

bool load_game(GameState *state) {
  if (!save_exists()) return false;

  SaveData save;
  int read = persist_read_data(PERSIST_KEY_SAVE, &save, sizeof(save));
  if (read != sizeof(save)) return false;

  memset(state, 0, sizeof(GameState));

  state->hp = save.hp;
  state->weapon.value = save.weapon_value;
  state->weapon.suit = save.weapon_suit;
  state->weapon.max_monster = save.weapon_max_monster;
  state->weapon.equipped = save.weapon_equipped != 0;
  state->dungeon_count = save.dungeon_count;
  state->dungeon_idx = save.dungeon_idx;

  for (int i = 0; i < ROOM_SIZE; i++) {
    state->room[i].value = save.room_values[i];
    state->room[i].suit = save.room_suits[i];
    state->room_active[i] = save.room_active[i] != 0;
  }

  state->carryover_slot = save.carryover_slot;
  state->cards_resolved = save.cards_resolved;
  state->potion_used = save.potion_used != 0;
  state->avoided_last = save.avoided_last != 0;
  state->phase = (GamePhase)save.phase;

  memcpy(state->dungeon, save.dungeon_order, sizeof(save.dungeon_order));

  return true;
}

void save_clear(void) {
  persist_delete(PERSIST_KEY_SAVE);
  persist_write_bool(PERSIST_KEY_EXISTS, false);
}

void save_high_score(int16_t score) {
  persist_write_int(PERSIST_KEY_HIGH_SCORE, (int32_t)score);
}

int16_t load_high_score(void) {
  if (!persist_exists(PERSIST_KEY_HIGH_SCORE)) return 0;
  return (int16_t)persist_read_int(PERSIST_KEY_HIGH_SCORE);
}
