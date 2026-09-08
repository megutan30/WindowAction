#include "strategy.h"
#include "collision.h"
#include "hierarchy.h"
#include "player.h"
#include "editor.h"
#include <stdlib.h>

int g_requestRestart = 0;
int g_requestNext = 0;
int g_requestToTitle = 0;
int g_requestExit = 0;

/* UpdateMovable専用: `w`(Goal/ボタン)がmovedIndexの祖先チェーンのどこかに
   親を持っていれば、同じactualDx/actualDyだけ平行移動させる。以前はGoal用
   の単発呼び出しとButtonループ用の呼び出しが、同じ「祖先チェーンに含む
   か」ガードと移動処理をそれぞれ個別に書いていた。 */
static void FollowMovedWindowIfDescendant(GameWindowData *w, int movedIndex, int actualDx, int actualDy)
{
    if (w->parentIdx < 0 || w->minimized)
        return;
    if (!Hierarchy_ChainContains(w->parentIdx, movedIndex))
        return;
    RECT b;
    GetWindowFullBounds(w->hwnd, &b);
    SetWindowPos(w->hwnd, NULL, b.left + actualDx, b.top + actualDy, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    InvalidateRect(w->hwnd, NULL, FALSE);
}

/* 以前はここに種別ごとのswitch文があったが、各処理はMovableWindow/
   ResizableWindow/UnconstrainedWindow/MinimizableWindowのOnMouseDown
   オーバーライドへ分かれていた。コンポーネント化により1つのウィンドウが
   複数のcapabilities(移動+リサイズ等)を同時に持てるようになったため、
   単一継承の派生クラスでは表現できなくなり、GameWindowData自身の
   1本のメソッドへ統合した。最小化はウィンドウ全体クリックでの
   トグルをやめ、専用ボタン(GameWindowProcのWM_LBUTTONDOWN、
   GetMinimizeButtonRect参照)だけで行うようにしたため、ここには
   最小化の分岐が無い。 */
void Strategy_HandleMouseDown(int index)
{
    GameWindowData *data = GetWindowData(index);
    if (!data)
        return;
    data->OnMouseDown(index);
}

void GameWindowData::OnMouseDown(int index)
{
    /* 片軸のみ(X軸のみ/Y軸のみ)のリサイズ能力は、ウィンドウ全体ではなく
       該当する縁（当たり判定はGetResizeEdgeZoneRight/Bottom、GameWindowProcの
       WM_SETCURSORと共通）をつかんだ場合だけリサイズを開始する（実際に
       要望された挙動）。両軸を持つ場合はwantResizeが常に真のまま従来通り
       ウィンドウ全体のクリックでリサイズを開始する。縁をつかんだ場合は
       Move能力を同時に持っていても移動を開始させない
       （suppressMove）-- そうしないと以前のようにdragging/resizingが
       同時に立ち、Strategy_UpdateAllのif/else-if順で常に移動が優先されて
       しまい、縁をつかんだ意図（リサイズ）が実現できない。 */
    int resizeX = HasCapability(capabilities, WC_RESIZE_X);
    int resizeY = HasCapability(capabilities, WC_RESIZE_Y);
    int wantResize = resizeX || resizeY;
    int suppressMove = 0;

    if (wantResize && resizeX != resizeY)
    {
        POINT cur;
        GetCursorPos(&cur);
        RECT full;
        GetWindowRect(hwnd, &full);
        RECT edge = resizeX ? GetResizeEdgeZoneRight(full) : GetResizeEdgeZoneBottom(full);
        int onEdge = PtInRect(&edge, cur) != 0;
        wantResize = onEdge;
        suppressMove = onEdge;
    }

    if (!suppressMove && (HasCapability(capabilities, WC_MOVE_X) || HasCapability(capabilities, WC_MOVE_Y)))
    {
        dragging = 1;
        SetCapture(hwnd);
        /* window.PointToClient(Cursor.Position)相当: 画面絶対座標そのままでは
           なく、その時点のウィンドウ左上を基準にした相対座標で記録する
           （UpdateDrag参照 -- この基準の動的な取り直しが、ドラッグ開始位置
           からの累積差分を絶対目標位置へ正しく収束させるために必須）。 */
        POINT curAbs;
        GetCursorPos(&curAbs);
        RECT outer0;
        GetWindowRect(hwnd, &outer0);
        lastMouse.x = curAbs.x - outer0.left;
        lastMouse.y = curAbs.y - outer0.top;
        blockedL = blockedR = blockedU = blockedD = 0;
    }

    if (wantResize)
    {
        resizing = 1;
        SetCapture(hwnd);
        GetCursorPos(&resizeDragStart);

        /* resizeOrigSize/unconstrainedAnchorは常にlogicalW/H(符号付き)+
           アンカー点として統一的に扱う -- 反転を許可しない軸でもこの表現に
           乗せることで、UpdateResize側を軸ごとに分岐させずに済む。反転を
           許可しない軸のlogicalは常に非負にクランプされる(UpdateResize
           参照)ため、意味的には従来のResizableWindow(符号なしouterサイズ)と
           完全に等価になる。 */
        RECT r;
        GetWindowRect(hwnd, &r);
        resizeOrigSize.cx = logicalW;
        resizeOrigSize.cy = logicalH;
        unconstrainedAnchor.x = (logicalW < 0) ? r.right : r.left;
        unconstrainedAnchor.y = (logicalH < 0) ? r.bottom : r.top;

        /* これをインクリメントすると、以前のジェスチャー中に確立されたすべての
           origSize（ウィンドウの子、ゴール、ボタン、プレイヤーすべて）が一括で
           無効化される。以下のHierarchy_RecordOriginalSizesは、今この時点で
           既にアタッチされているものについて即座に再確立する --
           ResizableWindowStrategyのoriginalSizes辞書がStartResizing()の
           たびに空から始まり、RecordOriginalSizesRecursiveによって埋められる
           挙動を踏襲している。このジェスチャーの途中で子になったもの
           （プレイヤーが歩いて入ってくる、ゴール/ボタンがドラッグで
           乗せられる）は、代わりにHierarchy_ApplyRelativeTransformが初めて
           触れられた際に currentSize/scale を遅延的に逆算するフォールバックに
           任される。
           これはオリジナルの "if (!originalSizes.ContainsKey(child))"
           フォールバックと同じで、ドラッグ開始後にウィンドウが既にどれだけ
           拡大縮小していたかに関わらず、いきなりcurrentSize倍にジャンプ
           することを防ぐ。 */
        g_resizeGeneration++;
        Hierarchy_RecordOriginalSizes(index);
    }
}

void Strategy_HandleMouseUp(int index)
{
    GameWindowData *data = GetWindowData(index);
    if (!data)
        return;
    if (data->dragging || data->resizing)
        ReleaseCapture();
    data->dragging = 0;
    data->resizing = 0;

    /* WindowMessageHandler.HandleLeftButtonUpは、マウスアップ時にドラッグ中の
       Movableだけでなく、すべてのGameWindowに対してCheckPotentialParentWindow
       を呼び出す -- 単なる静的なウィンドウをクリックしただけでも親子関係が
       再評価される。 */
    if (IsQueryableWindow(data->kind))
        Hierarchy_CheckAndUpdate(index);
}

/* 以前はここに種別ごとのswitch文があったが、各処理は各ボタンクラスの
   OnClickオーバーライドへ移した(内容は変更していない)。呼び出し元
   (GameWindowProcのWM_LBUTTONUPハンドラ)はg_windows[index].OnClick()を
   直接呼ぶよう変更したため、この関数自体は不要になった。 */

void StartButton::OnClick() { g_requestNext = 1; }
void RetryButton::OnClick() { g_requestRestart = 1; }
void ToTitleButton::OnClick() { g_requestToTitle = 1; }
void ExitButton::OnClick() { g_requestExit = 1; }
#ifdef ENABLE_STAGE_EDITOR
void TestButton::OnClick() { g_requestTest = 1; }
void ExportButton::OnClick() { Editor_ExportStage(); }
void ResetButton::OnClick()
{
    /* hInstanceはstrategy.cからは持っていないため、main.cのリクエスト
       フラグ経由でEditor_LoadTestStageを呼び直す（g_requestTestを
       再利用する: 現在既にテストステージ中でも同じ処理で作り直せる）。 */
    g_requestTest = 1;
}
#endif

/* 以前のUpdateMovableと同じロジックに、軸ごとのcapabilities判定を追加した
   だけ -- WC_MOVE_Xが無ければdxを、WC_MOVE_Yが無ければdyを、衝突判定/
   適用パイプラインに渡す前に0にする。これにより「移動できるが片方の軸
   だけ」というウィンドウも既存のCollision_ValidatePosition以下のロジックを
   一切変えずに実現できる。 */
void GameWindowData::UpdateDrag(int index)
{
    int wantX = HasCapability(capabilities, WC_MOVE_X);
    int wantY = HasCapability(capabilities, WC_MOVE_Y);

    POINT curAbs;
    GetCursorPos(&curAbs);

    RECT current;
    GetWindowFullBounds(hwnd, &current);

    /* MovableWindowStrategy.CalculateMovementと一致させる: window.PointToClient(
       Cursor.Position)は画面絶対座標を「その時点のウィンドウ現在位置」基準の
       相対座標に変換する。lastMouseはOnMouseDownでドラッグ開始時に一度だけ
       記録され、以後このドラッグ中は更新しないが、curは毎フレームウィンドウの
       現在位置を基準に取り直す（PointToClientと同じ）ため、「ドラッグ開始
       位置 + カーソル累積移動量」という絶対目標位置に代数的に収束する:
         P(t) = P(t-1) + [ (cur(t)-outer(t-1)) - lastMouse ]
              = P(t-1) + (C(t)-C(0)) - (P(t-1)-P(0))
              = P(0) + (C(t)-C(0))
       （outerを画面絶対座標のまま使うと、この P(t-1) の相殺項が失われ、
       毎フレーム移動量が累積加算されて暴走する -- 実際に発生した回帰バグ）。
       これにより、ある1フレームで衝突により移動が一部しか適用できなくても、
       次のフレームは常にこの絶対目標へ向けて再計算されるため、カーソルと
       ウィンドウの相対位置がドラッグ中にズレていくことがない。 */
    RECT outerNow;
    GetWindowRect(hwnd, &outerNow);
    POINT cur = {curAbs.x - outerNow.left, curAbs.y - outerNow.top};

    int dx = wantX ? (cur.x - lastMouse.x) : 0;
    int dy = wantY ? (cur.y - lastMouse.y) : 0;

    CollisionOptions opts;
    opts.excludeIndex = index;
    opts.excludeChildren = 1;
    opts.checkNormalWindows = HasCapability(capabilities, WC_NOENTRY);

    /* UpdateBlockFlags相当: 現在位置から1px先読みした矩形が障害物と重なって
       いるかを毎フレーム静的に判定する（スイープ判定ではない）。既に押し
       当たっている方向へさらに進もうとしている場合はその軸の移動量を丸ごと
       0にする -- ValidatePositionのスイープクランプに任せず、壁に密着した
       まま完全に静止させる。反対方向へカーソルが戻ればブロックは即座に
       解除される。 */
    RECT r1 = current, l1 = current, d1 = current, u1 = current;
    OffsetRect(&r1, 1, 0);
    OffsetRect(&l1, -1, 0);
    OffsetRect(&d1, 0, 1);
    OffsetRect(&u1, 0, -1);
    blockedR = Collision_CheckOverlap(r1, opts);
    blockedL = Collision_CheckOverlap(l1, opts);
    blockedD = Collision_CheckOverlap(d1, opts);
    blockedU = Collision_CheckOverlap(u1, opts);

    int moveX = ((dx > 0 && blockedR) || (dx < 0 && blockedL)) ? 0 : dx;
    int moveY = ((dy > 0 && blockedD) || (dy < 0 && blockedU)) ? 0 : dy;

    RECT proposed = current;
    OffsetRect(&proposed, moveX, moveY);

    RECT validated = Collision_ValidatePosition(current, proposed, opts);

    /* `current`/`validated`はCollisionBoundsと等価（4辺のうち3辺はクライアント
       ベース -- GetWindowFullBounds参照）であり、これは上の衝突比較に対しては
       まさに正しいが、SetWindowPosに渡すには誤り: そのAPIはウィンドウの
       外枠を基準に位置決めする。移動量（デルタ）は枠に依存しない
       （ドラッグしても枠の幅は変わらない）ため、どちらの座標系でも有効
       -- 絶対的な目標位置だけは、使用前にウィンドウの実際の外枠位置に
       変換し直す必要がある。 */
    int actualDx = validated.left - current.left;
    int actualDy = validated.top - current.top;

    if (actualDx != 0 || actualDy != 0)
    {
        RECT outer;
        GetWindowRect(hwnd, &outer);
        SetWindowPos(hwnd, NULL, outer.left + actualDx, outer.top + actualDy, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        Hierarchy_PropagateMove(index, actualDx, actualDy);
        Player_FollowParentMove(Player_GetActive(), index, actualDx, actualDy);

        /* Goal/ボタンは誰のchildIdx[]配列にも入っていないため、
           Hierarchy_PropagateMoveでは到達できない -- 代わりに自身の祖先
           チェーンを辿る。Player_FollowParentMoveと同じ考え方で、単純な
           平行移動は、移動したウィンドウの下にどれだけ深くネストされて
           いてもそのまま適用される。 */
        int goalIdx = FindGoalIndex();
        if (goalIdx >= 0)
            FollowMovedWindowIfDescendant(&g_windows[goalIdx], index, actualDx, actualDy);

        for (int b = 0; b < g_windowCount; b++)
            if (IsButtonWindowKind(g_windows[b].kind))
                FollowMovedWindowIfDescendant(&g_windows[b], index, actualDx, actualDy);

        InvalidateRect(hwnd, NULL, FALSE);
    }

    /* lastMouseはここで更新しない -- StartDragging相当のOnMouseDownでのみ
       記録され、ドラッグ終了までドラッグ開始時の値を保持し続ける。 */
    Hierarchy_CheckAndUpdate(index);
}

/* 以前のUpdateResizable(通常のリサイズ)とUpdateUnconstrained(反転可能な
   制限なしリサイズ)を1本に統合したもの。WC_RESIZE_FLIP_X/Yを軸ごとの
   修飾フラグとして扱い、Collision_ValidateSizeFromAnchorへ渡すflipX/flipYを
   その軸が反転を許可していない場合は常にfalseにする。flipX=flipY=false・
   anchor=ドラッグ開始時の左上のとき、Collision_ValidateSizeFromAnchorは
   Collision_ValidateSize(旧UpdateResizableが使っていた関数)と数式的に
   完全に同じ結果を返すため、これは「純粋なResizable」の挙動を変えない
   （両関数の実装を突き合わせて確認済み）。両軸とも反転を許可しない場合は
   子の下限を旧UpdateResizableと同じCHILD_UNBOUNDED_MIN_SIZEに、どちらかの
   軸でも反転を許可する場合は旧UpdateUnconstrainedと同じUNCONSTRAINED_MIN_ABS_SIZEに
   する（軸ごとに異なる下限を混在させるとCollision_ValidateSizeFromAnchorの
   単一のminSize引数と噛み合わないため、「どちらかの軸でも反転可なら
   反転可グループの制限値を両軸に適用する」という単純化を採用した -- 既存の
   純粋なResizable/純粋なUnconstrainedの2パターンは完全に元の挙動のまま、
   新しく増えた「移動+リサイズ」等の組み合わせのみがこの単純化の対象になる）。 */
void GameWindowData::UpdateResize(int index)
{
    int wantX = HasCapability(capabilities, WC_RESIZE_X);
    int wantY = HasCapability(capabilities, WC_RESIZE_Y);
    int flipX = HasCapability(capabilities, WC_RESIZE_FLIP_X);
    int flipY = HasCapability(capabilities, WC_RESIZE_FLIP_Y);

    POINT cur;
    GetCursorPos(&cur);

    int dx = wantX ? (cur.x - resizeDragStart.x) : 0;
    int dy = wantY ? (cur.y - resizeDragStart.y) : 0;

    int newLogicalW = resizeOrigSize.cx + dx;
    int newLogicalH = resizeOrigSize.cy + dy;

    /* 反転を許可する軸は0付近の不感帯: 絶対値がUNCONSTRAINED_MIN_ABS_SIZE
       未満にならないよう符号を保ったままクランプする（ユーザーがドラッグし
       続ければ自然に符号が反転する）。反転を許可しない軸は0を跨がせず、
       通常のMIN_WINDOW_SIZEを下限にする（従来のUpdateResizableと同じ）。 */
    if (flipX)
    {
        if (newLogicalW >= 0 && newLogicalW < UNCONSTRAINED_MIN_ABS_SIZE)
            newLogicalW = UNCONSTRAINED_MIN_ABS_SIZE;
        else if (newLogicalW < 0 && newLogicalW > -UNCONSTRAINED_MIN_ABS_SIZE)
            newLogicalW = -UNCONSTRAINED_MIN_ABS_SIZE;
    }
    else if (newLogicalW < MIN_WINDOW_SIZE)
    {
        newLogicalW = MIN_WINDOW_SIZE;
    }
    if (flipY)
    {
        if (newLogicalH >= 0 && newLogicalH < UNCONSTRAINED_MIN_ABS_SIZE)
            newLogicalH = UNCONSTRAINED_MIN_ABS_SIZE;
        else if (newLogicalH < 0 && newLogicalH > -UNCONSTRAINED_MIN_ABS_SIZE)
            newLogicalH = -UNCONSTRAINED_MIN_ABS_SIZE;
    }
    else if (newLogicalH < MIN_WINDOW_SIZE)
    {
        newLogicalH = MIN_WINDOW_SIZE;
    }

    /* isFlippedX/Yは「今回のフレームで実際に反転しているか」であり、
       flipX/Y(その軸が反転を許可されているか)とは別物 -- 反転を許可しない
       軸は上のクランプで0を跨がないため、isFlippedX/Yは常にfalseになる。 */
    int isFlippedX = newLogicalW < 0;
    int isFlippedY = newLogicalH < 0;
    int absW = abs(newLogicalW);
    int absH = abs(newLogicalH);

    int prevAbsW = abs(logicalW);
    int prevAbsH = abs(logicalH);
    /* 反転イベント(前フレームまでの符号と今回の符号が食い違う)の検出用。
       子孫のinheritedFlipX/Yは「今その内部にいるか」のライブ判定ではなく、
       実際に反転が起きた瞬間だけXORで積算する永続フラグのため、コミット前の
       符号をここで保持しておく必要がある。 */
    int wasFlippedX = logicalW < 0;
    int wasFlippedY = logicalH < 0;

    CollisionOptions opts;
    opts.excludeIndex = index;
    opts.excludeChildren = 1;
    /* 反転を許可する軸を持つウィンドウは、通常ウィンドウ用のNoEntry判定
       (WC_NOENTRYの場合のみ)とは無関係に、不可侵ゾーン/不可侵ウィンドウ
       境界には常にぶつかる(元のUpdateUnconstrainedと同じ) -- GatherObstacles
       はNoEntry系をcheckNormalWindowsの値に関わらず常に収集するため、
       この設定のままでよい。 */
    opts.checkNormalWindows = HasCapability(capabilities, WC_NOENTRY);

    SIZE currentAbs = {prevAbsW, prevAbsH};
    SIZE proposed = {absW, absH};
    int minAbsSize = (flipX || flipY) ? UNCONSTRAINED_MIN_ABS_SIZE : MIN_WINDOW_SIZE;
    SIZE validated = Collision_ValidateSizeFromAnchor(unconstrainedAnchor, isFlippedX, isFlippedY,
                                                       currentAbs, proposed, opts,
                                                       minAbsSize, MAX_WINDOW_SIZE);

    int visualLeft = isFlippedX ? (unconstrainedAnchor.x - validated.cx) : unconstrainedAnchor.x;
    int visualTop = isFlippedY ? (unconstrainedAnchor.y - validated.cy) : unconstrainedAnchor.y;

    RECT outer;
    GetWindowRect(hwnd, &outer);
    if (outer.left != visualLeft || outer.top != visualTop ||
        (outer.right - outer.left) != validated.cx || (outer.bottom - outer.top) != validated.cy)
    {
        /* 反転中は移動+リサイズが同時に起きる（visualLeft/Topが毎フレーム
           動く）。SWP_NOREDRAWを付けずに位置とサイズを同時に変えると、OS側が
           SetWindowPosの中で（このすぐ下の明示的なInvalidateRect+UpdateWindow
           より前に）古い内容を新しい位置/サイズへ引き伸ばして即座に描画
           してしまうことがあり、これが1フレームごとに新しい正しい描画で
           上書きされる形になって、見た目上がくがくして見える（実際に報告
           された不具合: 反転中に伸ばすとタイトルバーが滑らかに動かない）。
           反転を許可しない軸だけの単純なリサイズ(SWP_NOMOVE相当)ではこの
           同時発生が起きないが、統合後は同じ経路を通るため常にSWP_NOREDRAW
           にしておく。 */
        SetWindowPos(hwnd, NULL, visualLeft, visualTop, validated.cx, validated.cy,
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);

        /* 子の追従はHierarchy_ApplyScaleではなくHierarchy_ApplyRelativeTransform
           を使う: アンカー基準の反転で親の可視矩形の左上そのものが動く/
           反転しうるため、子の絶対位置を固定したままサイズだけ変えると
           相対配置が崩れる（実際に発生した不具合）。子は親に対する相対位置・
           相対サイズを保ったまま追従させる。スケール基準はドラッグ開始時点の
           サイズ・位置＝ジェスチャー全体を通して固定のrootOrigRect。 */
        int origAbsW = abs(resizeOrigSize.cx);
        int origAbsH = abs(resizeOrigSize.cy);
        int origFlipX = resizeOrigSize.cx < 0;
        int origFlipY = resizeOrigSize.cy < 0;
        int origVisualLeft = origFlipX ? (unconstrainedAnchor.x - origAbsW) : unconstrainedAnchor.x;
        int origVisualTop = origFlipY ? (unconstrainedAnchor.y - origAbsH) : unconstrainedAnchor.y;
        RECT rootOrigRect = {origVisualLeft, origVisualTop, origVisualLeft + origAbsW, origVisualTop + origAbsH};
        RECT rootNewRect = {visualLeft, visualTop, visualLeft + validated.cx, visualTop + validated.cy};
        /* 反転を全く許可しない場合は子の下限も従来のCHILD_UNBOUNDED_MIN_SIZEを
           使う(通常のResizableと同じ、親自身のクランプで間接的にサイズ制限を
           受ける)。 */
        int childMinSize = (flipX || flipY) ? UNCONSTRAINED_MIN_ABS_SIZE : CHILD_UNBOUNDED_MIN_SIZE;
        Hierarchy_ApplyRelativeTransform(index, rootOrigRect, rootNewRect, childMinSize, MAX_WINDOW_SIZE);

        /* 反転イベントが起きた軸だけ、その時点の全子孫のinheritedFlipX/Yを
           永続的にXORで反転させる。親から切り離された後もこの見た目は
           元に戻らず、再度いずれかの祖先が反転した時だけ変化する。
           同時に、直接の子（ウィンドウ・プレイヤー）の位置も該当軸について
           鏡映する -- 直前のHierarchy_ApplyRelativeTransformは正のスケール比
           だけで追従させるため反転を正しく表現できず、そのままだと反転で
           見た目上反対側に移動したタイトルバー等にプレイヤーがめり込んで
           しまう（実際に報告された不具合）。 */
        if (isFlippedX != wasFlippedX || isFlippedY != wasFlippedY)
        {
            int mirrorX = isFlippedX != wasFlippedX;
            int mirrorY = isFlippedY != wasFlippedY;
            Hierarchy_ToggleInheritedFlip(index, mirrorX, mirrorY);
            Hierarchy_MirrorDirectChildren(index, rootNewRect, mirrorX, mirrorY);
        }

        logicalW = isFlippedX ? -validated.cx : validated.cx;
        logicalH = isFlippedY ? -validated.cy : validated.cy;

        InvalidateRect(hwnd, NULL, FALSE);
        /* 拡大直後、実際にWM_PAINTで塗りつぶされるまで新しい領域が黒く
           フラッシュするのを防ぐため同期的に再描画する（ゴール/子ウィンドウ/
           プレイヤーと同じ対策）。 */
        UpdateWindow(hwnd);
    }
}

void Strategy_UpdateAll(float dt)
{
    (void)dt;
    for (int i = 0; i < g_windowCount; i++)
    {
        GameWindowData *data = &g_windows[i];
        if (data->dragging)
            data->UpdateDrag(i);
        else if (data->resizing)
            data->UpdateResize(i);
    }
}
