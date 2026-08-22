#ifndef STAGE_H
#define STAGE_H

#include <windows.h>

#define TOTAL_STAGES 23

void Stage_Load(HINSTANCE hInstance, int index);
int Stage_Count(void);
int Stage_HasGoal(void);
RECT Stage_GetGoalBounds(void);
void Stage_GetPlayerStart(int *x, int *y);
int Stage_IsTitleStage(void);
int Stage_Current(void);

#endif
