#include "windowquery.h"
#include "gamewindow.h"
#include "zorder.h"
#include <limits.h>
#include <stdlib.h>

#define WINDOW_SNAP_DISTANCE 20

int WindowQuery_RectsOverlap(RECT a, RECT b)
{
    return a.left < b.right && a.right > b.left && a.top < b.bottom && a.bottom > b.top;
}

int WindowQuery_FullyContains(RECT outer, RECT inner)
{
    /* WindowCollisionDetector.IsWindowContainedWithinBoundsに合わせた厳密な不等号:
       辺が接しているだけでは内包とみなさない（境界でのジッターを防ぐ）。 */
    return inner.left > outer.left && inner.top > outer.top &&
           inner.right < outer.right && inner.bottom < outer.bottom;
}

static int PointInRect(POINT p, RECT r)
{
    /* WindowCollisionDetector.IsPointWithinBoundsに合わせた厳密な不等号。 */
    return p.x > r.left && p.x < r.right && p.y > r.top && p.y < r.bottom;
}

int WindowQuery_GetFullyContaining(RECT bounds)
{
    int best = -1;
    int bestZ = -1;
    for (int i = 0; i < g_windowCount; i++)
    {
        GameWindowData *d = &g_windows[i];
        if (!IsQueryableWindow(d->kind) || !d->hwnd || d->minimized)
            continue;
        RECT wb;
        GetWindowFullBounds(d->hwnd, &wb);
        if (!WindowQuery_FullyContains(wb, bounds))
            continue;
        int z = ZOrder_GetIndex(d->hwnd);
        if (z > bestZ)
        {
            bestZ = z;
            best = i;
        }
    }
    return best;
}

void WindowQuery_GetClientBounds(int index, RECT *out)
{
    GameWindowData *d = GetWindowData(index);
    if (!d || !d->hwnd)
    {
        ZeroMemory(out, sizeof(RECT));
        return;
    }
    RECT client;
    GetClientRect(d->hwnd, &client);
    POINT tl = {0, 0};
    POINT br = {client.right, client.bottom};
    ClientToScreen(d->hwnd, &tl);
    ClientToScreen(d->hwnd, &br);
    out->left = tl.x;
    out->top = tl.y;
    out->right = br.x;
    out->bottom = br.y;

    /* GameWindowはWS_CAPTIONを使わずゲーム描画のタイトルバー帯（クライアント
       領域最上部TITLE_BAR_HEIGHT px、PaintGameWindowのDrawTitleBar参照）を
       持つため、GetClientRectはその帯を含んだ全クライアント領域を返す。
       「移動可能領域」としてはこの帯を歩行可能な内部空間に含めてはいけない
       （WS_CAPTION時代はOSが非クライアント領域として自動的に除外していた）
       ので、Goal/ボタン以外の種別ではここで明示的に上端をタイトルバー分
       押し下げる。 */
    if (d->kind != WT_GOAL && !IsButtonWindowKind(d->kind))
    {
        out->top += TITLE_BAR_HEIGHT;
        if (out->top > out->bottom)
            out->top = out->bottom;
    }
}

static int IsAdjacent(RECT a, RECT b)
{
    int nearOnAxis = abs(a.right - b.left) <= WINDOW_SNAP_DISTANCE ||
                      abs(a.left - b.right) <= WINDOW_SNAP_DISTANCE ||
                      abs(a.bottom - b.top) <= WINDOW_SNAP_DISTANCE ||
                      abs(a.top - b.bottom) <= WINDOW_SNAP_DISTANCE;
    int overlapsOtherAxis = a.left <= b.right && b.left <= a.right &&
                             a.top <= b.bottom && b.top <= a.bottom;
    return nearOnAxis && overlapsOtherAxis;
}

static void AppendDescendantsClientBounds(int index, RECT *out, int *count, int maxOut)
{
    GameWindowData *d = &g_windows[index];
    for (int i = 0; i < d->childCount; i++)
    {
        int ci = d->childIdx[i];
        if (*count < maxOut)
        {
            WindowQuery_GetClientBounds(ci, &out[*count]);
            (*count)++;
        }
        AppendDescendantsClientBounds(ci, out, count, maxOut);
    }
}

int WindowQuery_BuildMovableRegion(int parentIdx, RECT *out, int maxOut)
{
    if (parentIdx < 0 || maxOut <= 0)
        return 0;

    int count = 0;
    WindowQuery_GetClientBounds(parentIdx, &out[count]);
    RECT currentClient = out[count];
    count++;

    for (int i = 0; i < g_windowCount; i++)
    {
        GameWindowData *w = &g_windows[i];
        if (i == parentIdx || w->parentIdx >= 0 || !IsQueryableWindow(w->kind))
            continue;
        if (!w->hwnd || w->minimized)
            continue;

        RECT wClient;
        WindowQuery_GetClientBounds(i, &wClient);
        if (!WindowQuery_RectsOverlap(wClient, currentClient) && !IsAdjacent(wClient, currentClient))
            continue;

        if (count < maxOut)
            out[count++] = wClient;
        AppendDescendantsClientBounds(i, out, &count, maxOut);
    }

    return count;
}

int WindowQuery_PointInAnyRect(POINT p, const RECT *rects, int count)
{
    for (int i = 0; i < count; i++)
    {
        RECT r = rects[i];
        if (p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom)
            return 1;
    }
    return 0;
}

int WindowQuery_GetTopWindowAt(RECT bounds, int currentParentIdx)
{
    /* WindowCollisionDetector.GetTopWindowAtは、currentWindow==nullのとき
       まったく別の経路を取る: 単純な完全包含+最大Z-order判定であり、
       GetWindowFullyContainingと同一 -- 以下の5点最前面所有者ヒューリスティックは
       候補を比較するための既存の親が存在することを前提としているため使わない。 */
    if (currentParentIdx < 0)
        return WindowQuery_GetFullyContaining(bounds);

    POINT pts[5] = {
        {bounds.left, bounds.bottom},
        {bounds.right, bounds.bottom},
        {bounds.left, bounds.top},
        {bounds.right, bounds.top},
        {(bounds.left + bounds.right) / 2, (bounds.top + bounds.bottom) / 2},
    };

    int owner[5];
    for (int p = 0; p < 5; p++)
    {
        int best = -1, bestZ = -1;
        for (int i = 0; i < g_windowCount; i++)
        {
            GameWindowData *d = &g_windows[i];
            if (!IsQueryableWindow(d->kind) || !d->hwnd || d->minimized)
                continue;
            RECT wb;
            GetWindowFullBounds(d->hwnd, &wb);
            if (!PointInRect(pts[p], wb))
                continue;
            int z = ZOrder_GetIndex(d->hwnd);
            if (z > bestZ)
            {
                bestZ = z;
                best = i;
            }
        }
        owner[p] = best;
    }

    int currentHasPoint = 0;
    for (int p = 0; p < 5; p++)
        if (owner[p] == currentParentIdx)
            currentHasPoint = 1;

    int bestCandidate = -1;
    int bestBottom = INT_MAX;
    int bestZ = -1;
    for (int p = 0; p < 2; p++) /* 「足元」の2点 */
    {
        int idx = owner[p];
        if (idx < 0 || idx == currentParentIdx)
            continue;
        RECT wb;
        GetWindowFullBounds(g_windows[idx].hwnd, &wb);
        int z = ZOrder_GetIndex(g_windows[idx].hwnd);
        if (wb.bottom < bestBottom || (wb.bottom == bestBottom && z > bestZ))
        {
            bestBottom = wb.bottom;
            bestZ = z;
            bestCandidate = idx;
        }
    }

    if (currentHasPoint)
    {
        if (bestCandidate >= 0)
        {
            int curBottom = INT_MAX;
            if (currentParentIdx >= 0 && g_windows[currentParentIdx].hwnd)
            {
                RECT cb;
                GetWindowFullBounds(g_windows[currentParentIdx].hwnd, &cb);
                curBottom = cb.bottom;
            }
            if (bestBottom <= curBottom)
                return bestCandidate;
        }
        return currentParentIdx;
    }

    if (bestCandidate >= 0)
        return bestCandidate;

    for (int p = 0; p < 5; p++)
        if (owner[p] >= 0)
            return owner[p];

    return -1;
}
