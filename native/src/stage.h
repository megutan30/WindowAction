#ifndef STAGE_H
#define STAGE_H

#include <windows.h>

/* stage.cのStage_Loadが定義するcaseの個数と必ず一致させること --
   足りないと最後の方のステージ（ゲームクリア画面を含む）がStage_Load冒頭の
   index >= TOTAL_STAGES判定、およびmain.cのLoadStageのクランプにより
   常に到達不能になり、最後の有効ステージへ戻ってループしてしまう
   （実際に報告された不具合）。ステージを追加/削除したら必ずここも
   更新すること。 */
#define TOTAL_STAGES 25

void Stage_Load(HINSTANCE hInstance, int index);
int Stage_Count(void);
int Stage_HasGoal(void);
RECT Stage_GetGoalBounds(void);
void Stage_GetPlayerStart(int *x, int *y);
int Stage_IsTitleStage(void);
int Stage_Current(void);

/* CurrentStage.EnableDesktopIconsと一致: trueならプレイヤーの接地判定が
   デスクトップアイコンも床として扱う（player.c参照）。main.cはこれを見て
   DesktopIcon_Refresh/DesktopIcon_Clearを呼び分ける。 */
int Stage_DesktopIconsEnabled(void);

#endif
