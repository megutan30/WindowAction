#include "collision.h"
#include "gamewindow.h"
#include "noentry.h"
#include "hierarchy.h"
#include "player.h"
#include "zorder.h"
#include <stdlib.h>
#include <limits.h>

#define MAX_OBSTACLES 160

static int RectsOverlap(RECT a, RECT b)
{
    return a.left < b.right && a.right > b.left && a.top < b.bottom && a.bottom > b.top;
}

/* ZOrderVisibilityService.IsWindowVisibleFromNoEntryと一致: NoEntry
   ウィンドウが移動/リサイズ中のとき、通常の実体ウィンドウは、より前面の
   何かに完全に隠れていない場合のみ障害物として扱う。ここで不透明な遮蔽物と
   なるのは他のNoEntryウィンドウのみ（通常ウィンドウ同士は「透明」であり
   互いを隠さない） -- ただしオリジナルから引き継いだ意図的な例外が一つある:
   excludeIndexの親ウィンドウは、NoEntryでなくても常に遮蔽物として扱われる。
   これにより、ウィンドウが自分の親の背後をすり抜けられないようにしている。 */
static int IsNormalWindowVisibleFromExcluded(int windowIndex, int excludeIndex)
{
    GameWindowData *win = &g_windows[windowIndex];
    RECT windowBounds;
    GetWindowFullBounds(win->hwnd, &windowBounds);

    HRGN visible = CreateRectRgnIndirect(&windowBounds);
    int myZ = ZOrder_GetIndex(win->hwnd);
    int excludeParentIdx = (excludeIndex >= 0) ? g_windows[excludeIndex].parentIdx : -1;

    for (int i = 0; i < g_windowCount; i++)
    {
        if (!g_windows[i].hwnd || i == windowIndex)
            continue;
        if (ZOrder_GetIndex(g_windows[i].hwnd) <= myZ)
            continue;
        if (i == excludeIndex)
            continue; /* 移動中のウィンドウ自身が自分の障害物を隠すことはない */

        RECT coverBounds;
        GetWindowFullBounds(g_windows[i].hwnd, &coverBounds);

        if (i != excludeParentIdx && (!g_windows[i].isNoEntry || g_windows[i].minimized))
            continue;

        HRGN coverRgn = CreateRectRgnIndirect(&coverBounds);
        CombineRgn(visible, visible, coverRgn, RGN_DIFF);
        DeleteObject(coverRgn);
    }

    RECT box;
    int rgnType = GetRgnBox(visible, &box);
    DeleteObject(visible);
    return (rgnType != NULLREGION && rgnType != ERROR);
}

static int GatherObstacles(CollisionOptions opts, RECT *out, int maxOut)
{
    int n = 0;
    for (int i = 0; i < g_noEntryZoneCount && n < maxOut; i++)
        out[n++] = g_noEntryZones[i];

    for (int i = 0; i < g_windowCount && n < maxOut; i++)
    {
        if (i == opts.excludeIndex)
            continue;
        if (opts.excludeChildren && opts.excludeIndex >= 0 && Hierarchy_IsDescendantOf(i, opts.excludeIndex))
            continue;

        GameWindowData *d = &g_windows[i];
        if (!d->hwnd || d->minimized)
            continue;

        if (d->isNoEntry)
        {
            RECT bnd[4];
            int c = NoEntry_GetBoundaryRects(i, bnd);
            for (int b = 0; b < c && n < maxOut; b++)
                out[n++] = bnd[b];
        }
        else if (opts.checkNormalWindows && d->solid)
        {
            if (IsNormalWindowVisibleFromExcluded(i, opts.excludeIndex))
            {
                RECT wb;
                GetWindowFullBounds(d->hwnd, &wb);
                if (n < maxOut)
                    out[n++] = wb;
            }
        }
        else if (opts.checkNormalWindows && IsButtonWindowKind(d->kind) && n < maxOut)
        {
            /* CollisionFilter.CreateStandardOptionsは、CheckButtonsをCheckNormalWindowsと
               同じ「移動中のウィンドウがNoEntryである」条件に結び付けている。 */
            RECT wb;
            GetWindowFullBounds(d->hwnd, &wb);
            out[n++] = wb;
        }
    }

    /* CollisionFilter.CreateStandardOptionsは、CheckPlayerも同じ「移動中の
       ウィンドウがNoEntryである」フラグに結び付けている: NoEntryのMovable/
       Resizableウィンドウは、プレイヤーがその内部（またはその子孫の内部）に
       現在立っている場合を除き、プレイヤーを押し抜くことはできない
       -- 親は自分の子を押しのけられない。 */
    if (opts.checkNormalWindows && n < maxOut)
    {
        Player *p = Player_GetActive();
        if (p && !(opts.excludeIndex >= 0 && p->parentIdx >= 0 &&
                   Hierarchy_ChainContains(p->parentIdx, opts.excludeIndex)))
        {
            RECT pb;
            Player_GetBounds(p, &pb);
            out[n++] = pb;
        }
    }
    return n;
}

RECT Collision_ValidatePosition(RECT current, RECT proposed, CollisionOptions opts)
{
    RECT obstacles[MAX_OBSTACLES];
    int n = GatherObstacles(opts, obstacles, MAX_OBSTACLES);

    int width = current.right - current.left;
    int height = current.bottom - current.top;

    /* 最終位置だけでなく、各軸ごとにcurrent->proposedの経路全体をスイープすることで、
       高速なドラッグが1フレームで薄い障害物をすり抜けないようにする。
       SweepBoundsHelper.CreateSweepBoundsXAxis/YAxisと厳密に一致。 */
    int useX = proposed.left;
    {
        int minX = (current.left < proposed.left) ? current.left : proposed.left;
        int maxX = (current.right > (proposed.left + width)) ? current.right : (proposed.left + width);
        RECT xMove = {minX, current.top, maxX, current.top + height};
        int bestDist = INT_MAX;
        int found = 0;
        for (int i = 0; i < n; i++)
        {
            RECT o = obstacles[i];
            if (!RectsOverlap(xMove, o))
                continue;
            int candidateX = (proposed.left > current.left) ? (o.left - width) : o.right;
            int dist = abs(candidateX - current.left);
            if (dist < bestDist)
            {
                bestDist = dist;
                useX = candidateX;
                found = 1;
            }
        }
        if (!found)
            useX = proposed.left;
    }

    int useY = proposed.top;
    {
        int minY = (current.top < proposed.top) ? current.top : proposed.top;
        int maxY = (current.bottom > (proposed.top + height)) ? current.bottom : (proposed.top + height);
        RECT yMove = {useX, minY, useX + width, maxY};
        int bestDist = INT_MAX;
        int found = 0;
        for (int i = 0; i < n; i++)
        {
            RECT o = obstacles[i];
            if (!RectsOverlap(yMove, o))
                continue;
            int candidateY = (proposed.top > current.top) ? (o.top - height) : o.bottom;
            int dist = abs(candidateY - current.top);
            if (dist < bestDist)
            {
                bestDist = dist;
                useY = candidateY;
                found = 1;
            }
        }
        if (!found)
            useY = proposed.top;
    }

    RECT result = {useX, useY, useX + width, useY + height};
    return result;
}

int Collision_CheckOverlap(RECT bounds, CollisionOptions opts)
{
    RECT obstacles[MAX_OBSTACLES];
    int n = GatherObstacles(opts, obstacles, MAX_OBSTACLES);
    for (int i = 0; i < n; i++)
        if (RectsOverlap(bounds, obstacles[i]))
            return 1;
    return 0;
}

SIZE Collision_ValidateSize(RECT current, SIZE proposed, CollisionOptions opts)
{
    return Collision_ValidateSizeEx(current, proposed, opts, MIN_WINDOW_SIZE, MAX_WINDOW_SIZE);
}

SIZE Collision_ValidateSizeEx(RECT current, SIZE proposed, CollisionOptions opts, int minSize, int maxSize)
{
    RECT obstacles[MAX_OBSTACLES];
    int n = GatherObstacles(opts, obstacles, MAX_OBSTACLES);

    int currentW = current.right - current.left;
    int currentH = current.bottom - current.top;
    int isGrowingW = proposed.cx > currentW;
    int isGrowingH = proposed.cy > currentH;

    int minWidth = proposed.cx;
    if (isGrowingW)
    {
        RECT xResize = {current.left, current.top, current.left + proposed.cx, current.top + currentH};
        int best = proposed.cx;
        for (int i = 0; i < n; i++)
        {
            RECT o = obstacles[i];
            if (!RectsOverlap(xResize, o))
                continue;
            int candidateW = o.left - current.left;
            if (candidateW < best)
                best = candidateW;
        }
        minWidth = best;
    }

    int effectiveW = isGrowingW ? minWidth : currentW;
    int minHeight = proposed.cy;
    if (isGrowingH)
    {
        RECT yResize = {current.left, current.top, current.left + effectiveW, current.top + proposed.cy};
        int best = proposed.cy;
        for (int i = 0; i < n; i++)
        {
            RECT o = obstacles[i];
            if (!RectsOverlap(yResize, o))
                continue;
            int candidateH = o.top - current.top;
            if (candidateH < best)
                best = candidateH;
        }
        minHeight = best;
    }

    if (minWidth < minSize)
        minWidth = minSize;
    if (minWidth > maxSize)
        minWidth = maxSize;
    if (minHeight < minSize)
        minHeight = minSize;
    if (minHeight > maxSize)
        minHeight = maxSize;

    SIZE result = {minWidth, minHeight};
    return result;
}
