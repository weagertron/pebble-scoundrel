#include "rules.h"

// ──────────────────────────────────────────────
// Static state
// ──────────────────────────────────────────────

static Window *s_window = NULL;
static ScrollLayer *s_scroll = NULL;
static TextLayer *s_text = NULL;
static RulesCallback s_callback = NULL;

static const char *RULES_TEXT =
  "SCOUNDREL RULES\n\n"
  "A solo dungeon-crawl card game by Zach Gage & Kurt Bieg.\n\n"
  "DECK (44 cards)\n"
  "Remove jokers, red face cards, and red aces from a standard deck.\n\n"
  "♥ Hearts 2-10: Health Potions\n"
  "♦ Diamonds 2-10: Weapons\n"
  "♠ Spades 2-A: Monsters\n"
  "♣ Clubs 2-A: Monsters\n\n"
  "SETUP\n"
  "Start with 20 HP (max 20). Shuffle deck = the Dungeon.\n\n"
  "GAMEPLAY\n"
  "Deal 4 cards face-up = a Room.\n\n"
  "You may avoid a Room (put cards at bottom of dungeon). You cannot avoid two rooms in a row.\n\n"
  "If you face the Room: resolve 3 of the 4 cards, one at a time. The 4th carries over to the next Room.\n\n"
  "CARD TYPES\n\n"
  "Potion (♥): Heal by card value. Max 1 potion per Room. Second is wasted.\n\n"
  "Weapon (♦): Must equip. Replaces previous weapon. Fresh weapon hits any monster.\n\n"
  "Monster (♠/♣): Fight barehanded (take full damage) or with weapon.\n"
  "  With weapon: take (monster - weapon) damage, minimum 0.\n"
  "  Weapon degradation: after killing a monster of value X, weapon can only hit monsters < X.\n"
  "  You may fight barehanded even with a weapon equipped.\n\n"
  "SCORING\n"
  "Survive = your HP is your score. Die = subtract remaining monsters from 0.";

// ──────────────────────────────────────────────
// Window handlers
// ──────────────────────────────────────────────

static void back_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_callback) s_callback(SCREEN_TITLE);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_BACK, back_handler);
  // ScrollLayer handles UP/DOWN for scrolling
}

static void window_load_handler(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_scroll = scroll_layer_create(bounds);
  scroll_layer_set_click_config_onto_window(s_scroll, window);

  // Add back button handler on top of scroll layer
  window_set_click_config_provider(window, click_config_provider);

  GSize max_size = GRect(0, 0, bounds.size.w, 2000).size;
  s_text = text_layer_create(GRect(4, 4, bounds.size.w - 8, max_size.h));
  text_layer_set_font(s_text, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text(s_text, RULES_TEXT);
  text_layer_set_text_color(s_text, GColorWhite);
  text_layer_set_background_color(s_text, GColorClear);
  text_layer_set_overflow_mode(s_text, GTextOverflowModeWordWrap);

  GSize text_size = text_layer_get_content_size(s_text);
  text_layer_set_size(s_text, text_size);
  scroll_layer_set_content_size(s_scroll, GSize(bounds.size.w, text_size.h + 8));

  scroll_layer_add_child(s_scroll, text_layer_get_layer(s_text));
  layer_add_child(root, scroll_layer_get_layer(s_scroll));
}

static void window_unload_handler(Window *window) {
  text_layer_destroy(s_text);
  s_text = NULL;
  scroll_layer_destroy(s_scroll);
  s_scroll = NULL;
  window_destroy(s_window);
  s_window = NULL;
}

// ──────────────────────────────────────────────
// Public API
// ──────────────────────────────────────────────

void rules_window_push(RulesCallback callback) {
  if (s_window) return;

  s_callback = callback;

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load_handler,
    .unload = window_unload_handler
  });
  window_set_background_color(s_window, GColorBlack);
  window_stack_push(s_window, true);
}

void rules_window_pop(void) {
  if (s_window) {
    window_stack_remove(s_window, true);
  }
}
