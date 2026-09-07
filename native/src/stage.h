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

/* Goal/ボタンの既定サイズ。editor.cのパレット項目（実際に配置される種別と
   同じ見た目にするための仮アイコン）もこれと同じ寸法にする必要があるため、
   ここで共有し二重管理を避ける（以前はeditor.c側にEDITOR_GOAL_SIZE等の
   独自定義があり、値をここと手動で同期する前提になっていた）。 */
#define GOAL_SIZE 64
#define BTN_W 150
#define BTN_H 40

void Stage_Load(HINSTANCE hInstance, int index);
int Stage_Count(void);
int Stage_HasGoal(void);
RECT Stage_GetGoalBounds(void);
void Stage_GetPlayerStart(int *x, int *y);
int Stage_Current(void);

/* CurrentStage.EnableDesktopIconsと一致: trueならプレイヤーの接地判定が
   デスクトップアイコンも床として扱う（player.c参照）。main.cはこれを見て
   DesktopIcon_Refresh/DesktopIcon_Clearを呼び分ける。 */
int Stage_DesktopIconsEnabled(void);

#endif
