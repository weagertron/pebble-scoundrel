#ifndef RULES_H
#define RULES_H

#include "../core_types.h"
typedef void (*RulesCallback)(GameScreen screen);

void rules_window_push(RulesCallback callback);
void rules_window_pop(void);

#endif // RULES_H
