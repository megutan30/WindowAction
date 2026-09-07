/* Core/StageManager.cs の InitializeStages() から1:1で移植。 */
#include "stage.h"
#include "gamewindow.h"
#include "noentry.h"
#include "hierarchy.h"

#define GOAL_SIZE 64
#define BTN_W 150
#define BTN_H 40

static int g_hasGoal = 0;
static RECT g_goalBounds;
static int g_playerStartX, g_playerStartY;
static int g_isTitleStage = 0;
static int g_currentStage = -1;
/* CurrentStage.EnableDesktopIconsと一致: このフラグを立てたステージだけ、
   プレイヤーの接地判定がデスクトップアイコンを床として扱う
   （player.cのCheckGroundedNormal/Inverted参照）。デスクトップアイコンの
   実際の取得・可視化マーカー生成はDesktopIcon_Refresh(main.c)が
   Stage_DesktopIconsEnabled()を見て行う。 */
static int g_desktopIconsEnabled = 0;

static void MakeGoal(HINSTANCE h, int x, int y)
{
    HWND hwnd = CreateGameWindow(h, WT_GOAL, x, y, GOAL_SIZE, GOAL_SIZE, "G");
    g_hasGoal = 1;
    GetWindowFullBounds(hwnd, &g_goalBounds);
}

static void MakeButton(HINSTANCE h, WindowKind kind, int x, int y, const char *label)
{
    CreateGameWindow(h, kind, x, y, BTN_W, BTN_H, label);
}

int Stage_Count(void) { return TOTAL_STAGES; }
int Stage_HasGoal(void) { return g_hasGoal; }
RECT Stage_GetGoalBounds(void) { return g_goalBounds; }
void Stage_GetPlayerStart(int *x, int *y) { *x = g_playerStartX; *y = g_playerStartY; }
int Stage_IsTitleStage(void) { return g_isTitleStage; }
int Stage_Current(void) { return g_currentStage; }
int Stage_DesktopIconsEnabled(void) { return g_desktopIconsEnabled; }

void Stage_Load(HINSTANCE h, int index)
{
    if (index < 0 || index >= TOTAL_STAGES)
        return;

    ResetWindowRegistry();
    g_hasGoal = 0;
    g_isTitleStage = 0;
    g_desktopIconsEnabled = 0;
    g_currentStage = index;

    switch (index)
    {
    case 0: /* タイトル */
        CreateGameWindow(h, WT_NORMAL_BLACK, 625, 750, 625, 188, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK, 250, 563, 500, 250, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK, 1125, 563, 500, 250, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK, 250, 375, 500, 250, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK, 1125, 375, 500, 250, NULL);
        CreateGameWindow(h, WT_TEXT_DISPLAY, 625, 150, 625, 313, "Window Action Game");
        g_playerStartX = 913;
        g_playerStartY = 813;
        MakeButton(h, WT_BTN_START, 850, 500, "Start");
        MakeButton(h, WT_BTN_EXIT, 850, 625, "Exit");
#ifdef ENABLE_STAGE_EDITOR
        MakeButton(h, WT_BTN_TEST, 850, 750, "Test");
#endif
        g_isTitleStage = 1;
        break;

    case 1: /* ステージ1 */
        CreateGameWindow(h, WT_NORMAL_BLACK, 125, 750, 625, 250, NULL);
        CreateGameWindow(h, WT_NORMAL_WHITE, 625, 625, 750, 250, NULL);
        CreateGameWindow(h, WT_TEXT_DISPLAY, 625, 63, 375, 125, "Stage 1");
        MakeGoal(h, 1250, 750);
        g_playerStartX = 188;
        g_playerStartY = 813;
        MakeButton(h, WT_BTN_TOTITLE, 106, 113, "Title");
        MakeButton(h, WT_BTN_RETRY, 369, 113, "Retry");
        break;

    case 2: /* ステージ2 */
        CreateGameWindow(h, WT_NORMAL_BLACK, 125, 750, 625, 250, NULL);
        CreateGameWindow(h, WT_MOVABLE, 688, 763, 250, 250, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK, 1250, 750, 625, 250, NULL);
        CreateGameWindow(h, WT_TEXT_DISPLAY, 625, 63, 375, 125, "Stage 2");
        MakeGoal(h, 1625, 875);
        g_playerStartX = 188;
        g_playerStartY = 813;
        MakeButton(h, WT_BTN_TOTITLE, 106, 113, "Title");
        MakeButton(h, WT_BTN_RETRY, 369, 113, "Retry");
        break;

    case 3: /* ステージ3 */
        CreateGameWindow(h, WT_NORMAL_BLACK, 125, 245, 625, 250, NULL);
        CreateGameWindow(h, WT_RESIZABLE, 725, 250, 250, 250, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK, 1250, 750, 625, 250, NULL);
        CreateGameWindow(h, WT_TEXT_DISPLAY, 625, 63, 375, 125, "Stage 3");
        MakeGoal(h, 1625, 875);
        g_playerStartX = 188;
        g_playerStartY = 313;
        MakeButton(h, WT_BTN_TOTITLE, 106, 113, "Title");
        MakeButton(h, WT_BTN_RETRY, 369, 113, "Retry");
        break;

    case 4: /* ステージ4 */
        CreateGameWindow(h, WT_NORMAL_BLACK, 63, 838, 625, 250, NULL);
        CreateGameWindow(h, WT_NORMAL_WHITE, 563, 675, 625, 250, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK, 1063, 513, 625, 250, NULL);
        CreateGameWindow(h, WT_NORMAL_WHITE, 563, 350, 625, 250, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK, 63, 188, 625, 250, NULL);
        CreateGameWindow(h, WT_TEXT_DISPLAY, 625, 63, 375, 125, "Stage 4");
        MakeGoal(h, 125, 1000);
        g_playerStartX = 438;
        g_playerStartY = 355;
        MakeButton(h, WT_BTN_TOTITLE, 106, 113, "Title");
        MakeButton(h, WT_BTN_RETRY, 369, 113, "Retry");
        break;

    case 5: /* ステージ5 */
        CreateGameWindow(h, WT_NORMAL_BLACK, 63, 838, 1000, 250, NULL);
        CreateGameWindow(h, WT_NORMAL_WHITE, 63, 675, 625, 250, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK, 63, 513, 625, 250, NULL);
        CreateGameWindow(h, WT_NORMAL_WHITE, 63, 350, 625, 250, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK, 63, 188, 625, 250, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK, 1063, 188, 625, 250, NULL);
        CreateGameWindow(h, WT_NORMAL_WHITE, 1063, 350, 625, 250, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK, 1063, 513, 625, 250, NULL);
        CreateGameWindow(h, WT_NORMAL_WHITE, 1063, 675, 625, 250, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK, 688, 838, 1000, 250, NULL);
        CreateGameWindow(h, WT_TEXT_DISPLAY, 625, 63, 375, 125, "Stage 5");
        MakeGoal(h, 1500, 250);
        g_playerStartX = 313;
        g_playerStartY = 250;
        MakeButton(h, WT_BTN_TOTITLE, 106, 113, "Title");
        MakeButton(h, WT_BTN_RETRY, 369, 113, "Retry");
        break;

    case 6: /* ステージ6 */
        CreateGameWindow(h, WT_NORMAL_BLACK, 125, 750, 375, 250, NULL);
        CreateGameWindow(h, WT_MOVABLE, 1500, 125, 250, 250, NULL);
        CreateGameWindow(h, WT_TEXT_DISPLAY, 625, 63, 375, 125, "Stage 6");
        MakeGoal(h, 1625, 250);
        g_playerStartX = 188;
        g_playerStartY = 813;
        NoEntry_AddZone(h, 825, 375, 125, 750);
        NoEntry_AddZone(h, 1200, 0, 125, 750);
        MakeButton(h, WT_BTN_TOTITLE, 106, 113, "Title");
        MakeButton(h, WT_BTN_RETRY, 369, 113, "Retry");
        break;

    case 7: /* ステージ7 -- StageManager.cs自体のこの箇所のラベルは「Stage 13」
               になっている（index 13の本物のStage 13と共有されている、
               コピペミスと思われる）。忠実性を保つためそのまま再現している。 */
        CreateGameWindow(h, WT_RESIZABLE, 725, 375, 250, 713, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK, 725, 125, 250, 313, NULL);
        CreateGameWindow(h, WT_TEXT_DISPLAY, 625, 63, 375, 125, "Stage 7");
        MakeGoal(h, 750, 250);
        g_playerStartX = 750;
        g_playerStartY = 938;
        MakeButton(h, WT_BTN_TOTITLE, 106, 113, "Title");
        MakeButton(h, WT_BTN_RETRY, 369, 113, "Retry");
        break;

    case 8: /* ステージ8 */
        CreateGameWindow(h, WT_RESIZABLE, 1100, 63, 250, 250, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK, 100, 750, 1063, 250, NULL);
        CreateGameWindow(h, WT_TEXT_DISPLAY, 625, 63, 375, 125, "Stage 8");
        MakeGoal(h, 1125, 125);
        g_playerStartX = 125;
        g_playerStartY = 875;
        MakeButton(h, WT_BTN_TOTITLE, 106, 113, "Title");
        MakeButton(h, WT_BTN_RETRY, 369, 113, "Retry");
        break;

    case 9: /* ステージ9 */
        CreateGameWindow(h, WT_RESIZABLE, 938, 125, 938, 750, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK, 969, 188, 375, 250, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK, 313, 125, 375, 188, NULL);
        CreateGameWindow(h, WT_TEXT_DISPLAY, 625, 63, 375, 125, "Stage 9");
        MakeGoal(h, 375, 225);
        g_playerStartX = 1313;
        g_playerStartY = 750;
        MakeButton(h, WT_BTN_TOTITLE, 106, 50, "Title");
        MakeButton(h, WT_BTN_RETRY, 369, 50, "Retry");
        break;

    case 10: /* ステージ10 */
        CreateGameWindow(h, WT_NORMAL_BLACK, 1000, 375, 625, 250, NULL);
        CreateGameWindow(h, WT_MOVABLE, 188, 388, 625, 625, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK, 375, 500, 250, 250, NULL);
        CreateGameWindow(h, WT_TEXT_DISPLAY, 625, 63, 375, 125, "Stage 10");
        MakeGoal(h, 469, 543);
        g_playerStartX = 313;
        g_playerStartY = 813;
        MakeButton(h, WT_BTN_TOTITLE, 106, 113, "Title");
        MakeButton(h, WT_BTN_RETRY, 369, 113, "Retry");
        break;

    case 11: /* ステージ11 */
        CreateGameWindow(h, WT_MOVABLE, 188, 368, 625, 625, NULL);
        CreateGameWindow(h, WT_TEXT_DISPLAY, 625, 63, 375, 125, "Stage 11");
        MakeGoal(h, 419, 543);
        g_playerStartX = 313;
        g_playerStartY = 813;
        MakeButton(h, WT_BTN_TOTITLE, 106, 113, "Title");
        MakeButton(h, WT_BTN_RETRY, 369, 113, "Retry");
        break;

    case 12: /* ステージ12 */
        CreateGameWindow(h, WT_MOVABLE, 188, 388, 625, 625, NULL);
        MakeGoal(h, 419, 563);
        g_playerStartX = 313;
        g_playerStartY = 813;
        MakeButton(h, WT_BTN_TOTITLE, 500, 300, "Title");
        MakeButton(h, WT_BTN_RETRY, 700, 300, "Retry");
        break;

    case 13: /* ステージ13 */
        CreateGameWindow(h, WT_MOVABLE, 63, 750, 250, 250, NULL);
        CreateGameWindow(h, WT_RESIZABLE, 63, 250, 500, 500, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK, 1250, 750, 625, 250, NULL);
        CreateGameWindow(h, WT_TEXT_DISPLAY, 625, 63, 375, 125, "Stage 13");
        MakeGoal(h, 1625, 875);
        g_playerStartX = 188;
        g_playerStartY = 813;
        NoEntry_AddZone(h, 1075, 0, 125, 500);
        NoEntry_AddZone(h, 1075, 625, 125, 500);
        MakeButton(h, WT_BTN_TOTITLE, 106, 113, "Title");
        MakeButton(h, WT_BTN_RETRY, 369, 113, "Retry");
        break;

    case 14: /* ステージ14 */
        CreateGameWindow(h, WT_NORMAL_BLACK, 500, 550, 500, 300, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK, 900, 550, 500, 300, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK_NOENTRY, 125, 750, 625, 250, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK_NOENTRY, 1250, 750, 625, 250, NULL);
        CreateGameWindow(h, WT_TEXT_DISPLAY, 625, 63, 375, 125, "Stage 14");
        MakeGoal(h, 1625, 875);
        g_playerStartX = 188;
        g_playerStartY = 813;
        MakeButton(h, WT_BTN_TOTITLE, 106, 113, "Title");
        MakeButton(h, WT_BTN_RETRY, 369, 113, "Retry");
        break;

    case 15: /* ステージ15 */
        CreateGameWindow(h, WT_NORMAL_BLACK, 500, 250, 500, 600, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK, 900, 250, 500, 600, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK_NOENTRY, 125, 650, 625, 350, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK_NOENTRY, 1250, 650, 625, 350, NULL);
        CreateGameWindow(h, WT_TEXT_DISPLAY, 625, 63, 375, 125, "Stage 15");
        MakeGoal(h, 625, 375);
        g_playerStartX = 188;
        g_playerStartY = 813;
        MakeButton(h, WT_BTN_TOTITLE, 106, 113, "Title");
        MakeButton(h, WT_BTN_RETRY, 369, 113, "Retry");
        break;

    case 16: /* ステージ16 */
        CreateGameWindow(h, WT_NORMAL_BLACK, 625, 313, 625, 725, NULL);
        CreateGameWindow(h, WT_MINIMIZABLE, 625, 375, 625, 375, NULL);
        CreateGameWindow(h, WT_TEXT_DISPLAY, 625, 63, 375, 125, "Stage 16");
        MakeGoal(h, 750, 875);
        g_playerStartX = 750;
        g_playerStartY = 500;
        MakeButton(h, WT_BTN_TOTITLE, 106, 113, "Title");
        MakeButton(h, WT_BTN_RETRY, 369, 113, "Retry");
        break;

    case 17: /* ステージ17 */
        CreateGameWindow(h, WT_MINIMIZABLE, 250, 750, 375, 250, NULL);
        CreateGameWindow(h, WT_MINIMIZABLE, 750, 750, 375, 250, NULL);
        CreateGameWindow(h, WT_TEXT_DISPLAY, 625, 63, 375, 125, "Stage 18");
        MakeGoal(h, 950, 925);
        g_playerStartX = 375;
        g_playerStartY = 875;
        MakeButton(h, WT_BTN_TOTITLE, 106, 113, "Title");
        MakeButton(h, WT_BTN_RETRY, 369, 113, "Retry");
        break;

    case 18: /* ステージ18 */
        CreateGameWindow(h, WT_MINIMIZABLE, 50, 750, 375, 250, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK, 750, 750, 375, 250, NULL);
        CreateGameWindow(h, WT_TEXT_DISPLAY, 625, 63, 375, 125, "Stage 19");
        MakeGoal(h, 1750, 525);
        g_playerStartX = 275;
        g_playerStartY = 875;
        NoEntry_AddZone(h, 1250, 613, 825, 325);
        NoEntry_AddZone(h, 500, 906, 125, 188);
        MakeButton(h, WT_BTN_TOTITLE, 106, 113, "Title");
        MakeButton(h, WT_BTN_RETRY, 369, 113, "Retry");
        break;

    case 19: /* ステージ19 */
        CreateGameWindow(h, WT_MINIMIZABLE, 250, 750, 375, 250, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK, 625, 594, 375, 250, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK, 875, 438, 375, 250, NULL);
        CreateGameWindow(h, WT_MINIMIZABLE, 1250, 63, 625, 250, NULL);
        CreateGameWindow(h, WT_TEXT_DISPLAY, 625, 63, 375, 125, "Stage 20");
        MakeGoal(h, 1750, 125);
        g_playerStartX = 375;
        g_playerStartY = 875;
        NoEntry_AddZone(h, 1250, 313, 625, 625);
        NoEntry_AddZone(h, 0, 906, 125, 188);
        MakeButton(h, WT_BTN_TOTITLE, 106, 113, "Title");
        MakeButton(h, WT_BTN_RETRY, 369, 113, "Retry");
        break;

    // case 21: /* ステージ21 - デスクトップアイコン（未実装、通常のプラットフォームステージとしてプレイされる） */
    //     CreateGameWindow(h, WT_MINIMIZABLE, 250, 750, 375, 250, NULL);
    //     CreateGameWindow(h, WT_TEXT_DISPLAY, 625, 463, 575, 125, "Stage21 DeskTopIcon");
    //     MakeGoal(h, 1750, 125);
    //     g_playerStartX = 375;
    //     g_playerStartY = 875;
    //     MakeButton(h, WT_BTN_TOTITLE, 106, 113, "Title");
    //     MakeButton(h, WT_BTN_RETRY, 369, 113, "Retry");
    //     break;
        
    case 20: /* ステージ20 */
        CreateGameWindow(h, WT_RESIZABLE_NOENTRY, 520, 408, 250, 250, NULL);
        CreateGameWindow(h, WT_MINIMIZABLE, 916, 688, 250, 250, NULL);
        CreateGameWindow(h, WT_TEXT_DISPLAY, 625, 63, 375, 125, "Stage 21");
        MakeGoal(h, 607, 487);
        g_playerStartX = 1021;
        g_playerStartY = 878;
        MakeButton(h, WT_BTN_TOTITLE, 106, 113, "Title");
        MakeButton(h, WT_BTN_RETRY, 369, 113, "Retry");
        break;

    case 21: /* ステージ22 */
        CreateGameWindow(h, WT_NORMAL_BLACK, 286, 256, 250, 250, NULL);
        MakeGoal(h, 392, 374);
        CreateGameWindow(h, WT_UNCONSTRAINED, 612, 693, 434, 279, NULL);
        CreateGameWindow(h, WT_TEXT_DISPLAY, 625, 63, 375, 125, "Stage 22");
        g_playerStartX = 717;
        g_playerStartY = 902;
        MakeButton(h, WT_BTN_TOTITLE, 106, 113, "Title");
        MakeButton(h, WT_BTN_RETRY, 369, 113, "Retry");
        break;

    case 22: /* ステージ23 */
        MakeGoal(h, 1657, 42);
        CreateGameWindow(h, WT_UNCONSTRAINED, 342, 423, 347, 300, NULL);
        CreateGameWindow(h, WT_MINIMIZABLE, 374, 496, 203, 192, NULL);
        NoEntry_AddZone(h, 5, 125, 672, 126);
        NoEntry_AddZone(h, 847, 126, 1058, 110);
        CreateGameWindow(h, WT_TEXT_DISPLAY, 125, 163, 375, 125, "Stage 23");
        g_playerStartX = 448;
        g_playerStartY = 628;
        MakeButton(h, WT_BTN_TOTITLE, 106, 113, "Title");
        MakeButton(h, WT_BTN_RETRY, 369, 113, "Retry");
        break;

    case 23: /* ゲームクリア */
        CreateGameWindow(h, WT_NORMAL_BLACK, 625, 750, 625, 188, NULL);
        CreateGameWindow(h, WT_TEXT_DISPLAY, 250, 563, 500, 250, "Game");
        CreateGameWindow(h, WT_TEXT_DISPLAY, 1125, 563, 500, 250, "Clear");
        CreateGameWindow(h, WT_NORMAL_BLACK, 250, 375, 500, 250, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK, 1125, 375, 500, 250, NULL);
        CreateGameWindow(h, WT_TEXT_DISPLAY, 625, 150, 625, 313, "Thank you!!");
        g_playerStartX = 913;
        g_playerStartY = 813;
        MakeButton(h, WT_BTN_TOTITLE, 850, 500, "Title");
        g_isTitleStage = 1;
        break;

    case 24: /* ステージ24 -- デスクトップアイコンを足場として使う。
                当初のステージ21案（上のコメントアウトされたブロック参照）を
                実際のデスクトップアイコン当たり判定付きで復活させたもの。
                アイコンの位置は実行環境のデスクトップに依存するため、
                アイコンが無い/少ない環境でもクリア可能なよう、通常の
                ウィンドウの飛び石だけでもゴールへ到達できるようにしてある
                （デスクトップアイコンはあくまで追加の/近道の足場）。 */
        CreateGameWindow(h, WT_MINIMIZABLE, 250, 750, 375, 250, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK, 700, 800, 200, 200, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK, 1000, 700, 200, 200, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK, 1300, 800, 200, 200, NULL);
        CreateGameWindow(h, WT_NORMAL_BLACK, 1600, 750, 375, 250, NULL);
        CreateGameWindow(h, WT_TEXT_DISPLAY, 625, 463, 575, 125, "Stage24 DesktopIcon");
        MakeGoal(h, 1725, 875);
        g_playerStartX = 313;
        g_playerStartY = 813;
        g_desktopIconsEnabled = 1;
        MakeButton(h, WT_BTN_TOTITLE, 106, 113, "Title");
        MakeButton(h, WT_BTN_RETRY, 369, 113, "Retry");
        break;
    }

    /* WindowManager.RegisterWindowは、ウィンドウが生成されるたびに
       CheckPotentialParentWindowを呼び出すため、親子ツリー（およびそれに
       依存する枠線の色や移動/リサイズの伝播）は、プレイヤーやユーザーが
       何かに触れる前にすでに構築されている。 */
    for (int i = 0; i < g_windowCount; i++)
    {
        if (IsQueryableWindow(g_windows[i].kind))
            Hierarchy_CheckAndUpdate(i);
    }

    /* Goal自身のコンストラクタも、最初の親探索を即座に行う
       （オリジナルではGetTopWindowAt。ここではGoal_UpdateParentがこの後
       毎フレーム使うのと同じ完全包含チェックで代用しており、十分に近い）。 */
    Goal_UpdateParent();

    /* GameButtonにはこのようなコンストラクタ時点での探索は存在しない
       -- 最初は親を持たない状態で始まり、最初のUpdateAsyncで初めて
       アタッチされる -- が、ここで同じチェックを一度実行しておいても
       害はなく、1フレーム分のギャップを回避できる。 */
    Button_UpdateParent();
}
