/* WindowAction ネイティブ実装 (Win32) - フルエンジン: ウィンドウ戦略、衝突判定、階層管理、全23ステージ */
#include <windows.h>
#include "gamewindow.h"
#include "player.h"
#include "zorder.h"
#include "noentry.h"
#include "strategy.h"
#include "stage.h"
#include "gamefont.h"

static Player g_player;

static void LoadStage(HINSTANCE hInstance, int index)
{
    if (index < 0)
        index = 0;
    if (index >= Stage_Count())
        index = Stage_Count() - 1;

    Stage_Load(hInstance, index);

    int sx, sy;
    Stage_GetPlayerStart(&sx, &sy);
    Player_Reset(&g_player, sx, sy);
    Player_AssignInitialParent(&g_player);
}

static void HandleStageRequests(HINSTANCE hInstance)
{
    if (g_requestExit)
    {
        PostQuitMessage(0);
        return;
    }
    if (g_requestToTitle)
    {
        g_requestToTitle = 0;
        LoadStage(hInstance, 0);
    }
    else if (g_requestRestart)
    {
        g_requestRestart = 0;
        LoadStage(hInstance, Stage_Current());
    }
    else if (g_requestNext)
    {
        g_requestNext = 0;
        LoadStage(hInstance, Stage_Current() + 1);
    }
}

static void CheckGoal(HINSTANCE hInstance)
{
    if (!Stage_HasGoal())
        return;

    /* ゴールはMovableな親にドラッグされて移動したり、リサイズされたり、
       最小化によって隠れたりすることがある -- Stage_GetGoalBounds()は
       ステージロード時点で固定されたスナップショットなので、接触判定は
       プレイヤーの場合と同様に毎フレームゴールの実際のHWND境界を読み取る
       必要がある。最小化されたゴール（最小化されたウィンドウの中に隠れている）
       には一切触れられない。 */
    int goalIdx = FindGoalIndex();
    if (goalIdx < 0 || !g_windows[goalIdx].hwnd || g_windows[goalIdx].minimized)
        return;

    RECT pb, gb;
    Player_GetBounds(&g_player, &pb);
    GetWindowFullBounds(g_windows[goalIdx].hwnd, &gb);

    if (pb.left < gb.right && pb.right > gb.left && pb.top < gb.bottom && pb.bottom > gb.top)
    {
        LoadStage(hInstance, Stage_Current() + 1);
    }
}

static void HandleDeletableInput(void)
{
    if (!(GetAsyncKeyState(VK_DELETE) & 0x8000))
        return;
    /* DeleteWindowはg_windows[i].hwndを無効化するだけで配列の他の要素を
       詰め直したりしないため、有効件数分の単純な前方走査で安全。 */
    for (int i = 0; i < g_windowCount; i++)
    {
        if (g_windows[i].kind == WT_DELETABLE && g_windows[i].hwnd)
            DeleteWindow(i);
    }
}

static void InvalidateLiveWindows(void)
{
    /* NoEntryの縞模様はアニメーションし、Movable/Resizable/Minimizableのマークは
       ホバーハイライトのために現在のカーソル位置を追跡し、また親リンクが
       変化した際にはウィンドウ自身が動いていなくても親色のアウトラインを
       更新する必要がある -- これらはすべて次のWM_PAINTでしか反映されないため、
       毎フレーム強制的に発生させる。 */
    for (int i = 0; i < g_windowCount; i++)
    {
        if (IsQueryableWindow(g_windows[i].kind) && g_windows[i].hwnd)
            InvalidateRect(g_windows[i].hwnd, NULL, FALSE);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    (void)hPrevInstance;
    (void)nCmdShow;

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    GameFont_Init(hInstance);

    RegisterGameWindowClass(hInstance);
    RegisterPlayerWindowClass(hInstance);
    NoEntry_RegisterWindowClass(hInstance);

    CreatePlayerWindow(hInstance, &g_player, 0, 0);

    int startStage = 0;
    if (lpCmdLine && lpCmdLine[0] != '\0')
    {
        int v = 0;
        for (const char *c = lpCmdLine; *c >= '0' && *c <= '9'; c++)
            v = v * 10 + (*c - '0');
        startStage = v;
    }
    LoadStage(hInstance, startStage);

    LARGE_INTEGER freq, prev, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev);

    const double targetFrameMs = 1000.0 / 60.0;

    MSG msg;
    for (;;)
    {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                return (int)msg.wParam;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        QueryPerformanceCounter(&now);
        double dt = (double)(now.QuadPart - prev.QuadPart) / (double)freq.QuadPart;
        prev = now;
        if (dt > 0.1)
            dt = 0.1;

        Strategy_UpdateAll((float)dt);
        Player_Update(&g_player, (float)dt);
        Goal_UpdateParent();
        Button_UpdateParent();
        HandleDeletableInput();
        NoEntry_UpdateAnimation((float)dt);
        MinimizeAnim_UpdateAll((float)dt);
        InvalidateLiveWindows();
        ZOrder_ReassertOverlayFront(g_player.hwnd);

        CheckGoal(hInstance);
        HandleStageRequests(hInstance);

        QueryPerformanceCounter(&now);
        double elapsedMs = 1000.0 * (double)(now.QuadPart - prev.QuadPart) / (double)freq.QuadPart;
        double sleepMs = targetFrameMs - elapsedMs;
        if (sleepMs > 0)
            Sleep((DWORD)sleepMs);

        /* InputSystemはどちらかのShiftキーではなく、Keys.LShiftKeyを明示的にチェックする。 */
        if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) && (GetAsyncKeyState(VK_LSHIFT) & 0x8000))
            return 0;
    }
}
