#include "render.h"
#include <stdio.h>
#include <string.h>

// ──────────────────────────────────────────────
// Colors
// ──────────────────────────────────────────────

#define COL_BG           GColorBlack
#define COL_CARD_BG      GColorWhite
#define COL_RED_SUIT     GColorDarkCandyAppleRed
#define COL_BLACK_SUIT   GColorBlack
#define COL_HP_HEALTHY   GColorIslamicGreen
#define COL_HP_LOW       GColorDarkCandyAppleRed
#define COL_HP_BG        GColorDarkGray
#define COL_WEAPON       GColorChromeYellow
#define COL_SELECTED     GColorChromeYellow
#define COL_BTN_BG       GColorWhite
#define COL_BTN_TEXT     GColorBlack
#define COL_BTN_DISABLED GColorDarkGray
#define COL_INFO_TEXT    GColorLightGray

// ──────────────────────────────────────────────
// Card rendering
// ──────────────────────────────────────────────

GRect render_card_rect(uint8_t index) {
  return GRect(CARD_X(index), CARD_Y(index), CARD_W, CARD_H);
}

void render_card(GContext *ctx, const Card *card, GRect rect, bool selected, bool empty) {
  // Card background
  graphics_context_set_fill_color(ctx, COL_CARD_BG);
  graphics_fill_rect(ctx, rect, 4, GCornersAll);

  if (empty) {
    // Draw a simple cross pattern for empty slot
    graphics_context_set_stroke_color(ctx, COL_BTN_DISABLED);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_line(ctx, GPoint(rect.origin.x + 4, rect.origin.y + 4),
                           GPoint(rect.origin.x + rect.size.w - 4, rect.origin.y + rect.size.h - 4));
    graphics_draw_line(ctx, GPoint(rect.origin.x + rect.size.w - 4, rect.origin.y + 4),
                           GPoint(rect.origin.x + 4, rect.origin.y + rect.size.h - 4));
    return;
  }

  GColor suit_color = card_is_red(card) ? COL_RED_SUIT : COL_BLACK_SUIT;

  // Card border
  graphics_context_set_stroke_color(ctx, suit_color);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_round_rect(ctx, rect, 4);

  // Selection highlight
  if (selected) {
    graphics_context_set_stroke_color(ctx, COL_SELECTED);
    graphics_context_set_stroke_width(ctx, 3);
    GRect sel = grect_inset(rect, GEdgeInsets(-2, -2, -2, -2));
    graphics_draw_round_rect(ctx, sel, 6);
  }

  // Value text (top-left) — use smaller font for face cards to fit "(12)"
  graphics_context_set_text_color(ctx, suit_color);
  const char *val_str = card_value_str(card);
  bool is_face = (card->value > 10);
  GFont val_font = is_face
    ? fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD)
    : fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GRect val_rect = GRect(rect.origin.x + 4, rect.origin.y + 2, 52, is_face ? 20 : 24);
  graphics_draw_text(ctx, val_str, val_font, val_rect,
                     GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);

  // Suit symbol (center)
  GPoint center = GPoint(rect.origin.x + rect.size.w / 2,
                         rect.origin.y + rect.size.h / 2 + 2);
  render_suit_symbol(ctx, center, 16, card->suit);

  // Small value bottom-right
  GFont small_font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  GRect val_rect2 = GRect(rect.origin.x + 4,
                           rect.origin.y + rect.size.h - 18, rect.size.w - 8, 16);
  graphics_draw_text(ctx, val_str, small_font, val_rect2,
                     GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);

  // Card role indicator (small icon in top-right)
  CardRole role = card_get_role(card);
  const char *role_str = "";
  switch (role) {
    case CARD_POTION: role_str = "+"; break;
    case CARD_WEAPON: role_str = "W"; break;
    case CARD_MONSTER: role_str = "!"; break;
  }
  GRect role_rect = GRect(rect.origin.x + rect.size.w - 16, rect.origin.y + 4, 12, 14);
  graphics_context_set_text_color(ctx, suit_color);
  graphics_draw_text(ctx, role_str, small_font, role_rect,
                     GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
}

// ──────────────────────────────────────────────
// Suit symbols (drawn with primitives)
// ──────────────────────────────────────────────

static void draw_heart(GContext *ctx, GPoint center, uint8_t size) {
  // Heart: two circles on top + triangle pointing down
  int8_t r = size / 4;
  int8_t offset = r;

  // Left circle
  graphics_fill_circle(ctx, GPoint(center.x - offset, center.y - offset / 2), r);
  // Right circle
  graphics_fill_circle(ctx, GPoint(center.x + offset, center.y - offset / 2), r);
  // Bottom triangle
  GPathInfo info = {
    .num_points = 3,
    .points = (GPoint[]) {
      { center.x - size / 2, center.y - offset / 2 },
      { center.x + size / 2, center.y - offset / 2 },
      { center.x, center.y + size / 2 }
    }
  };
  GPath *path = gpath_create(&info);
  gpath_draw_filled(ctx, path);
  gpath_destroy(path);
}

static void draw_diamond(GContext *ctx, GPoint center, uint8_t size) {
  GPathInfo info = {
    .num_points = 4,
    .points = (GPoint[]) {
      { center.x, center.y - size / 2 },
      { center.x + size / 3, center.y },
      { center.x, center.y + size / 2 },
      { center.x - size / 3, center.y }
    }
  };
  GPath *path = gpath_create(&info);
  gpath_draw_filled(ctx, path);
  gpath_destroy(path);
}

static void draw_spade(GContext *ctx, GPoint center, uint8_t size) {
  // Spade: inverted heart + stem
  int8_t r = size / 4;
  int8_t offset = r;

  // Left circle (lower)
  graphics_fill_circle(ctx, GPoint(center.x - offset, center.y + offset / 2), r);
  // Right circle (lower)
  graphics_fill_circle(ctx, GPoint(center.x + offset, center.y + offset / 2), r);
  // Top triangle
  GPathInfo info = {
    .num_points = 3,
    .points = (GPoint[]) {
      { center.x - size / 2, center.y + offset / 2 },
      { center.x + size / 2, center.y + offset / 2 },
      { center.x, center.y - size / 2 }
    }
  };
  GPath *path = gpath_create(&info);
  gpath_draw_filled(ctx, path);
  gpath_destroy(path);

  // Stem
  GRect stem = GRect(center.x - 2, center.y + offset, 4, size / 3);
  graphics_fill_rect(ctx, stem, 0, GCornerNone);
}

static void draw_club(GContext *ctx, GPoint center, uint8_t size) {
  int8_t r = size / 4;

  // Three circles
  graphics_fill_circle(ctx, GPoint(center.x, center.y - r), r);
  graphics_fill_circle(ctx, GPoint(center.x - r, center.y + r / 2), r);
  graphics_fill_circle(ctx, GPoint(center.x + r, center.y + r / 2), r);

  // Stem
  GRect stem = GRect(center.x - 2, center.y + r / 2, 4, size / 3);
  graphics_fill_rect(ctx, stem, 0, GCornerNone);
}

void render_suit_symbol(GContext *ctx, GPoint center, uint8_t size, Suit suit) {
  GColor color = (suit == SUIT_HEARTS || suit == SUIT_DIAMONDS)
                 ? COL_RED_SUIT : GColorBlack;
  graphics_context_set_fill_color(ctx, color);

  switch (suit) {
    case SUIT_HEARTS:  draw_heart(ctx, center, size); break;
    case SUIT_DIAMONDS: draw_diamond(ctx, center, size); break;
    case SUIT_SPADES:  draw_spade(ctx, center, size); break;
    case SUIT_CLUBS:   draw_club(ctx, center, size); break;
    default: break;
  }
}

// ──────────────────────────────────────────────
// HP bar
// ──────────────────────────────────────────────

void render_hp_bar(GContext *ctx, uint8_t hp, uint8_t max_hp) {
  GRect bar_rect = GRect(CARD_MARGIN, HP_BAR_Y, SCREEN_W - 2 * CARD_MARGIN, HP_BAR_H);

  // Background
  graphics_context_set_fill_color(ctx, COL_HP_BG);
  graphics_fill_rect(ctx, bar_rect, 2, GCornersAll);

  // Health fill
  uint16_t fill_w = (uint16_t)(bar_rect.size.w * hp / max_hp);
  if (fill_w > 0) {
    GRect fill = GRect(bar_rect.origin.x, bar_rect.origin.y, fill_w, bar_rect.size.h);
    graphics_context_set_fill_color(ctx,
      hp > max_hp / 3 ? COL_HP_HEALTHY : COL_HP_LOW);
    graphics_fill_rect(ctx, fill, 2, GCornersAll);
  }
}

// ──────────────────────────────────────────────
// Weapon status
// ──────────────────────────────────────────────

void render_weapon_status(GContext *ctx, const WeaponState *weapon) {
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_14);

  if (!weapon->equipped) {
    graphics_context_set_text_color(ctx, COL_BTN_DISABLED);
    GRect r = GRect(SCREEN_W - 80, 3, 76, 16);
    graphics_draw_text(ctx, "No weapon", font, r,
                       GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
    return;
  }

  // Weapon value and degradation indicator
  static char buf[16];
  if (weapon->max_monster == WEAPON_ANY) {
    snprintf(buf, sizeof(buf), "W:%d", weapon->value);
  } else {
    snprintf(buf, sizeof(buf), "W:%d<%d", weapon->value, weapon->max_monster);
  }

  graphics_context_set_text_color(ctx, COL_WEAPON);
  GRect r = GRect(SCREEN_W - 80, 3, 76, 16);
  graphics_draw_text(ctx, buf, font, r,
                     GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
}

// ──────────────────────────────────────────────
// HP text (top-left)
// ──────────────────────────────────────────────

// Used by game_screen — declared here for consistency
void render_hp_text(GContext *ctx, uint8_t hp) {
  static char buf[8];
  snprintf(buf, sizeof(buf), "HP:%d", hp);
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  graphics_context_set_text_color(ctx, hp > MAX_HP / 3 ? COL_HP_HEALTHY : COL_HP_LOW);
  graphics_draw_text(ctx, buf, font, GRect(CARD_MARGIN, 3, 60, 16),
                     GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
}

// ──────────────────────────────────────────────
// Action buttons
// ──────────────────────────────────────────────

void render_action_rects(GRect rects[3]) {
  int total_w = 3 * ACTION_BTN_W + 2 * ACTION_GAP;
  int start_x = (SCREEN_W - total_w) / 2;
  for (int i = 0; i < 3; i++) {
    rects[i] = GRect(start_x + i * (ACTION_BTN_W + ACTION_GAP), ACTION_Y, ACTION_BTN_W, ACTION_H);
  }
}

void render_action_button(GContext *ctx, GRect rect, const char *label,
                          bool enabled, bool highlighted) {
  if (enabled) {
    graphics_context_set_fill_color(ctx, highlighted ? COL_SELECTED : COL_BTN_BG);
    graphics_context_set_stroke_color(ctx, highlighted ? COL_SELECTED : COL_BTN_BG);
  } else {
    graphics_context_set_fill_color(ctx, COL_BTN_DISABLED);
    graphics_context_set_stroke_color(ctx, COL_BTN_DISABLED);
  }

  graphics_fill_rect(ctx, rect, 4, GCornersAll);

  graphics_context_set_text_color(ctx, enabled ? COL_BTN_TEXT : GColorDarkGray);
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  graphics_draw_text(ctx, label, font, rect,
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

// ──────────────────────────────────────────────
// Info bar
// ──────────────────────────────────────────────

void render_info_bar(GContext *ctx, uint8_t dungeon_remaining, uint8_t cards_resolved,
                     bool potion_used, bool can_avoid) {
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  graphics_context_set_text_color(ctx, COL_INFO_TEXT);

  static char left_buf[20];
  static char right_buf[20];

  snprintf(left_buf, sizeof(left_buf), "Deck:%d", dungeon_remaining);
  snprintf(right_buf, sizeof(right_buf), "%d/3 %s%s",
           cards_resolved,
           potion_used ? "+" : "",
           can_avoid ? " [X]" : "");

  GRect left_r = GRect(CARD_MARGIN, INFO_Y, 80, INFO_H);
  GRect right_r = GRect(SCREEN_W - 100, INFO_Y, 96, INFO_H);

  graphics_draw_text(ctx, left_buf, font, left_r,
                     GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, right_buf, font, right_r,
                     GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
}

// ──────────────────────────────────────────────
// Combat flash overlay
// ──────────────────────────────────────────────

void render_combat_flash(GContext *ctx, int16_t damage) {
  // Semi-transparent red overlay
  graphics_context_set_fill_color(ctx, (GColor){ .argb = 0b11000011 }); // ~50% red
  graphics_fill_rect(ctx, GRect(0, 0, SCREEN_W, SCREEN_H), 0, GCornerNone);

  // Damage number in center
  static char buf[16];
  snprintf(buf, sizeof(buf), "-%d", damage);
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, buf, font,
                     GRect(0, SCREEN_H / 2 - 20, SCREEN_W, 30),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

// ──────────────────────────────────────────────
// Avoid room button
// ──────────────────────────────────────────────

void render_avoid_button(GContext *ctx, bool enabled) {
  GRect rect = GRect((SCREEN_W - ACTION_BTN_W) / 2, ACTION_Y, ACTION_BTN_W, ACTION_H);
  render_action_button(ctx, rect, "Avoid", enabled, false);
}
