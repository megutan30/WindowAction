#include "noentry.h"
#include "gamewindow.h"
#include "zorder.h"
#include "windowquery.h"
#include "gdiobj.h"

#define NOENTRY_BOUNDARY_WIDTH 5
#define ZONE_STRIPE_WIDTH 20
#define ZONE_PATTERN_HEIGHT (ZONE_STRIPE_WIDTH * 2)

static int RectsOverlap(RECT a, RECT b)
{
    return WindowQuery_RectsOverlap(a, b);
}

static const char *kZoneWindowClass = "WA_NoEntryZone";
static HWND g_zoneHwnd[MAX_NOENTRY_ZONES];
static float g_zoneAnimOffset = 0.0f;

static void PaintZone(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);

    ScopedCompatibleDC memDC(hdc);
    GdiHandle<HBITMAP> memBmp(CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top));
    ScopedSelectObject selectMemBmp(memDC, memBmp);

    /* NoEntryZone_Paint: 幅STRIPE_WIDTH=20の水平帯を、FromArgb(180,Red)と
       FromArgb(180,Black)で交互に下方向へスクロールさせる。GDIには任意の
       背景に対する安価なピクセル単位のアルファ合成手段がないため、
       これらは元の半透明色を単色で近似したもの。 */
    int startY = -((int)g_zoneAnimOffset % ZONE_PATTERN_HEIGHT);
    {
        GdiBrush redBrush(RGB(220, 50, 50));
        GdiBrush darkBrush(RGB(50, 50, 50));
        for (int y = startY; y < rc.bottom + ZONE_PATTERN_HEIGHT; y += ZONE_PATTERN_HEIGHT)
        {
            RECT red = {rc.left, y, rc.right, y + ZONE_STRIPE_WIDTH};
            RECT dark = {rc.left, y + ZONE_STRIPE_WIDTH, rc.right, y + ZONE_PATTERN_HEIGHT};
            FillRect(memDC, &red, redBrush);
            FillRect(memDC, &dark, darkBrush);
        }
    }

    BitBlt(hdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, memDC, 0, 0, SRCCOPY);

    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK ZoneWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_PAINT:
        PaintZone(hwnd);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

void NoEntry_RegisterWindowClass(HINSTANCE hInstance)
{
    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = ZoneWindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = kZoneWindowClass;
    wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.hbrBackground = NULL;
    RegisterClassA(&wc);
}

void NoEntry_AddZone(HINSTANCE hInstance, int x, int y, int w, int h)
{
    if (g_noEntryZoneCount >= MAX_NOENTRY_ZONES)
        return;
    RECT r = {x, y, x + w, y + h};
    int idx = g_noEntryZoneCount;
    g_noEntryZones[g_noEntryZoneCount++] = r;

    /* WS_EX_TRANSPARENT（クリックスルー）+ WS_EX_TOPMOSTは、NoEntryZone.csの
       SetWindowPropertiesと厳密に一致。 */
    HWND hwnd = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST, kZoneWindowClass, "NoEntryZone",
        WS_POPUP | WS_VISIBLE,
        x, y, w, h,
        NULL, NULL, hInstance, NULL);

    if (hwnd)
    {
        SetLayeredWindowAttributes(hwnd, RGB(255, 0, 255), 0, LWA_COLORKEY);
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    g_zoneHwnd[idx] = hwnd;
}

void NoEntry_ResetZones(void)
{
    for (int i = 0; i < g_noEntryZoneCount; i++)
    {
        if (g_zoneHwnd[i])
            DestroyWindow(g_zoneHwnd[i]);
        g_zoneHwnd[i] = NULL;
    }
    g_noEntryZoneCount = 0;
}

void NoEntry_RemoveZone(int index)
{
    if (index < 0 || index >= g_noEntryZoneCount)
        return;
    if (g_zoneHwnd[index])
        DestroyWindow(g_zoneHwnd[index]);
    for (int i = index; i < g_noEntryZoneCount - 1; i++)
    {
        g_noEntryZones[i] = g_noEntryZones[i + 1];
        g_zoneHwnd[i] = g_zoneHwnd[i + 1];
    }
    g_noEntryZoneCount--;
}

void NoEntry_UpdateAnimation(float dt)
{
    const float SPEED = 40.0f; /* px/sec、現行のOutlineRendererと一致 */
    for (int i = 0; i < g_windowCount; i++)
    {
        if (HasCapability(g_windows[i].capabilities, WC_NOENTRY))
            g_windows[i].stripeOffset += SPEED * dt;
    }

    g_zoneAnimOffset += SPEED * dt;
    for (int i = 0; i < g_noEntryZoneCount; i++)
    {
        if (g_zoneHwnd[i])
            InvalidateRect(g_zoneHwnd[i], NULL, FALSE);
    }
}

int NoEntry_GetBoundaryRects(int windowIndex, RECT out[4])
{
    GameWindowData *data = GetWindowData(windowIndex);
    if (!data || !HasCapability(data->capabilities, WC_NOENTRY) || !data->hwnd || data->minimized)
        return 0;

    RECT b;
    GetWindowFullBounds(data->hwnd, &b);
    int bw = NOENTRY_BOUNDARY_WIDTH;

    out[0] = RECT{b.left, b.top, b.right, b.top + bw};                 /* 上 */
    out[1] = RECT{b.left, b.bottom - bw, b.right, b.bottom};           /* 下 */
    out[2] = RECT{b.left, b.top, b.left + bw, b.bottom};               /* 左 */
    out[3] = RECT{b.right - bw, b.top, b.right, b.bottom};             /* 右 */
    return 4;
}

int NoEntry_ComputeVisibleRegion(int windowIndex, RECT rect,
                                  const std::function<bool(int)> &isOccluder,
                                  RECT *outBounds)
{
    HWND hwnd = g_windows[windowIndex].hwnd;
    if (!hwnd || IsRectEmpty(&rect))
        return 0;

    int myZ = ZOrder_GetIndex(hwnd);
    GdiHandle<HRGN> visible(CreateRectRgnIndirect(&rect));

    for (int i = 0; i < g_windowCount; i++)
    {
        if (i == windowIndex || !g_windows[i].hwnd)
            continue;
        if (ZOrder_GetIndex(g_windows[i].hwnd) <= myZ)
            continue; /* 厳密により前面にあるウィンドウのみが遮蔽できる */
        if (!isOccluder(i))
            continue;

        RECT coverBounds;
        GetWindowFullBounds(g_windows[i].hwnd, &coverBounds);
        GdiHandle<HRGN> coverRgn(CreateRectRgnIndirect(&coverBounds));
        CombineRgn(visible, visible, coverRgn, RGN_DIFF);
    }

    RECT box;
    int rgnType = GetRgnBox(visible, &box);

    if (rgnType == NULLREGION || rgnType == ERROR)
        return 0;
    if (outBounds)
        *outBounds = box;
    return 1;
}

/* noentry.h参照。 */
int NoEntry_GetVisiblePortion(int windowIndex, RECT rect, RECT *outBounds)
{
    /* NoEntryBoundaryCollider.CheckCollisionは、他のNoEntryウィンドウに
       限らず、より前面にある「あらゆる」ウィンドウを除外対象とする --
       NoEntryウィンドウの境界の手前に置かれた通常ウィンドウも、通常の
       描画の重なり順（上に描かれたものが下を隠す）と同様にその部分を
       隠す。ここで対象とするのは、オリジナルのwindowsListが保持するのと
       同じGameWindow相当の集合のみ: GoalやボタンはそちらではWindowsList
       とは別に管理されており、遮蔽物にはならない。 */
    auto isOccluder = [](int i)
    { return IsQueryableWindow(g_windows[i].kind) && !g_windows[i].minimized; };
    return NoEntry_ComputeVisibleRegion(windowIndex, rect, isOccluder, outBounds);
}

int NoEntry_IsRectVisibleFromWindow(int windowIndex, RECT rect)
{
    return NoEntry_GetVisiblePortion(windowIndex, rect, NULL);
}

/* GatherObstacles(collision.c)専用: NoEntry_GetBoundaryRectsの可視性考慮版。
   各境界帯のうち、より前面の（NoEntryに限らない）ウィンドウに完全に隠されて
   いるものは出力から除外し、部分的に隠れているものは可視部分の外接矩形を
   返す（NoEntryBoundaryCollider.CheckCollisionのcollisionRect計算と同じ
   近似）。戻り値は書き込んだ矩形の個数(0～4)。
   collision.cのGatherObstaclesは以前NoEntry_GetBoundaryRectsを直接使って
   おり、この可視性チェックを一切経由していなかった -- そのためNoEntry
   ウィンドウの境界の手前に別のウィンドウが重なっているだけで、実際には
   見えていない（隠れている）はずの境界にMovable/Resizableウィンドウの
   移動・リサイズが弾かれてしまう不具合があった（実際に報告された不具合:
   元のC#実装と挙動が異なる、動かそうとするとはじかれる）。プレイヤーの
   衝突判定側は元々この関数(NoEntry_IsRectVisibleFromWindow)を経由して
   いたため影響を受けていなかった。 */
int NoEntry_GetVisibleBoundaryRects(int windowIndex, RECT out[4])
{
    RECT raw[4];
    int c = NoEntry_GetBoundaryRects(windowIndex, raw);
    int n = 0;
    for (int b = 0; b < c; b++)
    {
        RECT vis;
        if (NoEntry_GetVisiblePortion(windowIndex, raw[b], &vis))
            out[n++] = vis;
    }
    return n;
}

/* `bounds`がNoEntryウィンドウ`windowIndex`の境界の「可視」部分と重なっていれば
   true -- すなわち、重なった部分がより前面のNoEntryウィンドウに完全に
   覆われていない場合。NoEntryBoundaryCollider.CheckCollisionと一致:
   交差する各境界帯について、checkBounds/境界の交差部分だけを取り出し、
   その可視性を判定する。 */
static int IsBoundaryVisible(int windowIndex, RECT bounds)
{
    RECT bnd[4];
    int c = NoEntry_GetBoundaryRects(windowIndex, bnd);
    if (c == 0)
        return 0;

    for (int b = 0; b < c; b++)
    {
        if (!RectsOverlap(bounds, bnd[b]))
            continue;

        RECT intersection;
        if (!IntersectRect(&intersection, &bounds, &bnd[b]))
            continue;

        if (NoEntry_IsRectVisibleFromWindow(windowIndex, intersection))
            return 1;
    }
    return 0;
}

int NoEntry_IntersectsAny(RECT bounds)
{
    for (int i = 0; i < g_noEntryZoneCount; i++)
        if (RectsOverlap(bounds, g_noEntryZones[i]))
            return 1;

    for (int i = 0; i < g_windowCount; i++)
    {
        if (!HasCapability(g_windows[i].capabilities, WC_NOENTRY) || !g_windows[i].hwnd || g_windows[i].minimized)
            continue;
        if (IsBoundaryVisible(i, bounds))
            return 1;
    }
    return 0;
}
