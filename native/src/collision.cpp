#include "collision.h"
#include "gamewindow.h"
#include "noentry.h"
#include "hierarchy.h"
#include "player.h"
#include "zorder.h"
#include "windowquery.h"
#include "gdiobj.h"
#include <stdlib.h>
#include <limits.h>

#define MAX_OBSTACLES 160

static int RectsOverlap(RECT a, RECT b)
{
    return WindowQuery_RectsOverlap(a, b);
}

/* ZOrderVisibilityService.IsWindowVisibleFromNoEntryと一致: NoEntry
   ウィンドウが移動/リサイズ中のとき、通常の実体ウィンドウは、より前面の
   何かに完全に隠れていない場合のみ障害物として扱う。ここで不透明な遮蔽物と
   なるのは他のNoEntryウィンドウのみ（通常ウィンドウ同士は「透明」であり
   互いを隠さない） -- ただしオリジナルから引き継いだ意図的な例外が一つある:
   excludeIndexの親ウィンドウは、NoEntryでなくても常に遮蔽物として扱われる。
   これにより、ウィンドウが自分の親の背後をすり抜けられないようにしている。
   Z-order+Region方式の可視性判定コア自体はnoentry.cppのNoEntry_
   GetVisiblePortionと同一アルゴリズムのため、NoEntry_ComputeVisibleRegionを
   共有し、遮蔽条件だけをここで述語として渡す。 */
static int IsNormalWindowVisibleFromExcluded(int windowIndex, int excludeIndex)
{
    GameWindowData *win = &g_windows[windowIndex];
    RECT windowBounds;
    GetWindowFullBounds(win->hwnd, &windowBounds);

    int excludeParentIdx = (excludeIndex >= 0) ? g_windows[excludeIndex].parentIdx : -1;
    auto isOccluder = [excludeIndex, excludeParentIdx](int i)
    {
        if (i == excludeIndex) /* 移動中のウィンドウ自身が自分の障害物を隠すことはない */
            return false;
        if (i == excludeParentIdx)
            return true;
        return g_windows[i].isNoEntry != 0 && !g_windows[i].minimized;
    };
    return NoEntry_ComputeVisibleRegion(windowIndex, windowBounds, isOccluder, nullptr);
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
#ifdef ENABLE_STAGE_EDITOR
        /* パレットアイコン/ツールバーボタン等のエディター自身のUI要素は、
           テストステージに配置したウィンドウの障害物として扱わない --
           そうしないと不可侵ウィンドウ等をパレット付近に動かした際に、
           見た目上は無関係なエディターUIに衝突判定でぶつかって止まって
           しまい、パレット周辺での配置・移動がやりにくくなる。 */
        if (d->isEditorChrome)
            continue;
#endif

        if (d->isNoEntry)
        {
            /* NoEntry_GetBoundaryRects単体のAABBではなく、より前面の
               ウィンドウに隠れている部分を除外したNoEntry_GetVisibleBoundaryRects
               を使う（noentry.h参照）。以前はここが素のNoEntry_GetBoundaryRects
               を直接使っており、NoEntryウィンドウの境界の手前に別のウィンドウが
               重なっているだけで、実際には見えていない境界にMovable/Resizable
               の移動・リサイズが弾かれてしまっていた（実際に報告された不具合:
               元のC#実装と挙動が異なる）。 */
            RECT bnd[4];
            int c = NoEntry_GetVisibleBoundaryRects(i, bnd);
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
        /* 最小化中のプレイヤーはPlayer_GetBoundsが最小化直前の(凍結された)
           論理位置をそのまま返し続ける -- ShowWindow(SW_MINIMIZE)される
           実HWNDの矩形ではなくp->x/y/width/heightを見ているため、実際の
           見た目としてはタスクバーへ隠れているにもかかわらず、その元の
           位置に幽霊のような当たり判定が残り続けてしまっていた（実際に
           報告された不具合: 不可侵ウィンドウを動かすと最小化中のプレイヤー
           にぶつかる）。他の種別のウィンドウを最小化時に障害物から除外する
           のと同じく、最小化中はここでも除外する。 */
        if (p && !p->isMinimized &&
            !(opts.excludeIndex >= 0 && p->parentIdx >= 0 &&
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

/* Collision_ValidateSizeExはcurrent.left/topを固定して右/下方向にのみ
   伸びる前提で障害物を探す。WT_UNCONSTRAINED(制限なしリサイズ+反転)は
   flip中の軸ではアンカーを右/下端として固定し左/上方向へ伸びるため、
   その前提では反転側の障害物を検出できない（既知の制約だった）。
   本関数はアンカー基準で伸長方向をflipX/flipYごとに切り替えることで、
   反転中でも実際に伸びている側の障害物を正しく検出する。
   currentAbs: 直前フレームでコミット済みの絶対サイズ（成長中かどうかの
   判定と、直交する軸のチェック矩形の一辺に使う）。 */
SIZE Collision_ValidateSizeFromAnchor(POINT anchor, int flipX, int flipY,
                                       SIZE currentAbs, SIZE proposed,
                                       CollisionOptions opts, int minSize, int maxSize)
{
    RECT obstacles[MAX_OBSTACLES];
    int n = GatherObstacles(opts, obstacles, MAX_OBSTACLES);

    int isGrowingW = proposed.cx > currentAbs.cx;
    int isGrowingH = proposed.cy > currentAbs.cy;

    int curVisTop = flipY ? (anchor.y - currentAbs.cy) : anchor.y;

    int minWidth = proposed.cx;
    if (isGrowingW)
    {
        RECT xResize = flipX
            ? RECT{anchor.x - proposed.cx, curVisTop, anchor.x, curVisTop + currentAbs.cy}
            : RECT{anchor.x, curVisTop, anchor.x + proposed.cx, curVisTop + currentAbs.cy};
        int best = proposed.cx;
        for (int i = 0; i < n; i++)
        {
            RECT o = obstacles[i];
            if (!RectsOverlap(xResize, o))
                continue;
            int candidateW = flipX ? (anchor.x - o.right) : (o.left - anchor.x);
            if (candidateW < best)
                best = candidateW;
        }
        minWidth = best;
    }

    int effectiveW = isGrowingW ? minWidth : currentAbs.cx;
    int curVisLeft = flipX ? (anchor.x - effectiveW) : anchor.x;

    int minHeight = proposed.cy;
    if (isGrowingH)
    {
        RECT yResize = flipY
            ? RECT{curVisLeft, anchor.y - proposed.cy, curVisLeft + effectiveW, anchor.y}
            : RECT{curVisLeft, anchor.y, curVisLeft + effectiveW, anchor.y + proposed.cy};
        int best = proposed.cy;
        for (int i = 0; i < n; i++)
        {
            RECT o = obstacles[i];
            if (!RectsOverlap(yResize, o))
                continue;
            int candidateH = flipY ? (anchor.y - o.bottom) : (o.top - anchor.y);
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
