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

    /* 実際のOS Z-orderは、移動したグループ自身だけをHWND_TOPMOSTへ送れば
       十分に同期できる -- あるウィンドウを最前面へ移動すると、それ以外の
       ウィンドウは明示的にSetWindowPosを呼ばなくてもOS側が自動的に相対順位を
       調整するため。以前はg_order全件（テストモードのパレット/ツールバーの
       ような無関係なウィンドウも含む）を毎回SetWindowPosし直しており、
       クリックのたびに画面上の全ウィンドウが明滅する不具合があった
       （実際に報告された不具合）。グループ内の相対順序は従来通りgroup[0]
       （祖先側）から順にTOPMOSTへ送ることで、最後に送るgroup[groupCount-1]が
       最終的に最前面になる。 */
    for (int g = 0; g < groupCount; g++)
        SetWindowPos(group[g], HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
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
