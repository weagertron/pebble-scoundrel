#include <pebble.h>
#include "core_types.h"
#include "deck.h"
#include "game.h"
#include "persistence.h"
#include "ui/title.h"
#include "ui/game_screen.h"
#include "ui/rules.h"
#include "ui/game_over.h"

// ──────────────────────────────────────────────
// Global game state
// ──────────────────────────────────────────────

GameState g_game;

// ──────────────────────────────────────────────
// Screen flow
// ──────────────────────────────────────────────

static void show_title(void);
static void start_game(bool continue_game);
static void show_rules(void);
static void show_game_over(void);

static void on_title_action(GameScreen screen) {
  switch (screen) {
    case SCREEN_GAME:
      start_game(title_last_was_continue());
      break;
    case SCREEN_RULES:
      show_rules();
      break;
    default:
      break;
  }
}

static void on_rules_done(GameScreen screen) {
  show_title();
}

static void on_game_action(GameScreen screen) {
  switch (screen) {
    case SCREEN_GAME_OVER:
      show_game_over();
      break;
    case SCREEN_TITLE:
      save_game(&g_game);
      show_title();
      break;
    default:
      break;
  }
}

static void on_game_over_action(GameScreen screen) {
  switch (screen) {
    case SCREEN_GAME:
      start_game(false);
      break;
    case SCREEN_TITLE:
      show_title();
      break;
    default:
      break;
  }
}

// ──────────────────────────────────────────────
// Screen management
// ──────────────────────────────────────────────

static void show_title(void) {
  bool has_save = save_exists();
  title_window_push(on_title_action, has_save);
}

static void start_game(bool continue_game) {
  if (continue_game && save_exists()) {
    if (!load_game(&g_game)) {
      game_init(&g_game);
    }
    // Ensure playable state
    if (g_game.phase == PHASE_COMBAT_RESULT) {
      g_game.phase = PHASE_SELECT_CARD;
    } else if (g_game.phase == PHASE_ROOM_COMPLETE) {
      game_deal_room(&g_game);
    }
  } else {
    game_init(&g_game);
    save_clear();
  }
  game_screen_window_push(on_game_action);
}

static void show_rules(void) {
  rules_window_push(on_rules_done);
}

static void show_game_over(void) {
  bool victory = (g_game.hp > 0);
  int16_t score = game_calculate_score(&g_game);

  int16_t best = load_high_score();
  if (score > best) {
    save_high_score(score);
  }

  save_clear();
  game_over_window_push(on_game_over_action, victory, score, g_game.hp);
}

// ──────────────────────────────────────────────
// Entry point
// ──────────────────────────────────────────────

static void init(void) {
  show_title();
}

static void deinit(void) {
  // Save if game is in progress
  if (g_game.phase > PHASE_TITLE &&
      g_game.phase < PHASE_GAME_OVER) {
    save_game(&g_game);
  }
}

int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}
