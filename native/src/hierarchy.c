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

/* Goal/ボタン共通: origSizeGenが現在の世代と一致しなければ、このジェスチャー
   （または「今まさにこの親に子として現れた瞬間」）の基準をその場で確立する。
   Goal_UpdateParent/Button_UpdateParentは毎フレーム無条件に親子判定を
   行うため、ドラッグ中のウィンドウにGoal/ボタンが後から新しく子として
   現れることは（プレイヤーが歩いて部屋に入るのと同様に）普通に起こりうる。
   単に現在の実矩形をそのまま基準にしてしまうと、確立したその同じフレームで
   newSize = currentSize * scale（scaleはジェスチャー開始からの累積値）が
   即座に適用されてしまい、既にある程度リサイズが進行した状態のウィンドウに
   入った瞬間、その累積分だけ大きさ・位置が一気にジャンプする（実際に
   報告された不具合: 親を持たないGoal/ボタンがリサイズ中のウィンドウに
   入ると急激に大きさが変わる）。
   逆算方式（origSize = currentSize / scale、origOffset = currentOffset /
   scale）で基準を求めることで、確立した瞬間はnewSize = origSize * scaleが
   ちょうど現在のサイズ・位置に戻り（丸め誤差の範囲でジャンプなし）、以降の
   フレームだけがそこからの本当のスケール変化を反映する。通常の子ウィンドウ
   に対する遅延確立(236-244行目)は現在の実矩形をそのまま基準にする単純な
   方式だが、それらは本来ドラッグ中の相手にしか新しく子にならない
   （Hierarchy_CheckAndUpdateは操作対象自身の親子判定のみを行う）ため
   この問題が実質的に起こらない -- Goal/ボタンのように毎フレーム無条件に
   再判定される場合だけ、この逆算が必要になる。 */
static void EnsureOrigBoundsForGeneration(GameWindowData *w, RECT oldRect, RECT newRect, float scaleX, float scaleY)
{
    if (w->origSizeGen == g_resizeGeneration)
        return;

    RECT cur;
    GetWindowFullBounds(w->hwnd, &cur);

    int origW = RoundToNearest((float)(cur.right - cur.left) / scaleX);
    int origH = RoundToNearest((float)(cur.bottom - cur.top) / scaleY);
    if (origW < 1)
        origW = 1;
    if (origH < 1)
        origH = 1;
    int origLeft = oldRect.left + RoundToNearest((float)(cur.left - newRect.left) / scaleX);
    int origTop = oldRect.top + RoundToNearest((float)(cur.top - newRect.top) / scaleY);

    w->origBoundsAtResizeStart.left = origLeft;
    w->origBoundsAtResizeStart.top = origTop;
    w->origBoundsAtResizeStart.right = origLeft + origW;
    w->origBoundsAtResizeStart.bottom = origTop + origH;
    w->origSizeGen = g_resizeGeneration;
}

static void ApplyScaleToGoalIfParented(int rootIndex, RECT oldRect, RECT newRect, float scaleX, float scaleY,
                                        int minSize, int maxSize)
{
    int goalIdx = FindGoalIndex();
    if (goalIdx < 0)
        return;
    GameWindowData *goal = &g_windows[goalIdx];
    if (goal->parentIdx != rootIndex || !goal->hwnd || goal->minimized)
        return;

    EnsureOrigBoundsForGeneration(goal, oldRect, newRect, scaleX, scaleY);
    RECT goalOldRect = goal->origBoundsAtResizeStart;

    /* 以前はサイズ上限・下限として常にGoal自身の固定値(20x20)を使っていた
       が、これは呼び出し元(UpdateResizable/UpdateUnconstrained)がrootIndex
       自身の子に課すminSize/maxSizeと無関係だった。制限なしリサイズ
       (UNCONSTRAINED_MIN_ABS_SIZE=20)ならたまたま一致して問題にならないが、
       通常の子ウィンドウ・プレイヤーと同じminSize/maxSizeを使わないと、
       室内がその制限まで縮んでも辻褄が合わなくなる不整合が起きうる
       （実際に報告された不具合: 制限なしリサイズウィンドウの子のとき、
       プレイヤーやウィンドウのようにならない）。通常の子ウィンドウ
       (Hierarchy_ApplyRelativeTransform本体)と全く同じclampを使う。 */
    int newW = RoundToNearest((float)(goalOldRect.right - goalOldRect.left) * scaleX);
    int newH = RoundToNearest((float)(goalOldRect.bottom - goalOldRect.top) * scaleY);
    if (newW < minSize)
        newW = minSize;
    if (newW > maxSize)
        newW = maxSize;
    if (newH < minSize)
        newH = minSize;
    if (newH > maxSize)
        newH = maxSize;
    int newX = newRect.left + RoundToNearest((float)(goalOldRect.left - oldRect.left) * scaleX);
    int newY = newRect.top + RoundToNearest((float)(goalOldRect.top - oldRect.top) * scaleY);

    /* Player_ApplyParentRelativeTransformのAdjustPositionAfterResizeと同じ
       安全策: スケール追従後の位置が親の現在の矩形をはみ出さないよう、
       その場でクランプし直す。newW/newHは親と同じminSize/maxSizeで
       既にクランプ済みなので親の幅/高さを超えることはなく、このクランプは
       常に親の内側に収まる位置を返せる。これが無いと、丸め誤差や
       (goalOldRect.left - oldRect.left)がゲスチャー開始時点の相対位置に
       基づく古い基準のままなことの影響で、親が非常に小さく縮んだ際に
       Goalが親の外へはみ出して見えることがあった（実際に報告された不具合:
       プレイヤーやウィンドウと違って途中で縮まなくなり最終的にはみ出る）。 */
    int maxX = newRect.right - newW;
    int maxY = newRect.bottom - newH;
    if (newX < newRect.left)
        newX = newRect.left;
    if (newX > maxX)
        newX = maxX;
    if (newY < newRect.top)
        newY = newRect.top;
    if (newY > maxY)
        newY = maxY;

    /* SWP_NOREDRAW: 位置とサイズが同時に変わるため、これを付けないとOS側が
       古い内容を新しい位置/サイズへ引き伸ばして即座に描画してしまうことが
       ある（通常の子ウィンドウ/プレイヤーと同じ対策）。 */
    SetWindowPos(goal->hwnd, NULL, newX, newY, newW, newH, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
    InvalidateRect(goal->hwnd, NULL, FALSE);
    UpdateWindow(goal->hwnd);
}

static void ApplyScaleToButtonsIfParented(int rootIndex, RECT oldRect, RECT newRect, float scaleX, float scaleY,
                                           int minSize, int maxSize)
{
    for (int i = 0; i < g_windowCount; i++)
    {
        GameWindowData *btn = &g_windows[i];
        if (!IsButtonWindowKind(btn->kind) || btn->parentIdx != rootIndex || !btn->hwnd || btn->minimized)
            continue;

        EnsureOrigBoundsForGeneration(btn, oldRect, newRect, scaleX, scaleY);
        RECT btnOldRect = btn->origBoundsAtResizeStart;

        /* Goalと同じ理由で、ボタン自身の固定値(150x40)ではなく呼び出し元の
           minSize/maxSizeでclampする -- 以前は制限なしリサイズで室内が
           150x40よりずっと小さく(最小20px、反転も)なってもボタンだけが
           150x40で頭打ちのまま縮まなくなり、室内からはみ出して見える
           不具合があった（実際に報告された不具合）。 */
        int newW = RoundToNearest((float)(btnOldRect.right - btnOldRect.left) * scaleX);
        int newH = RoundToNearest((float)(btnOldRect.bottom - btnOldRect.top) * scaleY);
        if (newW < minSize)
            newW = minSize;
        if (newW > maxSize)
            newW = maxSize;
        if (newH < minSize)
            newH = minSize;
        if (newH > maxSize)
            newH = maxSize;
        int newX = newRect.left + RoundToNearest((float)(btnOldRect.left - oldRect.left) * scaleX);
        int newY = newRect.top + RoundToNearest((float)(btnOldRect.top - oldRect.top) * scaleY);

        /* Goalと同じ安全策: 親の現在の矩形をはみ出さないようクランプする。 */
        int maxX = newRect.right - newW;
        int maxY = newRect.bottom - newH;
        if (newX < newRect.left)
            newX = newRect.left;
        if (newX > maxX)
            newX = maxX;
        if (newY < newRect.top)
            newY = newRect.top;
        if (newY > maxY)
            newY = maxY;

        SetWindowPos(btn->hwnd, NULL, newX, newY, newW, newH, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
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
   親に対する相対位置・相対サイズを保つ（Player_ApplyParentRelativeTransform）。
   Goal/ボタンも、通常の子ウィンドウ(このすぐ下のHierarchy_ApplyRelativeTransform
   本体)と全く同じ「親に対する相対位置・相対サイズを保つ」規則で追従させる
   -- 以前は元のC#実装に合わせてサイズのみ変更・位置固定だったが、それだと
   親をリサイズするたびに親の中での相対位置がズレていってしまっていた
   （実際に報告された不具合）。 */
static void ApplyScaleToSpecialChildren(int rootIndex, RECT oldRect, RECT newRect, float scaleX, float scaleY,
                                         int minSize, int maxSize)
{
    Player *p = Player_GetActive();
    if (p && p->parentIdx == rootIndex)
        Player_ApplyParentRelativeTransform(p, rootIndex, newRect);
    ApplyScaleToGoalIfParented(rootIndex, oldRect, newRect, scaleX, scaleY, minSize, maxSize);
    ApplyScaleToButtonsIfParented(rootIndex, oldRect, newRect, scaleX, scaleY, minSize, maxSize);
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

    ApplyScaleToSpecialChildren(rootIndex, oldRect, newRect, scaleX, scaleY, minSize, maxSize);

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

        /* SWP_NOREDRAW: 位置とサイズが同時に変わる場合、これを付けないとOS側が
           SetWindowPosの中で古い内容を新しい位置/サイズへ引き伸ばして即座に
           描画してしまうことがあり、すぐ下の同期的なInvalidateRect+
           UpdateWindowによる正しい描画で1フレームごとに上書きされる形になって
           がくがくして見える（UpdateUnconstrainedの反転時と同じ原因）。 */
        SetWindowPos(child->hwnd, NULL, newX, newY, newW, newH, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
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
   保つ。Goal/ボタンも、直接の子として乗っている間は通常の子ウィンドウ・
   プレイヤーと同じくここで鏡映する（下のMirrorHwndWithinParent呼び出し）
   -- 以前は「常に位置固定」という旧設計のまま対象外にしていたため、制限
   なしリサイズウィンドウが反転してもGoal/ボタンだけ反転前の（今となっては
   間違った側の）位置に取り残されていた（実際に報告された不具合）。 */
/* Hierarchy_PropagateMoveは、Goal_UpdateParent/Button_UpdateParentが
   childIdx[]にも登録するGoal/ボタンを意図的にスキップする -- UpdateMovable
   等の一部の呼び出し元は、Goal/ボタン自身の移動を祖先チェーンを辿る専用
   ロジックで別途処理しており、汎用ループでも動かすと同じdx/dyが1フレームで
   二重に適用されてしまうため。しかしHierarchy_MirrorDirectChildrenには
   そのような専用処理が無い -- 直接の子（鏡映済み）の内部にネストした
   Goal/ボタンは、鏡映で動いた分だけ平行移動させないと取り残されてしまう
   （実際に報告された不具合: 制限なしリサイズウィンドウの孫として置いた
   Goal/ボタンがきれいに反転しない）。直接の子自身は鏡映で正しい側へ動く
   ため、ここでは孫以下のGoal/ボタンだけを対象に、子が動いた分の平行移動
   だけを適用する（孫自身を鏡映する必要はない）。 */
static void PropagateMoveToNestedGoalAndButtons(int index, int dx, int dy)
{
    GameWindowData *data = GetWindowData(index);
    if (!data || (dx == 0 && dy == 0))
        return;
    for (int i = 0; i < data->childCount; i++)
    {
        GameWindowData *child = GetWindowData(data->childIdx[i]);
        if (!child || !child->hwnd)
            continue;
        if (child->kind == WT_GOAL || IsButtonWindowKind(child->kind))
        {
            RECT r;
            GetWindowRect(child->hwnd, &r);
            SetWindowPos(child->hwnd, NULL, r.left + dx, r.top + dy, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            InvalidateRect(child->hwnd, NULL, FALSE);
            continue;
        }
        PropagateMoveToNestedGoalAndButtons(data->childIdx[i], dx, dy);
    }
}

static void MirrorHwndWithinParent(HWND hwnd, RECT rootBounds, int mirrorX, int mirrorY)
{
    RECT cb;
    GetWindowRect(hwnd, &cb);
    int newLeft = cb.left;
    int newTop = cb.top;
    if (mirrorX)
        newLeft = rootBounds.left + rootBounds.right - cb.right;
    if (mirrorY)
        newTop = rootBounds.top + rootBounds.bottom - cb.bottom;
    if (newLeft == cb.left && newTop == cb.top)
        return;
    SetWindowPos(hwnd, NULL, newLeft, newTop, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    InvalidateRect(hwnd, NULL, FALSE);
}

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

    int goalIdx = FindGoalIndex();
    if (goalIdx >= 0 && g_windows[goalIdx].parentIdx == rootIndex && g_windows[goalIdx].hwnd &&
        !g_windows[goalIdx].minimized)
        MirrorHwndWithinParent(g_windows[goalIdx].hwnd, rootBounds, mirrorX, mirrorY);

    for (int b = 0; b < g_windowCount; b++)
    {
        GameWindowData *btn = &g_windows[b];
        if (!IsButtonWindowKind(btn->kind) || btn->parentIdx != rootIndex || !btn->hwnd || btn->minimized)
            continue;
        MirrorHwndWithinParent(btn->hwnd, rootBounds, mirrorX, mirrorY);
    }

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
           子に対する相対配置は保たれる（孫自身を鏡映する必要はない）。
           通常のウィンドウ孫はHierarchy_PropagateMoveが、ネストした
           Goal/ボタンはPropagateMoveToNestedGoalAndButtonsが担当する
           （前者は二重適用防止のため後者を意図的にスキップするので、
           両方呼ぶ必要がある）。 */
        Hierarchy_PropagateMove(root->childIdx[i], dx, dy);
        PropagateMoveToNestedGoalAndButtons(root->childIdx[i], dx, dy);
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
