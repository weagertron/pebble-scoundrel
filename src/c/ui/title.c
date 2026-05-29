#include "title.h"
#include "../render.h"
#include <stdio.h>

// ──────────────────────────────────────────────
// Static state
// ──────────────────────────────────────────────

static Window *s_window = NULL;
static Layer *s_canvas = NULL;
static TitleCallback s_callback = NULL;
static int s_selected = 0;
static bool s_has_save = false;
static bool s_last_continue = false;

#define MENU_NEW_GAME    0
#define MENU_CONTINUE    1
#define MENU_RULES       2
#define MENU_COUNT       3

static const char *MENU_LABELS[] = {"New Game", "Continue", "Rules"};

// Layout
#define TITLE_Y         30
#define TITLE_TEXT_H    30
#define SYMBOLS_Y       (TITLE_Y + TITLE_TEXT_H + 14)
#define SUBTITLE_Y      (SYMBOLS_Y + 14)
#define MENU_START_Y    (SUBTITLE_Y + 28)
#define MENU_H          32
#define MENU_MARGIN     16

// ──────────────────────────────────────────────
// Canvas drawing
static void canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  // Background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Title
  GFont title_font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, "SCOUNDREL", title_font,
                     GRect(0, TITLE_Y, SCREEN_W, TITLE_TEXT_H),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

  // Decorative suit symbols under title
  render_suit_symbol(ctx, GPoint(SCREEN_W / 2 - 30, SYMBOLS_Y), 8, SUIT_SPADES);
  render_suit_symbol(ctx, GPoint(SCREEN_W / 2 - 10, SYMBOLS_Y), 8, SUIT_HEARTS);
  render_suit_symbol(ctx, GPoint(SCREEN_W / 2 + 10, SYMBOLS_Y), 8, SUIT_DIAMONDS);
  render_suit_symbol(ctx, GPoint(SCREEN_W / 2 + 30, SYMBOLS_Y), 8, SUIT_CLUBS);

  // Subtitle
  GFont sub_font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  graphics_context_set_text_color(ctx, GColorLightGray);
  graphics_draw_text(ctx, "A dungeon crawl card game", sub_font,
                     GRect(0, SUBTITLE_Y, SCREEN_W, 18),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

  // Menu items
  int visible_count = s_has_save ? MENU_COUNT : MENU_COUNT - 1;
  int menu_start = MENU_START_Y;

  for (int i = 0; i < visible_count; i++) {
    // Map visual index to menu item index
    int menu_idx;
    if (s_has_save) {
      menu_idx = i; // 0=New, 1=Continue, 2=Rules
    } else {
      menu_idx = (i == 0) ? MENU_NEW_GAME : MENU_RULES;
    }

    GRect item_rect = GRect(MENU_MARGIN, menu_start + i * (MENU_H + 6),
                            SCREEN_W - 2 * MENU_MARGIN, MENU_H);
    bool selected = (i == s_selected);

    graphics_context_set_fill_color(ctx, selected ? GColorWhite : GColorDarkGray);
    graphics_fill_rect(ctx, item_rect, 4, GCornersAll);

    graphics_context_set_text_color(ctx, selected ? GColorBlack : GColorWhite);
    GFont menu_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    graphics_draw_text(ctx, MENU_LABELS[menu_idx], menu_font, item_rect,
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  }
}

// ──────────────────────────────────────────────
// Button handling
// ──────────────────────────────────────────────

static void up_handler(ClickRecognizerRef recognizer, void *context) {
  int visible = s_has_save ? MENU_COUNT : MENU_COUNT - 1;
  s_selected = (s_selected - 1 + visible) % visible;
  layer_mark_dirty(s_canvas);
}

static void down_handler(ClickRecognizerRef recognizer, void *context) {
  int visible = s_has_save ? MENU_COUNT : MENU_COUNT - 1;
  s_selected = (s_selected + 1) % visible;
  layer_mark_dirty(s_canvas);
}

static void select_handler(ClickRecognizerRef recognizer, void *context) {
  if (!s_callback) return;

  int menu_idx;
  if (s_has_save) {
    menu_idx = s_selected;
  } else {
    // Map: 0 → New Game, 1 → Rules
    if (s_selected == 0) menu_idx = MENU_NEW_GAME;
    else menu_idx = MENU_RULES;
  }
  switch (menu_idx) {
    case MENU_NEW_GAME:  s_last_continue = false; s_callback(SCREEN_GAME); break;
    case MENU_CONTINUE:  s_last_continue = true;  s_callback(SCREEN_GAME); break;
    case MENU_RULES:     s_callback(SCREEN_RULES); break;
  }
}

static void back_handler(ClickRecognizerRef recognizer, void *context) {
  window_stack_pop_all(true);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, back_handler);
}

// ──────────────────────────────────────────────
// Window handlers
// ──────────────────────────────────────────────

static void window_load_handler(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_canvas = layer_create(bounds);
  layer_set_update_proc(s_canvas, canvas_update_proc);
  layer_add_child(root, s_canvas);
}

static void window_unload_handler(Window *window) {
  layer_destroy(s_canvas);
  s_canvas = NULL;
  window_destroy(s_window);
  s_window = NULL;
}

// ──────────────────────────────────────────────
// Public API
// ──────────────────────────────────────────────

void title_window_push(TitleCallback callback, bool has_save) {
  if (s_window) return;

  s_callback = callback;
  s_has_save = has_save;
  s_selected = 0;

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load_handler,
    .unload = window_unload_handler
  });
  window_set_click_config_provider(s_window, click_config_provider);
  window_set_background_color(s_window, GColorBlack);
  window_stack_push(s_window, true);
}

void title_window_pop(void) {
  if (s_window) {
    window_stack_remove(s_window, true);
  }
}

bool title_last_was_continue(void) {
  return s_last_continue;
}
