#include "hierarchy.h"
#include "gamewindow.h"
#include "zorder.h"
#include "player.h"

static int FullyContains(RECT outer, RECT inner)
{
    return inner.left >= outer.left && inner.top >= outer.top &&
           inner.right <= outer.right && inner.bottom <= outer.bottom;
}

int Hierarchy_IsDescendantOf(int candidateIndex, int ancestorIndex)
{
    GameWindowData *anc = GetWindowData(ancestorIndex);
    if (!anc)
        return 0;
    for (int i = 0; i < anc->childCount; i++)
    {
        if (anc->childIdx[i] == candidateIndex)
            return 1;
        if (Hierarchy_IsDescendantOf(candidateIndex, anc->childIdx[i]))
            return 1;
    }
    return 0;
}

void Hierarchy_Attach(int parentIdx, int childIdx)
{
    GameWindowData *parent = GetWindowData(parentIdx);
    GameWindowData *child = GetWindowData(childIdx);
    if (!parent || !child)
        return;
    if (parent->childCount >= MAX_CHILDREN)
        return;
    parent->childIdx[parent->childCount++] = childIdx;
    child->parentIdx = parentIdx;
}

void Hierarchy_Detach(int childIdx)
{
    GameWindowData *child = GetWindowData(childIdx);
    if (!child || child->parentIdx < 0)
        return;
    GameWindowData *parent = GetWindowData(child->parentIdx);
    if (parent)
    {
        for (int i = 0; i < parent->childCount; i++)
        {
            if (parent->childIdx[i] == childIdx)
            {
                for (int j = i; j < parent->childCount - 1; j++)
                    parent->childIdx[j] = parent->childIdx[j + 1];
                parent->childCount--;
                break;
            }
        }
    }
    child->parentIdx = -1;
}

void Hierarchy_PropagateMove(int index, int dx, int dy)
{
    GameWindowData *data = GetWindowData(index);
    if (!data || (dx == 0 && dy == 0))
        return;
    for (int i = 0; i < data->childCount; i++)
    {
        GameWindowData *child = GetWindowData(data->childIdx[i]);
        if (!child || !child->hwnd)
            continue;
        /* GoalとボタンもchildIdx[]に含まれる（Goal_UpdateParent/
           Button_UpdateParentがそこにアタッチすることで、Hierarchy_MinimizeSubtree
           の再帰から到達して非表示にできるようにしている）が、それら自身の
           移動追従はstrategy.cで祖先チェーンを辿る形で別途処理される --
           この汎用ループでも移動させてしまうと、同じdx/dyが1フレームで
           二重に適用されてしまう。 */
        if (child->kind == WT_GOAL || IsButtonWindowKind(child->kind))
            continue;
        RECT r;
        GetWindowRect(child->hwnd, &r);
        SetWindowPos(child->hwnd, NULL, r.left + dx, r.top + dy, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(child->hwnd, NULL, FALSE);
        Hierarchy_PropagateMove(data->childIdx[i], dx, dy);
    }
}

void Hierarchy_RecordOriginalSizes(int rootIndex)
{
    GameWindowData *root = GetWindowData(rootIndex);
    if (!root)
        return;
    for (int i = 0; i < root->childCount; i++)
    {
        GameWindowData *child = GetWindowData(root->childIdx[i]);
        if (!child || !child->hwnd)
            continue;
        RECT r;
        GetWindowRect(child->hwnd, &r);
        child->origSize.cx = r.right - r.left;
        child->origSize.cy = r.bottom - r.top;
        child->origSizeGen = g_resizeGeneration;
        Hierarchy_RecordOriginalSizes(root->childIdx[i]);
    }
}

static int RoundToNearest(float v)
{
    return (v >= 0.0f) ? (int)(v + 0.5f) : (int)(v - 0.5f);
}

/* すべての遅延確立箇所で共有される処理: `gen`が現在のリサイズ世代と一致しな
   ければ、origSize = currentSize/scale（下限1）で逆算し、`gen`を現在値に
   更新する。これにより newSize = origSize*scale がこのフレームでちょうど
   currentSizeになる -- ジェスチャー途中でウィンドウが既にある程度拡大縮小
   された後に子になった場合でも、瞬間的な位置ジャンプが起きない。 */
static void EnsureOrigSizeForGeneration(int *gen, SIZE *origSize, int curW, int curH, float scaleX, float scaleY)
{
    if (*gen == g_resizeGeneration)
        return;
    int w = (int)RoundToNearest((float)curW / scaleX);
    int h = (int)RoundToNearest((float)curH / scaleY);
    origSize->cx = w < 1 ? 1 : w;
    origSize->cy = h < 1 ? 1 : h;
    *gen = g_resizeGeneration;
}

static void ApplyScaleToGoalIfParented(int rootIndex, float scaleX, float scaleY)
{
    int goalIdx = FindGoalIndex();
    if (goalIdx < 0)
        return;
    GameWindowData *goal = &g_windows[goalIdx];
    if (goal->parentIdx != rootIndex || !goal->hwnd || goal->minimized)
        return;

    RECT gb;
    GetWindowFullBounds(goal->hwnd, &gb);
    EnsureOrigSizeForGeneration(&goal->origSizeGen, &goal->origSize, gb.right - gb.left, gb.bottom - gb.top, scaleX, scaleY);

    /* Goal.UpdateTargetSizeは20x20の下限を強制する。位置は固定されたまま
       で、他のすべてのIEffectTargetのリサイズ（サイズのみ変更、位置は
       変更しない）と一致させる。 */
    int newW = RoundToNearest((float)goal->origSize.cx * scaleX);
    int newH = RoundToNearest((float)goal->origSize.cy * scaleY);
    if (newW < 20)
        newW = 20;
    if (newH < 20)
        newH = 20;
    SetWindowPos(goal->hwnd, NULL, 0, 0, newW, newH, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    InvalidateRect(goal->hwnd, NULL, FALSE);
    /* InvalidateRectだけでは再描画が次のメッセージループパスまで遅延され、
       拡大した分の新しい領域が実際に塗りつぶされるまで黒いフラッシュとして
       見えてしまう（Player_ApplyParentScaleと同じ問題）。UpdateWindowで
       同期的にWM_PAINTを強制し、その隙間を埋める。 */
    UpdateWindow(goal->hwnd);
}

static void ApplyScaleToButtonsIfParented(int rootIndex, float scaleX, float scaleY)
{
    for (int i = 0; i < g_windowCount; i++)
    {
        GameWindowData *btn = &g_windows[i];
        if (!IsButtonWindowKind(btn->kind) || btn->parentIdx != rootIndex || !btn->hwnd || btn->minimized)
            continue;

        RECT bb;
        GetWindowFullBounds(btn->hwnd, &bb);
        EnsureOrigSizeForGeneration(&btn->origSizeGen, &btn->origSize, bb.right - bb.left, bb.bottom - bb.top, scaleX, scaleY);

        /* GameButton.UpdateTargetSizeは150x40の下限を強制する。位置は固定
           されたままで、他のすべてのIEffectTargetのリサイズと同様。 */
        int newW = RoundToNearest((float)btn->origSize.cx * scaleX);
        int newH = RoundToNearest((float)btn->origSize.cy * scaleY);
        if (newW < 150)
            newW = 150;
        if (newH < 40)
            newH = 40;
        SetWindowPos(btn->hwnd, NULL, 0, 0, newW, newH, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(btn->hwnd, NULL, FALSE);
        UpdateWindow(btn->hwnd); /* ゴール/子ウィンドウと同じ黒フラッシュ対策 */
    }
}

void Hierarchy_ApplyScale(int rootIndex, float scaleX, float scaleY)
{
    GameWindowData *root = GetWindowData(rootIndex);
    if (!root)
        return;

    /* オリジナル実装ではPlayerとGoalは実際のIEffectTarget子要素であり
       （通常のネストされたウィンドウと同様にGameWindow.AddChildで追加される）、
       BaseWindowEffect.Applyの再帰的な走査はどの深さに親を持っていても
       それらに到達する -- ユーザーがドラッグしているウィンドウの直下に
       いる場合だけではない。すべての再帰レベルでチェックする（最上位だけ
       ではなく）ことで、孫の中にネストされたプレイヤーやゴールも正しく
       伝播したスケールを受け取れる。 */
    Player *p = Player_GetActive();
    if (p && p->parentIdx == rootIndex)
    {
        EnsureOrigSizeForGeneration(&p->origSizeGen, &p->origSize, p->width, p->height, scaleX, scaleY);
        Player_ApplyParentScale(p, rootIndex, scaleX, scaleY);
    }
    ApplyScaleToGoalIfParented(rootIndex, scaleX, scaleY);
    ApplyScaleToButtonsIfParented(rootIndex, scaleX, scaleY);

    for (int i = 0; i < root->childCount; i++)
    {
        GameWindowData *child = GetWindowData(root->childIdx[i]);
        if (!child || !child->hwnd)
            continue;
        /* Goalとボタン自身のリサイズ追従（20x20 / 150x40の下限、このループの
           100x100とは異なる）は上ですでに個別処理済み -- ここではスキップし、
           異なる2つのサイズ下限で二重にリサイズされないようにする。 */
        if (child->kind == WT_GOAL || IsButtonWindowKind(child->kind))
            continue;

        /* Hierarchy_RecordOriginalSizesがこのドラッグに対してすでに実行済み
           の後で（別ウィンドウのマウスアップ時にHierarchy_CheckAndUpdate経由で
           再親化されて）ジェスチャー途中に子になったウィンドウのための
           セーフティネット。 */
        if (child->origSizeGen != g_resizeGeneration)
        {
            RECT cr;
            GetWindowRect(child->hwnd, &cr);
            EnsureOrigSizeForGeneration(&child->origSizeGen, &child->origSize,
                                         cr.right - cr.left, cr.bottom - cr.top, scaleX, scaleY);
        }

        int newW = RoundToNearest((float)child->origSize.cx * scaleX);
        int newH = RoundToNearest((float)child->origSize.cy * scaleY);
        if (newW < MIN_WINDOW_SIZE)
            newW = MIN_WINDOW_SIZE;
        if (newW > MAX_WINDOW_SIZE)
            newW = MAX_WINDOW_SIZE;
        if (newH < MIN_WINDOW_SIZE)
            newH = MIN_WINDOW_SIZE;
        if (newH > MAX_WINDOW_SIZE)
            newH = MAX_WINDOW_SIZE;

        SetWindowPos(child->hwnd, NULL, 0, 0, newW, newH,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(child->hwnd, NULL, FALSE);
        UpdateWindow(child->hwnd); /* ゴールと同じ黒フラッシュ対策: 拡大直後に同期再描画 */

        /* ApplyScaleToChildrenRecursiveは、この子自身のmin/maxクランプ適用後に
           actualScale = newSize/origSizeを再計算し、それを孫に伝播する
           （ルートから渡された生のスケールではない）-- そのため、階層の途中で
           100x100の下限に張り付いた子でも、古くなったスケール係数で過剰/過小に
           縮小することなく、実際に着地したサイズを基準に自身の子孫を正しく
           スケーリングできる。 */
        float actualScaleX = (float)newW / (float)child->origSize.cx;
        float actualScaleY = (float)newH / (float)child->origSize.cy;

        Hierarchy_ApplyScale(root->childIdx[i], actualScaleX, actualScaleY);
    }
}

void Hierarchy_CheckAndUpdate(int movedIndex)
{
    GameWindowData *moved = GetWindowData(movedIndex);
    if (!moved || !moved->hwnd)
        return;

    RECT movedBounds;
    GetWindowFullBounds(moved->hwnd, &movedBounds);

    if (moved->parentIdx >= 0)
    {
        GameWindowData *parent = GetWindowData(moved->parentIdx);
        RECT parentBounds;
        GetWindowFullBounds(parent->hwnd, &parentBounds);
        if (!FullyContains(parentBounds, movedBounds))
            Hierarchy_Detach(movedIndex);
    }

    for (int i = moved->childCount - 1; i >= 0; i--)
    {
        GameWindowData *child = GetWindowData(moved->childIdx[i]);
        RECT childBounds;
        GetWindowFullBounds(child->hwnd, &childBounds);
        if (!FullyContains(movedBounds, childBounds))
            Hierarchy_Detach(moved->childIdx[i]);
    }

    int bestParent = -1;
    int bestZ = -1;
    for (int i = 0; i < g_windowCount; i++)
    {
        if (i == movedIndex)
            continue;
        if (Hierarchy_IsDescendantOf(i, movedIndex))
            continue;
        GameWindowData *cand = GetWindowData(i);
        /* WindowHierarchyManager.CheckPotentialParentWindowは常にWindowManager
           の`windows`リスト（GameWindowインスタンスを保持）のみを走査する --
           Goalとボタンは別途管理されており、そこでは親子候補として現れない。 */
        if (!IsQueryableWindow(cand->kind))
            continue;
        if (!cand->hwnd)
            continue;
        RECT candBounds;
        GetWindowFullBounds(cand->hwnd, &candBounds);
        if (!FullyContains(candBounds, movedBounds))
            continue;
        if (!ZOrder_IsInFront(moved->hwnd, cand->hwnd))
            continue;
        int z = ZOrder_GetIndex(cand->hwnd);
        if (z <= bestZ)
            continue;
        /* 切り替える価値があるためには、候補は現在の親（存在する場合）より
           前面にある必要がある。 */
        if (moved->parentIdx >= 0 && !ZOrder_IsInFront(cand->hwnd, g_windows[moved->parentIdx].hwnd))
            continue;
        bestZ = z;
        bestParent = i;
    }
    if (bestParent >= 0 && bestParent != moved->parentIdx)
    {
        if (moved->parentIdx >= 0)
            Hierarchy_Detach(movedIndex);
        Hierarchy_Attach(bestParent, movedIndex);
        return; /* WindowHierarchyManagerと一致させる: 再親化後は停止し、以下の子検索はスキップする */
    }

    for (int i = 0; i < g_windowCount; i++)
    {
        if (i == movedIndex)
            continue;
        GameWindowData *cand = GetWindowData(i);
        /* 既存のサブツリー全体（子および孫など）を除外する。これは
           WindowHierarchyManagerの existingGroup = {operatedWindow} U
           GetAllDescendants(operatedWindow) による、子候補走査時のフィルタと
           一致させるもの。これがないと、直接の親がZ-orderで`moved`より背面に
           いる孫が、`moved`に直接引き剥がされてフラット化されてしまう可能性
           がある。 */
        if (Hierarchy_IsDescendantOf(i, movedIndex))
            continue;
        if (Hierarchy_IsDescendantOf(movedIndex, i))
            continue;
        if (!IsQueryableWindow(cand->kind))
            continue;
        if (!cand->hwnd)
            continue;
        RECT candBounds;
        GetWindowFullBounds(cand->hwnd, &candBounds);
        if (!FullyContains(movedBounds, candBounds))
            continue;
        if (!ZOrder_IsInFront(cand->hwnd, moved->hwnd))
            continue;
        /* 候補を現在の（より背面に位置する）親から奪う。WindowHierarchyManagerの
           「operatedWindowが旧親よりも前面にある」という条件と一致させる。 */
        if (cand->parentIdx < 0 || ZOrder_IsInFront(moved->hwnd, g_windows[cand->parentIdx].hwnd))
        {
            if (cand->parentIdx >= 0)
                Hierarchy_Detach(i);
            Hierarchy_Attach(movedIndex, i);
        }
    }
}

int Hierarchy_ChainContains(int startIdx, int targetIdx)
{
    int idx = startIdx;
    int guard = 0;
    while (idx >= 0 && guard++ < MAX_WINDOWS)
    {
        if (idx == targetIdx)
            return 1;
        idx = g_windows[idx].parentIdx;
    }
    return 0;
}

void Hierarchy_MinimizeSubtree(int index)
{
    GameWindowData *data = GetWindowData(index);
    if (!data || !data->hwnd || data->minimized)
        return;

    /* 先に子要素へ再帰する -- 外側のウィンドウが自身を切り離し/非表示にする前に
       `child.OnMinimize()`が呼ばれる（GameWindowの子であれば、さらにその子へと
       カスケードする）挙動を踏襲している。どの深さの子孫も最終的に独立して
       最小化され、親を持たない状態になる。後で祖先を復元しても、それらは
       一切戻らない（GameWindow.OnRestoreはカスケードしない）。 */
    for (int i = data->childCount - 1; i >= 0; i--)
        Hierarchy_MinimizeSubtree(data->childIdx[i]);
    data->childCount = 0;

    if (data->parentIdx >= 0)
        Hierarchy_Detach(index);

    data->minimized = 1;
    ShowWindow(data->hwnd, SW_SHOWMINIMIZED);

    /* オリジナル実装ではPlayerFormも単なる別のIEffectTarget子要素であるため、
       このサブツリー内の他のGameWindow/Goalと同様に直接OnMinimize()呼び出しを
       受け取る -- 物理演算の更新を凍結し、このウィンドウから切り離す。 */
    Player *p = Player_GetActive();
    if (p && p->parentIdx == index)
        Player_OnMinimize(p);
}

void Hierarchy_RestoreWindow(int index)
{
    GameWindowData *data = GetWindowData(index);
    if (!data || !data->hwnd || !data->minimized)
        return;

    data->minimized = 0;
    ShowWindow(data->hwnd, SW_RESTORE);
    InvalidateRect(data->hwnd, NULL, FALSE);

    /* GameWindow.OnRestoreは明示的に親子関係の再検出を自身では行わない
       （「親子判定はOSの復元処理後に実行される」）-- 次のマウス操作による
       CheckPotentialParentWindowパスに依存している。ここにはそれに相当する
       クリック駆動のフックが存在しないため、最も近いネイティブ実装として
       即座に一度だけ再検出を実行する。 */
    Hierarchy_CheckAndUpdate(index);
}
