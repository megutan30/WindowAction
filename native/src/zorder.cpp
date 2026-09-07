#include "zorder.h"
#include "gamewindow.h"

static HWND g_order[MAX_WINDOWS];
static int g_orderCount = 0;

void ZOrder_Reset(void)
{
    g_orderCount = 0;
}

void ZOrder_Register(HWND hwnd)
{
    if (g_orderCount < MAX_WINDOWS)
        g_order[g_orderCount++] = hwnd;
}

void ZOrder_Unregister(HWND hwnd)
{
    for (int i = 0; i < g_orderCount; i++)
    {
        if (g_order[i] == hwnd)
        {
            for (int j = i; j < g_orderCount - 1; j++)
                g_order[j] = g_order[j + 1];
            g_orderCount--;
            return;
        }
    }
}

int ZOrder_GetIndex(HWND hwnd)
{
    for (int i = 0; i < g_orderCount; i++)
        if (g_order[i] == hwnd)
            return i;
    return -1;
}

int ZOrder_IsInFront(HWND a, HWND b)
{
    return ZOrder_GetIndex(a) > ZOrder_GetIndex(b);
}

static void CollectGroup(int windowIndex, HWND *group, int *count)
{
    GameWindowData *data = GetWindowData(windowIndex);
    if (!data)
        return;
    group[(*count)++] = data->hwnd;
    for (int i = 0; i < data->childCount; i++)
        CollectGroup(data->childIdx[i], group, count);
}

void ZOrder_BringToFront(HWND hwnd)
{
    int idx = FindWindowIndex(hwnd);
    if (idx < 0)
        return;

    HWND group[MAX_WINDOWS];
    int groupCount = 0;
    CollectGroup(idx, group, &groupCount);

    for (int g = 0; g < groupCount; g++)
        ZOrder_Unregister(group[g]);
    for (int g = 0; g < groupCount; g++)
        if (g_orderCount < MAX_WINDOWS)
            g_order[g_orderCount++] = group[g];

    /* 実際のOS Z-orderを内部リストに合わせて再同期する: 背面から前面へ順に走査し、
       各ウィンドウに再適用することで最後の呼び出し（最前面）が優先されるようにする。
       ここはHWND_TOPではなくHWND_TOPMOSTを使う -- ゲーム内の全ウィンドウは
       CreateGameWindowIndexedで常にHWND_TOPMOSTとして生成される設計（デスクトップ上の
       他アプリより常に手前に表示する）ため、再同期時も明示的にTOPMOSTを維持する方が
       意図に忠実。 */
    for (int i = 0; i < g_orderCount; i++)
        SetWindowPos(g_order[i], HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void ZOrder_ReassertOverlayFront(HWND playerHwnd)
{
    static const UINT flags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE;

    /* ゴールとボタンは、すべての通常の "Window" 種別のエントリより上に配置される... */
    for (int i = 0; i < g_windowCount; i++)
    {
        WindowKind k = g_windows[i].kind;
        if ((k == WT_GOAL || IsButtonWindowKind(k)) && g_windows[i].hwnd)
            SetWindowPos(g_windows[i].hwnd, HWND_TOPMOST, 0, 0, 0, 0, flags);
    }

    /* ...さらにプレイヤーはGoal/Buttonsよりも上に配置される。
       ZOrderPriority.Window < Button = Goal < Player と一致させるため。 */
    if (playerHwnd)
        SetWindowPos(playerHwnd, HWND_TOPMOST, 0, 0, 0, 0, flags);
}
