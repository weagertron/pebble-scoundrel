# Scoundrel — Pebble Time 2 Implementation Plan

A solo dungeon-crawl card game by Zach Gage & Kurt Bieg, targeting the Pebble Time 2 (Emery: 200×228, 64-color, capacitive touch, 128KB RAM, 512KB resources).

---

## Verified Rules (from original 2011 PDF)

### Deck (44 cards)
Remove from standard 52-card deck: Jokers, red face cards (J♥ Q♥ K♥ J♦ Q♦ K♦), red aces (A♥ A♦). Remaining:

| Suit | Cards | Role | Values |
|------|-------|------|--------|
| ♥ Hearts | 2–10 (9) | Health Potions | 2–10 |
| ♦ Diamonds | 2–10 (9) | Weapons | 2–10 |
| ♠ Spades | A,K,Q,J,10–2 (13) | Monsters | 14,13,12,11,10…2 |
| ♣ Clubs | A,K,Q,J,10–2 (13) | Monsters | 14,13,12,11,10…2 |

### Setup
- Shuffle 44 cards → face-down pile called the **Dungeon**
- Player starts at **20 HP** (max 20)

### Rooms
1. Deal 4 cards face-up = one **Room**
2. Player may **avoid** the room (all 4 cards go to bottom of dungeon). Cannot avoid two rooms in a row.
3. If facing the room: player resolves **3 of the 4 cards**, one at a time, in any order. The 4th card carries over to the next room (3 new cards are dealt on top of it).

### Card Resolution
- **Weapon (♦)**: Must equip. Replaces previous weapon (old weapon + its slain monsters → discard). Binding — you cannot skip a weapon if you choose it.
- **Potion (♥)**: Heal by card value (max 20). **Only 1 potion per room** has effect; second potion in same room is discarded with no effect.
- **Monster (♠/♣)**: Fight barehanded OR with equipped weapon.
  - **Barehanded**: take full monster value as damage. Monster is defeated.
  - **With weapon**: take `(monster_value − weapon_value)` damage, minimum 0. Monster is defeated.
  - **Weapon degradation**: after killing a monster of value X, the weapon can only be used on monsters of **strictly lower** value (< X). The weapon is NOT discarded — it remains for weaker monsters.
  - Player may choose to fight barehanded even with an equipped weapon.

### Win/Lose & Scoring
- **Lose**: HP reaches 0. Score = sum of remaining monster values × (−1), minus HP deficit.
- **Win**: Clear all 44 cards. Score = remaining HP. Bonus: if HP is 20 and last card was a potion, score = 20 + potion value.

---

## Architecture

### File Structure
```
pebble-scoundrel/
├── package.json
├── wscript
├── resources/
│   ├── fonts/
│   │   └── press-start-2p.ttf      # pixel font from roguelike
│   └── images/
│       └── app_icon.png
├── src/c/
│   ├── main.c                       # entry point, screen flow
│   ├── core_types.h                 # all enums, structs, constants
│   ├── deck.c / deck.h             # card creation, shuffle, deal
│   ├── game.c / game.h             # game logic engine
│   ├── persistence.c / persistence.h # save/load via persist API
│   ├── render.c / render.h         # shared drawing helpers (cards, bars, buttons)
│   └── ui/
│       ├── title.c / title.h       # title screen (touch menu)
│       ├── game_screen.c / game_screen.h  # main gameplay (cards + actions)
│       ├── rules.c / rules.h       # how-to-play scrollable text
│       └── game_over.c / game_over.h # death/victory + score
```

### Core Data Types

```c
// Suit
typedef enum { SUIT_HEARTS, SUIT_DIAMONDS, SUIT_SPADES, SUIT_CLUBS } Suit;

// Card role
typedef enum { CARD_POTION, CARD_WEAPON, CARD_MONSTER } CardRole;

// A single card (2 bytes)
typedef struct {
  uint8_t value;   // 2–14 (J=11, Q=12, K=13, A=14)
  uint8_t suit;    // Suit enum
} Card;

// Weapon state (4 bytes)
typedef struct {
  uint8_t value;           // weapon card value (0 = no weapon)
  uint8_t suit;            // weapon card suit
  uint8_t max_monster;     // highest monster value it can still hit (0xFF = any)
  bool equipped;
} WeaponState;

// Game phase
typedef enum {
  PHASE_TITLE,
  PHASE_ROOM_DEALT,       // 4 cards showing, can avoid or start
  PHASE_SELECT_CARD,      // tapping a card to resolve
  PHASE_RESOLVE_ACTION,   // chose a card, picking action (fight/equip/drink)
  PHASE_COMBAT_RESULT,    // brief combat outcome display
  PHASE_ROOM_COMPLETE,    // resolved 3 cards, proceeding to next room
  PHASE_GAME_OVER,
  PHASE_VICTORY
} GamePhase;

// Full game state (~60 bytes)
typedef struct {
  uint8_t hp;
  uint8_t max_hp;
  WeaponState weapon;
  Card dungeon[44];        // shuffled deck
  uint8_t dungeon_count;   // remaining cards in dungeon
  uint8_t dungeon_idx;     // next card to deal
  Card room[4];            // current room cards
  bool room_active[4];     // false = slot empty or carried-over
  uint8_t cards_resolved;  // 0–3 resolved this room
  bool potion_used;        // potion already used this room
  bool avoided_last;       // avoided previous room
  bool game_over;
  bool victory;
  GamePhase phase;
  int16_t score;
} GameState;
```

### Save Data (packed, ~48 bytes)
```c
typedef struct __attribute__((__packed__)) {
  uint8_t hp;
  uint8_t weapon_value;
  uint8_t weapon_suit;
  uint8_t weapon_max_monster;
  uint8_t weapon_equipped;
  uint8_t dungeon_count;
  uint8_t dungeon_idx;
  uint8_t room_values[4];
  uint8_t room_suits[4];
  uint8_t room_active[4];
  uint8_t cards_resolved;
  uint8_t potion_used;
  uint8_t avoided_last;
  uint8_t phase;
  Card dungeon_order[44]; // full deck order for restore
} SaveData;
```

---

## UI Design — Touch-Optimized for Emery (200×228)

### Main Game Screen Layout
```
┌──────────────────────────────────────────┐
│  HP ████████████████░░░░  18/20   ⚔♦7≤12│  Status bar (24px)
├──────────────────────────────────────────┤
│                                          │
│  ┌─────────┐  ┌─────────┐              │
│  │ ♠Q      │  │ ♥5      │              │  Card row 1
│  │  12     │  │         │              │  (88×96 each)
│  │         │  │         │              │
│  │      ♠Q │  │      ♥5 │              │
│  └─────────┘  └─────────┘              │
│                                          │
│  ┌─────────┐  ┌─────────┐              │
│  │ ♦3      │  │ ♣7      │              │  Card row 2
│  │         │  │  7      │              │
│  │         │  │         │              │
│  │      ♦3 │  │      ♣7 │              │
│  └─────────┘  └─────────┘              │
│                                          │
├──────────────────────────────────────────┤
│  [⚔ Fight]  [✋ Bare]  [⏭ Skip]        │  Action bar (40px)
├──────────────────────────────────────────┤
│  Dungeon: 32  │  Room: 2/3  │ ♥ Used   │  Info bar (16px)
└──────────────────────────────────────────┘
```

### Layout Constants
```c
#define SCREEN_W    200
#define SCREEN_H    228

// Status bar
#define STATUS_Y    0
#define STATUS_H    24
#define HP_BAR_Y    26
#define HP_BAR_H    6

// Card grid
#define CARD_W      88
#define CARD_H      96
#define CARD_GAP_X  8   // horizontal gap between cards
#define CARD_GAP_Y  8   // vertical gap between rows
#define GRID_X      8   // left margin
#define GRID_Y      36  // top of card area

// Card positions (2×2 grid)
// Card 0: (8, 36)     Card 1: (104, 36)
// Card 2: (8, 140)    Card 3: (104, 140)

// Action bar
#define ACTION_Y    188
#define ACTION_H    40

// Info bar
#define INFO_Y      216
#define INFO_H      12
```

### Touch Zones
- **4 card zones**: Full card rectangles for tap-to-select
- **3 action buttons**: Bottom action bar, ~64×40 each
- Selected card: highlighted with gold border (GColorChromeYellow)
- Monster card + weapon available: show "Fight ⚔" and "Barehanded" buttons
- Monster card + no weapon: show "Fight" only (full damage)
- Weapon card: auto-equips on tap (confirm with action button)
- Potion card: "Drink" button; if potion already used, show "Used" and auto-skip

### Card Rendering
Cards are drawn programmatically (no bitmap resources needed for cards):
- **Background**: White rounded rect with 4px corner radius
- **Border**: 2px — GColorDarkCandyAppleRed for hearts/diamonds, GColorBlack for spades/clubs
- **Value**: Top-left, system font FONT_KEY_GOTHIC_24_BOLD, colored by suit
- **Suit symbol**: Drawn with graphics primitives (heart/diamond/spade/club shapes)
- **Selected state**: Gold border (GColorChromeYellow), 3px stroke
- **Monster cards**: Add subtle red tint or skull icon to convey danger

### Color Palette
| Element | Color | Purpose |
|---------|-------|---------|
| Background | GColorBlack | Dark theme for e-paper contrast |
| Card face | GColorWhite | High contrast readability |
| Hearts/Diamonds border | GColorDarkCandyAppleRed | Red suit identity |
| Spades/Clubs border | GColorBlack | Black suit identity |
| HP bar (healthy) | GColorIslamicGreen | Health indicator |
| HP bar (damaged) | GColorDarkCandyAppleRed | Low health warning |
| Weapon indicator | GColorChromeYellow | Equipped weapon |
| Selected card | GColorChromeYellow | Touch highlight |
| Action buttons | GColorWhite bg, GColorBlack text | Clear tappable targets |
| Potion used badge | GColorDarkGray | Muted indicator |

---

## Game Flow & Screen Transitions

```
┌─────────┐     ┌──────────────┐     ┌───────────────┐
│  TITLE  │────▶│  ROOM_DEALT  │────▶│  SELECT_CARD  │
│         │     │  (4 cards)   │     │  (tap a card) │
└────▲────┘     └──────┬───────┘     └───────┬───────┘
     │                 │                     │
     │           [Avoid Room]           [Card tapped]
     │                 │                     │
     │                 ▼                     ▼
     │          Deal new room        ┌───────────────┐
     │          (if not 2 in a row)  │ RESOLVE_ACTION│
     │                               │ (action btns) │
     │                               └───────┬───────┘
     │                                       │
     │                              [Monster fought]
     │                                       │
     │                                       ▼
     │                               ┌───────────────┐
     │                               │COMBAT_RESULT  │
     │                               │ (brief flash) │
     │                               └───────┬───────┘
     │                                       │
     │                          [3 cards resolved?]
     │                           │           │
     │                      Yes: │      No:  │
     │                           ▼           ▼
     │                    Deal 3 cards   SELECT_CARD
     │                    (keep 4th)     (pick next)
     │                           │
     │                    [Deck empty?]
     │                     │       │
     │                Yes: │  No:  │
     │                     ▼       ▼
     │              ┌──────────┐  ROOM_DEALT
     │              │ VICTORY  │
     │              └────┬─────┘
     │                   │
     │          [HP <= 0 at any point]
     │                   │
     └──────────── ┌──────────┐
                   │GAME_OVER │
                   └──────────┘
```

---

## Implementation Phases

### Phase 1: Project Scaffold & Core Types
**Files**: `package.json`, `wscript`, `core_types.h`

- Create Pebble project targeting Emery only
- Define all enums, structs, constants
- Layout constants for 200×228 display
- Copy pixel font from roguelike project
- Verify project builds (`pebble build`)

### Phase 2: Game Engine (no UI)
**Files**: `deck.c/h`, `game.c/h`

**deck.c**:
- `deck_create()` — populate 44-card deck with correct composition
- `deck_shuffle()` — xorshift32 seeded from `time_ms()`
- `deck_deal()` — draw next card from dungeon

**game.c**:
- `game_init()` — fresh game state, create & shuffle deck, deal first room
- `game_select_card(index)` — player taps a card slot
- `game_resolve_monster(use_weapon)` — combat resolution, weapon degradation
- `game_equip_weapon()` — bind new weapon, discard old
- `game_drink_potion()` — heal (respect 1-per-room limit)
- `game_avoid_room()` — skip room (enforce no-double-avoid)
- `game_advance_room()` — after 3 resolved, carry over 4th, deal 3 new
- `game_is_over()` — check win/lose
- `game_score()` — calculate final score

All logic testable via `APP_LOG` before any UI exists.

### Phase 3: Rendering Foundation
**Files**: `render.c/h`

- `render_card(ctx, card, rect, selected)` — draw a single card with suit, value, border
- `render_card_back(ctx, rect)` — face-down card (unused slots)
- `render_hp_bar(ctx, hp, max_hp)` — health bar with color gradient
- `render_weapon_indicator(ctx, weapon)` — current weapon display
- `render_action_button(ctx, rect, label, enabled)` — touchable button
- Suit symbol drawing: heart/diamond/spade/club via `GPath` or simple geometry
- Card value strings: "2"–"10", "J", "Q", "K", "A"

### Phase 4: UI Screens
**Files**: `ui/title.c`, `ui/game_screen.c`, `ui/rules.c`, `ui/game_over.c`

**title.c**:
- "SCOUNDREL" title with card suit decorations
- Touch menu: New Game / Continue / Rules
- Continue only shown if save exists

**game_screen.c** (the main screen — largest file):
- Window with single canvas layer (custom `update_proc`)
- All rendering in one `layer_update_proc` for efficiency
- Touch handling via raw tap events on the canvas
- State machine for game phases (PHASE_ROOM_DEALT → PHASE_SELECT_CARD → etc.)
- Context-sensitive action buttons based on selected card
- Animations: card selection flash (brief highlight), combat damage flash (screen tint)
- E-paper: mark dirty only on state change, no continuous animation

**rules.c**:
- Scrollable text with the game rules
- Back button to return to title

**game_over.c**:
- Death: show "DEFEATED" + final score + remaining monster count
- Victory: show "ESCAPED" + final HP + score
- Touch menu: Play Again / Menu

### Phase 5: Persistence & Main Loop
**Files**: `persistence.c/h`, `main.c`

**persistence.c**:
- `save_game(state)` — pack game state to persist storage
- `load_game(state)` — restore from persist storage
- `save_exists()` — check for saved game
- `save_clear()` — delete save

**main.c**:
- `init()` — create game state, load save or init fresh, push title
- `deinit()` — save if in progress, destroy all windows/layers
- Screen flow callbacks (same pattern as roguelike's `on_*_action()` functions)
- `main()` — init, event loop, deinit

### Phase 6: Polish & E-Paper Optimization
- Haptic feedback: `vibes_short_pulse()` on monster hit, `vibes_double_pulse()` on death
- Ghosting management: periodic full-refresh via white flash every N room transitions
- Card selection: subtle scale animation on tap ( PropertyAnimation on card layer frame)
- Combat result: brief red tint overlay (100ms timer, then clear)
- Weapon degradation: visual indicator on card showing max allowed monster
- Button fallback: UP/DOWN to cycle card selection, SELECT to confirm (for non-touch use)

---

## Key Design Decisions

### Why Emery-Only
- 200×228 gives enough space for readable 2×2 card grid + action bar
- Capacitive touch enables intuitive card selection — core to the UX
- 128KB RAM / 512KB resources — no need to optimize for constrained platforms
- The roguelike already targets basalt+emery; we target emery for the touch advantage

### Why All-Canvas Rendering
- Single `LayerUpdateProc` for the game screen avoids layer management overhead
- Full control over pixel placement for the card grid
- Simpler touch zone math (rectangular hit testing against card positions)
- Better e-paper behavior (one dirty region = one screen update)

### Why Programmatic Cards (Not Bitmaps)
- 44 unique card faces would waste resource budget
- Drawing cards with primitives (rects, text, suit shapes) is ~50 lines of code
- More flexible for theming / color adjustments
- Suits drawn via `GPath` (vector paths) scale cleanly

### Touch + Button Hybrid
- Primary input: touch (tap cards, tap action buttons)
- Secondary input: UP/DOWN to cycle card highlight, SELECT to confirm
- BACK always returns to previous screen / title
- Ensures playability even if touch is unresponsive

### E-Paper Considerations
- No continuous animation — state-driven redraws only
- Combat "animation": single frame flash (red tint), cleared on next redraw
- Card selection: border color change (no movement animation needed)
- Full-screen refresh every 5 room transitions to clear ghosting
- High contrast: white cards on black background
- No dithering, no gradients — solid fills only
