#ifndef GAME_H
#define GAME_H

#include "core_types.h"

// ──────────────────────────────────────────────
// Game lifecycle
// ──────────────────────────────────────────────

// Initialize a fresh game: create deck, shuffle, deal first room
void game_init(GameState *state);

// ──────────────────────────────────────────────
// Room management
// ──────────────────────────────────────────────

// Deal a full room (4 cards). Called at game start and after room complete.
// If there's a carryover card, only deals 3 new cards.
// Sets phase to PHASE_ROOM_DEALT.
void game_deal_room(GameState *state);

// Avoid the current room. Returns false if not allowed (avoided last room).
// Moves all 4 cards to bottom of dungeon and deals a new room.
bool game_avoid_room(GameState *state);

// ──────────────────────────────────────────────
// Card resolution — call during PHASE_SELECT_CARD / PHASE_RESOLVE_ACTION
// ──────────────────────────────────────────────

// Select a card from the room (index 0-3). Returns false if slot is empty.
// Sets phase to PHASE_RESOLVE_ACTION.
bool game_select_card(GameState *state, uint8_t index);

// Get available actions for the currently selected card.
// Returns a bitmask of ActionType values (bit 0 = ACTION_FIGHT_WEAPON, etc.)
// Caller should check which actions are valid before presenting them.
uint8_t game_get_available_actions(const GameState *state);

// Resolve the selected card with the given action.
// Returns true if resolution succeeded.
// May change phase to PHASE_COMBAT_RESULT or PHASE_SELECT_CARD.
bool game_resolve(GameState *state, ActionType action);

// Called by UI after displaying combat result animation.
// Transitions from PHASE_COMBAT_RESULT to next state.
void game_finish_combat(GameState *state);

// ──────────────────────────────────────────────
// Combat helpers
// ──────────────────────────────────────────────

// Calculate damage from fighting a monster with/without weapon.
// Returns damage the player would take.
int16_t game_calc_combat_damage(const GameState *state, bool use_weapon);

// Check if the current weapon can be used against a monster of given value.
bool game_weapon_can_hit(const GameState *state, uint8_t monster_value);

// ──────────────────────────────────────────────
// State queries
// ──────────────────────────────────────────────

// Cards remaining in dungeon (including cards not yet dealt)
uint8_t game_dungeon_remaining(const GameState *state);

// Total cards remaining to resolve (dungeon + room cards)
uint8_t game_total_remaining(const GameState *state);

// Check if game is over
bool game_is_over(const GameState *state);

// Calculate score (call when game ends)
int16_t game_calculate_score(GameState *state);

// Get the currently selected card index (0xFF if none)
uint8_t game_selected_index(const GameState *state);

// Deselect the current card (return to PHASE_SELECT_CARD)
void game_deselect(GameState *state);

#endif // GAME_H
