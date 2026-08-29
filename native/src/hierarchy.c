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
        /* Hierarchy_ApplyRelativeTransform専用: サイズだけでなく、この時点の
           絶対位置も記録しておく。相対オフセット = このrectとrootIndex側の
           開始時点rectとの差分。 */
        child->origBoundsAtResizeStart = r;
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
       見えてしまう（Player_ApplyParentRelativeTransformと同じ問題）。
       UpdateWindowで同期的にWM_PAINTを強制し、その隙間を埋める。 */
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

/* オリジナル実装ではPlayerとGoalは実際のIEffectTarget子要素であり
   （通常のネストされたウィンドウと同様にGameWindow.AddChildで追加される）、
   BaseWindowEffect.Applyの再帰的な走査はどの深さに親を持っていても
   それらに到達する -- ユーザーがドラッグしているウィンドウの直下に
   いる場合だけではない。すべての再帰レベルでチェックする（最上位だけ
   ではなく）ことで、孫の中にネストされたプレイヤーやゴールも正しく
   伝播したスケールを受け取れる。Hierarchy_ApplyRelativeTransformの
   全再帰レベルから呼ばれる。プレイヤーは通常の子ウィンドウと同じく
   親に対する相対位置・相対サイズを保つ（Player_ApplyParentRelativeTransform）
   一方、Goal/ボタンは元のC#実装と同じ「サイズのみ変更、位置固定」のまま
   扱う。 */
static void ApplyScaleToSpecialChildren(int rootIndex, RECT oldRect, RECT newRect, float scaleX, float scaleY)
{
    Player *p = Player_GetActive();
    if (p && p->parentIdx == rootIndex)
        Player_ApplyParentRelativeTransform(p, rootIndex, oldRect, newRect);
    ApplyScaleToGoalIfParented(rootIndex, scaleX, scaleY);
    ApplyScaleToButtonsIfParented(rootIndex, scaleX, scaleY);
}

void Hierarchy_ApplyRelativeTransform(int rootIndex, RECT oldRect, RECT newRect, int minSize, int maxSize)
{
    GameWindowData *root = GetWindowData(rootIndex);
    if (!root)
        return;

    int oldW = oldRect.right - oldRect.left;
    int oldH = oldRect.bottom - oldRect.top;
    if (oldW <= 0 || oldH <= 0)
        return;

    float scaleX = (float)(newRect.right - newRect.left) / (float)oldW;
    float scaleY = (float)(newRect.bottom - newRect.top) / (float)oldH;

    ApplyScaleToSpecialChildren(rootIndex, oldRect, newRect, scaleX, scaleY);

    for (int i = 0; i < root->childCount; i++)
    {
        GameWindowData *child = GetWindowData(root->childIdx[i]);
        if (!child || !child->hwnd)
            continue;
        if (child->kind == WT_GOAL || IsButtonWindowKind(child->kind))
            continue;

        /* ジェスチャー開始時点の記録が無い（Hierarchy_RecordOriginalSizes後に
           途中で子になった）場合は、現在のrectをこの場でベースラインとして
           確立する。 */
        if (child->origSizeGen != g_resizeGeneration)
        {
            RECT cr;
            GetWindowRect(child->hwnd, &cr);
            child->origSize.cx = cr.right - cr.left;
            child->origSize.cy = cr.bottom - cr.top;
            child->origBoundsAtResizeStart = cr;
            child->origSizeGen = g_resizeGeneration;
        }

        RECT childOldRect = child->origBoundsAtResizeStart;

        int newW = RoundToNearest((float)child->origSize.cx * scaleX);
        int newH = RoundToNearest((float)child->origSize.cy * scaleY);
        /* 下限/上限はrootIndex側の呼び出し元から渡される -- 通常のResizable
           はMIN_WINDOW_SIZE(100)、制限なしリサイズはより小さいUNCONSTRAINED_
           MIN_ABS_SIZE(20)を使う（UpdateResizable/UpdateUnconstrained参照）。 */
        if (newW < minSize)
            newW = minSize;
        if (newW > maxSize)
            newW = maxSize;
        if (newH < minSize)
            newH = minSize;
        if (newH > maxSize)
            newH = maxSize;

        /* 親(root)に対する相対オフセットも同じスケールで追従させる -- これに
           より、子は親の中の同じ相対位置・相対サイズを保つ（絶対位置を
           固定していた旧Hierarchy_ApplyScaleとの違い）。 */
        int newX = newRect.left + RoundToNearest((float)(childOldRect.left - oldRect.left) * scaleX);
        int newY = newRect.top + RoundToNearest((float)(childOldRect.top - oldRect.top) * scaleY);

        SetWindowPos(child->hwnd, NULL, newX, newY, newW, newH, SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(child->hwnd, NULL, FALSE);
        UpdateWindow(child->hwnd);

        RECT childNewRect = {newX, newY, newX + newW, newY + newH};
        Hierarchy_ApplyRelativeTransform(root->childIdx[i], childOldRect, childNewRect, minSize, maxSize);
    }
}

void Hierarchy_ToggleInheritedFlip(int rootIndex, int toggleX, int toggleY)
{
    GameWindowData *root = GetWindowData(rootIndex);
    if (!root)
        return;

    /* プレイヤーだけはchildIdx[]には入らずparentIdxのみで追跡されるため
       （Hierarchy_Attachが一切呼ばれない）、再帰の各段でこの段のrootIndexに
       乗っているかを個別にチェックする必要がある。Goal/ボタンはGoal_UpdateParent/
       Button_UpdateParentがHierarchy_Attach経由でchildIdx[]にも登録するため、
       下のchildIdx[]ループ（種別によるスキップが無い）で自然に処理される --
       ここで別途XORすると二重反転で相殺されてしまう（実際に発生していた
       不具合）。 */
    Player *p = Player_GetActive();
    if (p && p->parentIdx == rootIndex)
    {
        if (toggleX)
            p->inheritedFlipX ^= 1;
        if (toggleY)
            p->inheritedFlipY ^= 1;
    }

    for (int i = 0; i < root->childCount; i++)
    {
        GameWindowData *child = GetWindowData(root->childIdx[i]);
        if (!child)
            continue;
        if (toggleX)
            child->inheritedFlipX ^= 1;
        if (toggleY)
            child->inheritedFlipY ^= 1;
        if (child->hwnd)
            InvalidateRect(child->hwnd, NULL, FALSE);

        Hierarchy_ToggleInheritedFlip(root->childIdx[i], toggleX, toggleY);
    }
}

/* `rootIndex`が今まさに反転イベントを起こした瞬間に、Hierarchy_ToggleInheritedFlip
   と一緒に一度だけ呼ぶ。Hierarchy_ApplyRelativeTransform/Player_
   ApplyParentRelativeTransformは常に正のスケール比（大きさの比率）だけで
   子の位置を追従させるため、反転（親の可視矩形の左上そのものが動く/
   入れ替わる）は正しく表現できない -- 反転前に親矩形の下寄りにいた子は、
   その「開始位置からの下寄り具合」がそのまま新しい矩形にも適用され、結果的に
   新しい矩形でも下寄りの位置、つまり反転で見た目上下端に移動したタイトル
   バー側に来てしまう（実際に報告された不具合: プレイヤーのめり込みと、
   そこが天井扱いになり常に「落下中」から抜け出せなくなる）。
   `rootBounds`（反転を反映済みの現在の親矩形）を軸に、直接の子（ウィンドウ
   ・プレイヤー）の位置をmirrorX/mirrorYで指定された軸について正しく鏡映
   する。孫以下は対象の子自身が反転したわけではないので鏡映せず、子が
   動いた分だけHierarchy_PropagateMoveで平行移動させ、内部の相対配置を
   保つ。Goal/ボタンは常に位置固定という既存の設計を崩さないよう対象外。 */
void Hierarchy_MirrorDirectChildren(int rootIndex, RECT rootBounds, int mirrorX, int mirrorY)
{
    if (!mirrorX && !mirrorY)
        return;
    GameWindowData *root = GetWindowData(rootIndex);
    if (!root)
        return;

    Player *p = Player_GetActive();
    if (p && p->parentIdx == rootIndex)
        Player_MirrorWithinParent(p, rootIndex, rootBounds, mirrorX, mirrorY);

    for (int i = 0; i < root->childCount; i++)
    {
        GameWindowData *child = GetWindowData(root->childIdx[i]);
        if (!child || !child->hwnd)
            continue;
        if (child->kind == WT_GOAL || IsButtonWindowKind(child->kind))
            continue;

        RECT cb;
        GetWindowRect(child->hwnd, &cb);
        int newLeft = cb.left;
        int newTop = cb.top;
        if (mirrorX)
            newLeft = rootBounds.left + rootBounds.right - cb.right;
        if (mirrorY)
            newTop = rootBounds.top + rootBounds.bottom - cb.bottom;

        int dx = newLeft - cb.left;
        int dy = newTop - cb.top;
        if (dx == 0 && dy == 0)
            continue;

        SetWindowPos(child->hwnd, NULL, newLeft, newTop, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(child->hwnd, NULL, FALSE);
        /* 子自身は鏡映で移動したが、その内部の孫たちは子から見て相対的には
           何も変わっていない -- 子が動いた分だけ平行移動させれば、孫の
           子に対する相対配置は保たれる（孫自身を鏡映する必要はない）。 */
        Hierarchy_PropagateMove(root->childIdx[i], dx, dy);
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
    /* SW_SHOWMINIMIZEDは「ウィンドウをアクティブ化した上で」最小化表示する
       ため、この最小化されたウィンドウがキーボードフォーカスを持ったままに
       なる。フォーカスを持つ最小化(アイコン化)ウィンドウに方向キー等が
       送られると、Windows標準のアイコンナビゲーション処理が働き行き先が
       無いため警告音が鳴る（実際に報告された不具合）。SW_MINIMIZEは
       アクティブ化せず（Zオーダー上の次のトップレベルウィンドウへ
       フォーカスを譲る）、同じくタスクバー表示・WINDOWPLACEMENTの更新は
       行われるため、ここでは意図的にこちらを使う。 */
    ShowWindow(data->hwnd, SW_MINIMIZE);

    /* オリジナル実装ではPlayerFormも単なる別のIEffectTarget子要素であるため、
       このサブツリー内の他のGameWindow/Goalと同様に直接OnMinimize()呼び出しを
       受け取る -- 物理演算の更新を凍結し、このウィンドウから切り離す。
       Goal/ボタンはGoal_UpdateParent/Button_UpdateParentがHierarchy_Attach
       経由でchildIdx[]にも登録するため、上のchildIdx[]再帰ループ（種別による
       スキップが無い）で通常の子と同じく非表示化・切り離しが行われる --
       ここで別途処理する必要はない（プレイヤーだけはHierarchy_Attachが一切
       呼ばれずparentIdxのみで追跡されるため、この個別チェックが必要）。 */
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
