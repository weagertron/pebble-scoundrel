#ifndef RENDER_H
#define RENDER_H

#include <pebble.h>
#include "core_types.h"

// ──────────────────────────────────────────────
// Layout constants (Emery 200×228)
// ──────────────────────────────────────────────

#define STATUS_H         20
#define HP_BAR_Y         22
#define HP_BAR_H         6

#define CARD_W           88
#define CARD_H           70
#define CARD_GAP         6
#define CARD_MARGIN      8
#define CARD_GRID_Y      32

// Card grid positions (2×2)
// Row 0: slots 0,1  Row 1: slots 2,3
#define CARD_X(i)  (CARD_MARGIN + ((i) % 2) * (CARD_W + CARD_GAP))
#define CARD_Y(i)  (CARD_GRID_Y + ((i) / 2) * (CARD_H + CARD_GAP))

#define ACTION_Y         182
#define ACTION_H         30
#define ACTION_BTN_W     56
#define ACTION_GAP       4

#define INFO_Y           212
#define INFO_H           16

// ──────────────────────────────────────────────
// Drawing functions
// ──────────────────────────────────────────────

// Draw a single card at the given rect.
// selected=true draws a gold highlight border.
// empty=true draws a face-down/empty slot.
void render_card(GContext *ctx, const Card *card, GRect rect, bool selected, bool empty);

// Draw the HP bar
void render_hp_bar(GContext *ctx, uint8_t hp, uint8_t max_hp);

// Draw HP text label (top-left)
void render_hp_text(GContext *ctx, uint8_t hp);

// Draw the weapon status indicator (top-right area)
void render_weapon_status(GContext *ctx, const WeaponState *weapon);

// Draw an action button at rect with label. enabled controls visual state.
void render_action_button(GContext *ctx, GRect rect, const char *label, bool enabled, bool highlighted);

// Draw the info bar (bottom of screen)
void render_info_bar(GContext *ctx, uint8_t dungeon_remaining, uint8_t cards_resolved,
                     bool potion_used, bool can_avoid);

// Draw suit symbol centered at a point
void render_suit_symbol(GContext *ctx, GPoint center, uint8_t size, Suit suit);

// Get the card grid rect for slot index (0-3)
GRect render_card_rect(uint8_t index);

// Get action button rects (3 buttons across action bar)
void render_action_rects(GRect rects[3]);

// Draw a combat result flash (red tint)
void render_combat_flash(GContext *ctx, int16_t damage);

// Draw the "avoid room" button
void render_avoid_button(GContext *ctx, bool enabled);

#endif // RENDER_H
