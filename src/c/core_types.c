#include "core_types.h"

// ──────────────────────────────────────────────
// Card helpers
// ──────────────────────────────────────────────

CardRole card_get_role(const Card *card) {
  switch (card->suit) {
    case SUIT_HEARTS:  return CARD_POTION;
    case SUIT_DIAMONDS: return CARD_WEAPON;
    case SUIT_SPADES:
    case SUIT_CLUBS:   return CARD_MONSTER;
    default:           return CARD_MONSTER;
  }
}

bool card_is_red(const Card *card) {
  return card->suit == SUIT_HEARTS || card->suit == SUIT_DIAMONDS;
}

const char *card_suit_char(const Card *card) {
  static const char *chars[] = {"H", "D", "S", "C"};
  if (card->suit >= SUIT_COUNT) return "?";
  return chars[card->suit];
}

const char *card_value_str(const Card *card) {
  switch (card->value) {
    case 11: return "J(11)";
    case 12: return "Q(12)";
    case 13: return "K(13)";
    case 14: return "A(14)";
    default: {
      static char buf[4];
      snprintf(buf, sizeof(buf), "%d", card->value);
      return buf;
    }
  }
}
