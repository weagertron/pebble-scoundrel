#include "game_screen.h"
#include "../render.h"
#include "../game.h"
#include <stdio.h>

#ifdef PBL_TOUCH
#include <pebble.h>
#endif

// ──────────────────────────────────────────────
// Static state
// ──────────────────────────────────────────────

static Window *s_window = NULL;
static Layer *s_canvas = NULL;
static GameScreenCallback s_callback = NULL;

// Navigation
static int s_card_cursor = 0;    // highlighted card (0-3)
static int s_btn_cursor = 0;     // highlighted action button

// Action buttons for current card
#define MAX_BUTTONS 3
static ActionType s_actions[MAX_BUTTONS];
static int s_action_count = 0;

// Timers
static AppTimer *s_combat_timer = NULL;
static AppTimer *s_room_timer = NULL;
static AppTimer *s_backlight_timer = NULL;
static int16_t s_last_damage = 0;

// Backlight keep-alive: re-trigger the standard auto-off interval
#define BACKLIGHT_INTERVAL_MS 5000

extern GameState g_game;

// ──────────────────────────────────────────────
// Forward declarations
// ──────────────────────────────────────────────

static void setup_actions(void);
static void do_action(ActionType action);
static void after_resolve(void);
static void combat_timer_cb(void *ctx);
static void room_timer_cb(void *ctx);
static void game_over_check(void);
static void backlight_timer_cb(void *ctx);
static void backlight_kick(void);

// ──────────────────────────────────────────────
// Helpers
// ──────────────────────────────────────────────

static const char *action_name(ActionType a) {
  switch (a) {
    case ACTION_FIGHT_WEAPON: return "Fight";
    case ACTION_FIGHT_BARE:   return "Bare";
    case ACTION_EQUIP:        return "Equip";
    case ACTION_DRINK:        return "Drink";
    case ACTION_SKIP_POTION:  return "Skip";
    default:                  return "?";
  }
}

static void first_active_card(void) {
  for (int i = 0; i < ROOM_SIZE; i++) {
    if (g_game.room_active[i]) { s_card_cursor = i; return; }
  }
  s_card_cursor = 0;
}

// ──────────────────────────────────────────────
// Backlight keep-alive
// ──────────────────────────────────────────────

static void backlight_timer_cb(void *ctx) {
  s_backlight_timer = NULL;
  light_enable_interaction();
  s_backlight_timer = app_timer_register(BACKLIGHT_INTERVAL_MS, backlight_timer_cb, NULL);
}

static void backlight_kick(void) {
  // Re-trigger the backlight and restart the keep-alive timer
  light_enable_interaction();
  if (s_backlight_timer) app_timer_cancel(s_backlight_timer);
  s_backlight_timer = app_timer_register(BACKLIGHT_INTERVAL_MS, backlight_timer_cb, NULL);
}

static void backlight_stop(void) {
  if (s_backlight_timer) {
    app_timer_cancel(s_backlight_timer);
    s_backlight_timer = NULL;
  }
  // Return to automatic backlight control
  light_enable(false);
}

static void setup_actions(void) {
  s_action_count = 0;
  s_btn_cursor = 0;
  uint8_t avail = game_get_available_actions(&g_game);

  // Order: fight weapon, fight bare, equip, drink, skip
  if (avail & (1 << ACTION_FIGHT_WEAPON))
    s_actions[s_action_count++] = ACTION_FIGHT_WEAPON;
  if (avail & (1 << ACTION_FIGHT_BARE))
    s_actions[s_action_count++] = ACTION_FIGHT_BARE;
  if (avail & (1 << ACTION_EQUIP))
    s_actions[s_action_count++] = ACTION_EQUIP;
  if (avail & (1 << ACTION_DRINK))
    s_actions[s_action_count++] = ACTION_DRINK;
  if (avail & (1 << ACTION_SKIP_POTION))
    s_actions[s_action_count++] = ACTION_SKIP_POTION;
}

// ──────────────────────────────────────────────
// Drawing
// ──────────────────────────────────────────────

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  // Background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // HP text + weapon status
  render_hp_text(ctx, g_game.hp);
  render_weapon_status(ctx, &g_game.weapon);
  render_hp_bar(ctx, g_game.hp, MAX_HP);

  // Cards — show cursor highlight during PHASE_ROOM_DEALT / PHASE_SELECT_CARD
  for (int i = 0; i < ROOM_SIZE; i++) {
    GRect r = render_card_rect(i);
    bool empty = !g_game.room_active[i];
    bool selected = false;

    if (g_game.phase == PHASE_RESOLVE_ACTION) {
      selected = (game_selected_index(&g_game) == i);
    } else if (g_game.phase == PHASE_ROOM_DEALT ||
               g_game.phase == PHASE_SELECT_CARD) {
      selected = (i == s_card_cursor) && !empty;
    }

    render_card(ctx, &g_game.room[i], r, selected, empty);
  }

  // Combat flash overlay
  if (g_game.phase == PHASE_COMBAT_RESULT) {
    render_combat_flash(ctx, s_last_damage);
    return;
  }

  // Action area
  GFont font14 = fonts_get_system_font(FONT_KEY_GOTHIC_14);

  switch (g_game.phase) {
    case PHASE_ROOM_DEALT: {
      // Show avoid button + prompt
      bool can_avoid = !g_game.avoided_last;
      render_avoid_button(ctx, can_avoid);
      graphics_context_set_text_color(ctx, GColorLightGray);
      #ifdef PBL_TOUCH
      graphics_draw_text(ctx, "Tap card or Avoid",
                         font14, GRect(4, ACTION_Y + ACTION_H + 2, SCREEN_W - 8, 16),
                         GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
      #else
      graphics_draw_text(ctx, "UP/DN: card  SEL: pick  BK: avoid",
                         font14, GRect(4, ACTION_Y + ACTION_H + 2, SCREEN_W - 8, 16),
                         GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
      #endif
      break;
    }

    case PHASE_SELECT_CARD: {
      static char buf[20];
      snprintf(buf, sizeof(buf), "Select card (%d/3)", g_game.cards_resolved);
      graphics_context_set_text_color(ctx, GColorLightGray);
      graphics_draw_text(ctx, buf, font14,
                         GRect(0, ACTION_Y + 8, SCREEN_W, 20),
                         GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
      break;
    }

    case PHASE_RESOLVE_ACTION: {
      // Draw action buttons
      GRect btn_rects[3];
      render_action_rects(btn_rects);
      for (int i = 0; i < s_action_count; i++) {
        render_action_button(ctx, btn_rects[i], action_name(s_actions[i]),
                             true, i == s_btn_cursor);
      }
      break;
    }

    case PHASE_ROOM_COMPLETE: {
      GFont font18 = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
      graphics_context_set_text_color(ctx, GColorChromeYellow);
      graphics_draw_text(ctx, "Room clear!", font18,
                         GRect(0, ACTION_Y + 4, SCREEN_W, ACTION_H),
                         GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
      break;
    }

    default:
      break;
  }

  // Info bar
  render_info_bar(ctx, game_dungeon_remaining(&g_game),
                  g_game.cards_resolved, g_game.potion_used,
                  !g_game.avoided_last);
}

// ──────────────────────────────────────────────
// Action execution
// ──────────────────────────────────────────────

static void do_action(ActionType action) {
  bool combat = (action == ACTION_FIGHT_WEAPON || action == ACTION_FIGHT_BARE);

  if (combat) {
    s_last_damage = game_calc_combat_damage(&g_game,
                       action == ACTION_FIGHT_WEAPON);
    game_resolve(&g_game, action);

    if (s_last_damage > 0) vibes_short_pulse();
    else vibes_double_pulse();

    layer_mark_dirty(s_canvas);
    s_combat_timer = app_timer_register(600, combat_timer_cb, NULL);
  } else {
    game_resolve(&g_game, action);
    if (action == ACTION_EQUIP) vibes_short_pulse();
    after_resolve();
  }
}

static void after_resolve(void) {
  layer_mark_dirty(s_canvas);
  game_over_check();

  if (g_game.phase == PHASE_ROOM_COMPLETE) {
    s_room_timer = app_timer_register(800, room_timer_cb, NULL);
  } else if (g_game.phase == PHASE_SELECT_CARD) {
    first_active_card();
  }
}

static void combat_timer_cb(void *ctx) {
  s_combat_timer = NULL;
  game_finish_combat(&g_game);
  after_resolve();
}

static void room_timer_cb(void *ctx) {
  s_room_timer = NULL;
  game_deal_room(&g_game);

  if (g_game.phase == PHASE_VICTORY) {
    game_calculate_score(&g_game);
    // Small delay then show game over
    app_timer_register(500, (AppTimerCallback)(void*)game_over_check, NULL);
    return;
  }

  first_active_card();
  layer_mark_dirty(s_canvas);
}

static void game_over_check(void) {
  if (g_game.hp == 0 || g_game.phase == PHASE_VICTORY ||
      g_game.phase == PHASE_GAME_OVER) {
    if (g_game.phase != PHASE_GAME_OVER) {
      game_calculate_score(&g_game);
    }
    if (s_callback) s_callback(SCREEN_GAME_OVER);
  }
}

// ──────────────────────────────────────────────
// Button handling
// ──────────────────────────────────────────────

static void up_click(ClickRecognizerRef rec, void *ctx) {
  backlight_kick();
  switch (g_game.phase) {
    case PHASE_ROOM_DEALT:
    case PHASE_SELECT_CARD:
      // Move card cursor to previous active card
      for (int i = 1; i <= ROOM_SIZE; i++) {
        int idx = (s_card_cursor - i + ROOM_SIZE) % ROOM_SIZE;
        if (g_game.room_active[idx]) {
          s_card_cursor = idx;
          break;
        }
      }
      break;

    case PHASE_RESOLVE_ACTION:
      if (s_action_count > 0) {
        s_btn_cursor = (s_btn_cursor - 1 + s_action_count) % s_action_count;
      }
      break;

    default:
      break;
  }
  layer_mark_dirty(s_canvas);
}

static void down_click(ClickRecognizerRef rec, void *ctx) {
  backlight_kick();
  switch (g_game.phase) {
    case PHASE_ROOM_DEALT:
    case PHASE_SELECT_CARD:
      for (int i = 1; i <= ROOM_SIZE; i++) {
        int idx = (s_card_cursor + i) % ROOM_SIZE;
        if (g_game.room_active[idx]) {
          s_card_cursor = idx;
          break;
        }
      }
      break;

    case PHASE_RESOLVE_ACTION:
      if (s_action_count > 0) {
        s_btn_cursor = (s_btn_cursor + 1) % s_action_count;
      }
      break;

    default:
      break;
  }
  layer_mark_dirty(s_canvas);
}

static void select_click(ClickRecognizerRef rec, void *ctx) {
  backlight_kick();
  switch (g_game.phase) {
    case PHASE_ROOM_DEALT:
    case PHASE_SELECT_CARD:
      // Select highlighted card
      if (g_game.room_active[s_card_cursor]) {
        if (game_select_card(&g_game, s_card_cursor)) {
          g_game.phase = PHASE_RESOLVE_ACTION;
          setup_actions();
        }
      }
      break;

    case PHASE_RESOLVE_ACTION:
      if (s_action_count > 0 && s_btn_cursor < s_action_count) {
        do_action(s_actions[s_btn_cursor]);
      }
      break;

    default:
      break;
  }
  layer_mark_dirty(s_canvas);
}

static void back_click(ClickRecognizerRef rec, void *ctx) {
  backlight_kick();
  switch (g_game.phase) {
    case PHASE_RESOLVE_ACTION:
      game_deselect(&g_game);
      // phase is now PHASE_SELECT_CARD
      break;

    case PHASE_ROOM_DEALT:
      // Avoid room
      if (!g_game.avoided_last && g_game.cards_resolved == 0) {
        if (game_avoid_room(&g_game)) {
          vibes_short_pulse();
          game_over_check();
          first_active_card();
        }
      }
      break;

    default:
      // Go back to title
      if (s_callback) s_callback(SCREEN_TITLE);
      break;
  }
  layer_mark_dirty(s_canvas);
}

static void click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_click_subscribe(BUTTON_ID_BACK, back_click);
}

// ──────────────────────────────────────────────
// Window handlers
// ──────────────────────────────────────────────

// ──────────────────────────────────────────────
// Touch handling (Emery / PBL_TOUCH)
// ──────────────────────────────────────────────

#ifdef PBL_TOUCH

static bool s_touch_active = false;  // true while finger is down

// Simple tap detection: touchdown position must be close to liftoff position
#define TAP_MAX_DRIFT 20

static void touch_handler(const TouchEvent *event, void *context) {
  static int16_t s_down_x = 0, s_down_y = 0;

  switch (event->type) {
    case TouchEvent_Touchdown:
      s_touch_active = true;
      s_down_x = event->x;
      s_down_y = event->y;
      backlight_kick();
      break;

    case TouchEvent_Liftoff: {
      if (!s_touch_active) break;
      s_touch_active = false;

      int16_t dx = event->x - s_down_x;
      int16_t dy = event->y - s_down_y;
      if (dx * dx + dy * dy > TAP_MAX_DRIFT * TAP_MAX_DRIFT) break;  // was a drag, not a tap

      int16_t tx = s_down_x;
      int16_t ty = s_down_y;

      switch (g_game.phase) {
        case PHASE_ROOM_DEALT:
        case PHASE_SELECT_CARD: {
          // Check if a card was tapped
          for (int i = 0; i < ROOM_SIZE; i++) {
            if (!g_game.room_active[i]) continue;
            GRect r = render_card_rect(i);
            if (tx >= r.origin.x && tx < r.origin.x + r.size.w &&
                ty >= r.origin.y && ty < r.origin.y + r.size.h) {
              s_card_cursor = i;
              if (game_select_card(&g_game, i)) {
                g_game.phase = PHASE_RESOLVE_ACTION;
                setup_actions();
              }
              layer_mark_dirty(s_canvas);
              return;
            }
          }

          // Check if the avoid button was tapped (only during PHASE_ROOM_DEALT)
          if (g_game.phase == PHASE_ROOM_DEALT &&
              !g_game.avoided_last && g_game.cards_resolved == 0) {
            GRect avoid_r = GRect((SCREEN_W - ACTION_BTN_W) / 2, ACTION_Y, ACTION_BTN_W, ACTION_H);
            if (tx >= avoid_r.origin.x && tx < avoid_r.origin.x + avoid_r.size.w &&
                ty >= avoid_r.origin.y && ty < avoid_r.origin.y + avoid_r.size.h) {
              if (game_avoid_room(&g_game)) {
                vibes_short_pulse();
                game_over_check();
                first_active_card();
              }
              layer_mark_dirty(s_canvas);
              return;
            }
          }
          break;
        }

        case PHASE_RESOLVE_ACTION: {
          // Check if an action button was tapped
          GRect btn_rects[3];
          render_action_rects(btn_rects);
          for (int i = 0; i < s_action_count; i++) {
            GRect r = btn_rects[i];
            if (tx >= r.origin.x && tx < r.origin.x + r.size.w &&
                ty >= r.origin.y && ty < r.origin.y + r.size.h) {
              do_action(s_actions[i]);
              return;  // do_action may trigger timers, don't touch canvas further
            }
          }

          // Tap elsewhere = deselect (go back to card selection)
          game_deselect(&g_game);
          layer_mark_dirty(s_canvas);
          break;
        }

        default:
          break;
      }
      break;
    }

    default:
      break;
  }
}

static void touch_subscribe(void) {
  if (touch_service_is_enabled()) {
    touch_service_subscribe(touch_handler, NULL);
  }
}

static void touch_unsubscribe(void) {
  touch_service_unsubscribe();
}

#endif // PBL_TOUCH

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, canvas_update_proc);
  layer_add_child(root, s_canvas);
  first_active_card();

  // Start backlight keep-alive
  backlight_kick();

  #ifdef PBL_TOUCH
  touch_subscribe();
  #endif
}

static void window_unload(Window *window) {
  backlight_stop();
  #ifdef PBL_TOUCH
  touch_unsubscribe();
  #endif
  if (s_combat_timer) { app_timer_cancel(s_combat_timer); s_combat_timer = NULL; }
  if (s_room_timer)   { app_timer_cancel(s_room_timer);   s_room_timer = NULL; }
  layer_destroy(s_canvas);
  s_canvas = NULL;
  window_destroy(s_window);
  s_window = NULL;
}

// ──────────────────────────────────────────────
// Public API
// ──────────────────────────────────────────────

void game_screen_window_push(GameScreenCallback callback) {
  if (s_window) return;
  s_callback = callback;
  s_card_cursor = 0;
  s_btn_cursor = 0;

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload
  });
  window_set_click_config_provider(s_window, click_config);
  window_set_background_color(s_window, GColorBlack);
  window_stack_push(s_window, true);
}

void game_screen_window_pop(void) {
  if (s_window) window_stack_remove(s_window, true);
}

void game_screen_refresh(void) {
  if (s_canvas) layer_mark_dirty(s_canvas);
}
