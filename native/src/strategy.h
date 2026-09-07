#ifndef STRATEGY_H
#define STRATEGY_H

#include "gamewindow.h"

extern int g_requestRestart;
extern int g_requestNext;
extern int g_requestToTitle;
extern int g_requestExit;

void Strategy_HandleMouseDown(int index);
void Strategy_HandleMouseUp(int index);
void Strategy_UpdateAll(float dt);

#endif
