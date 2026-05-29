#ifndef TITLE_H
#define TITLE_H

#include "../core_types.h"
typedef void (*TitleCallback)(GameScreen screen);

void title_window_push(TitleCallback callback, bool has_save);
void title_window_pop(void);
bool title_last_was_continue(void);

#endif // TITLE_H
