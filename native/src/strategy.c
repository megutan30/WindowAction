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

void Strategy_HandleMouseDown(int index)
{
    GameWindowData *data = GetWindowData(index);
    if (!data)
        return;

    switch (data->kind)
    {
    case WT_MOVABLE:
    case WT_MOVABLE_NOENTRY:
    {
        data->dragging = 1;
        SetCapture(data->hwnd);
        /* window.PointToClient(Cursor.Position)相当: 画面絶対座標そのままでは
           なく、その時点のウィンドウ左上を基準にした相対座標で記録する
           （UpdateMovable参照 -- この基準の動的な取り直しが、ドラッグ開始位置
           からの累積差分を絶対目標位置へ正しく収束させるために必須）。 */
        POINT curAbs;
        GetCursorPos(&curAbs);
        RECT outer0;
        GetWindowRect(data->hwnd, &outer0);
        data->lastMouse.x = curAbs.x - outer0.left;
        data->lastMouse.y = curAbs.y - outer0.top;
        data->blockedL = data->blockedR = data->blockedU = data->blockedD = 0;
        break;
    }
    case WT_RESIZABLE:
    case WT_RESIZABLE_NOENTRY:
    {
        data->resizing = 1;
        SetCapture(data->hwnd);
        GetCursorPos(&data->resizeDragStart);
        RECT r;
        GetWindowRect(data->hwnd, &r);
        data->resizeOrigSize.cx = r.right - r.left;
        data->resizeOrigSize.cy = r.bottom - r.top;

        /* これをインクリメントすると、以前のジェスチャー中に確立されたすべての
           origSize（ウィンドウの子、ゴール、ボタン、プレイヤーすべて）が一括で
           無効化される。以下のHierarchy_RecordOriginalSizesは、今この時点で
           既にアタッチされているものについて即座に再確立する --
           ResizableWindowStrategyのoriginalSizes辞書がStartResizing()の
           たびに空から始まり、RecordOriginalSizesRecursiveによって埋められる
           挙動を踏襲している。このジェスチャーの途中で子になったもの
           （プレイヤーが歩いて入ってくる、ゴール/ボタンがドラッグで
           乗せられる）は、代わりにHierarchy_ApplyScaleが初めて触れられた
           際に currentSize/scale を遅延的に逆算するフォールバックに任される。
           これはオリジナルの "if (!originalSizes.ContainsKey(child))"
           フォールバックと同じで、ドラッグ開始後にウィンドウが既にどれだけ
           拡大縮小していたかに関わらず、いきなりcurrentSize倍にジャンプ
           することを防ぐ。 */
        g_resizeGeneration++;
        Hierarchy_RecordOriginalSizes(index);
        break;
    }
    case WT_MINIMIZABLE:
    case WT_MINIMIZABLE_NOENTRY:
        SetWindowMinimized(index, !data->minimized);
        break;
    case WT_UNCONSTRAINED:
    case WT_UNCONSTRAINED_NOENTRY:
    {
        data->resizing = 1;
        SetCapture(data->hwnd);
        GetCursorPos(&data->resizeDragStart);

        /* このジェスチャーの基準となる符号付き論理サイズをresizeOrigSizeに
           退避する（cx/cyはLONGなので負値もそのまま格納できる）。 */
        data->resizeOrigSize.cx = data->logicalW;
        data->resizeOrigSize.cy = data->logicalH;

        /* アンカー点（このジェスチャー中ずっと固定される角）を、現在の反転
           状態から逆算する: 反転していない軸は実ウィンドウの左上そのもの、
           反転している軸は実ウィンドウの右(下)端がアンカーになる
           （UpdateUnconstrainedのVisualTopLeft計算と対になる）。 */
        RECT r;
        GetWindowRect(data->hwnd, &r);
        data->unconstrainedAnchor.x = (data->logicalW < 0) ? r.right : r.left;
        data->unconstrainedAnchor.y = (data->logicalH < 0) ? r.bottom : r.top;

        g_resizeGeneration++;
        Hierarchy_RecordOriginalSizes(index);
        break;
    }
    default:
        break;
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

void Strategy_HandleButtonClick(WindowKind kind)
{
    switch (kind)
    {
    case WT_BTN_START:
        g_requestNext = 1;
        break;
    case WT_BTN_RETRY:
        g_requestRestart = 1;
        break;
    case WT_BTN_TOTITLE:
        g_requestToTitle = 1;
        break;
    case WT_BTN_EXIT:
        g_requestExit = 1;
        break;
#ifdef ENABLE_STAGE_EDITOR
    case WT_BTN_TEST:
        g_requestTest = 1;
        break;
    case WT_BTN_EXPORT:
        Editor_ExportStage();
        break;
    case WT_BTN_RESET:
        /* hInstanceはstrategy.cからは持っていないため、main.cのリクエスト
           フラグ経由でEditor_LoadTestStageを呼び直す（g_requestTestを
           再利用する: 現在既にテストステージ中でも同じ処理で作り直せる）。 */
        g_requestTest = 1;
        break;
#endif
    default:
        break;
    }
}

static void UpdateMovable(int index, GameWindowData *data)
{
    POINT curAbs;
    GetCursorPos(&curAbs);

    RECT current;
    GetWindowFullBounds(data->hwnd, &current);

    /* MovableWindowStrategy.CalculateMovementと一致させる: window.PointToClient(
       Cursor.Position)は画面絶対座標を「その時点のウィンドウ現在位置」基準の
       相対座標に変換する。data->lastMouseはStrategy_HandleMouseDownでドラッグ
       開始時に一度だけ記録され、以後このドラッグ中は更新しないが、curは毎フレーム
       ウィンドウの現在位置を基準に取り直す（PointToClientと同じ）ため、
       「ドラッグ開始位置 + カーソル累積移動量」という絶対目標位置に代数的に
       収束する:
         P(t) = P(t-1) + [ (cur(t)-outer(t-1)) - lastMouse ]
              = P(t-1) + (C(t)-C(0)) - (P(t-1)-P(0))
              = P(0) + (C(t)-C(0))
       （outerを画面絶対座標のまま使うと、この P(t-1) の相殺項が失われ、
       毎フレーム移動量が累積加算されて暴走する -- 実際に発生した回帰バグ）。
       これにより、ある1フレームで衝突により移動が一部しか適用できなくても、
       次のフレームは常にこの絶対目標へ向けて再計算されるため、カーソルと
       ウィンドウの相対位置がドラッグ中にズレていくことがない。 */
    RECT outerNow;
    GetWindowRect(data->hwnd, &outerNow);
    POINT cur = {curAbs.x - outerNow.left, curAbs.y - outerNow.top};

    int dx = cur.x - data->lastMouse.x;
    int dy = cur.y - data->lastMouse.y;

    CollisionOptions opts;
    opts.excludeIndex = index;
    opts.excludeChildren = 1;
    opts.checkNormalWindows = data->isNoEntry;

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
    data->blockedR = Collision_CheckOverlap(r1, opts);
    data->blockedL = Collision_CheckOverlap(l1, opts);
    data->blockedD = Collision_CheckOverlap(d1, opts);
    data->blockedU = Collision_CheckOverlap(u1, opts);

    int moveX = ((dx > 0 && data->blockedR) || (dx < 0 && data->blockedL)) ? 0 : dx;
    int moveY = ((dy > 0 && data->blockedD) || (dy < 0 && data->blockedU)) ? 0 : dy;

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
        GetWindowRect(data->hwnd, &outer);
        SetWindowPos(data->hwnd, NULL, outer.left + actualDx, outer.top + actualDy, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        Hierarchy_PropagateMove(index, actualDx, actualDy);
        Player_FollowParentMove(Player_GetActive(), index, actualDx, actualDy);

        /* Goalは誰のchildIdx[]配列にも入っていないため、Hierarchy_PropagateMove
           では到達できない -- 代わりにGoal自身の祖先チェーンを辿る。
           Player_FollowParentMoveと同じ考え方で、単純な平行移動は、
           ゴールが移動したウィンドウの下にどれだけ深くネストされていても
           そのまま適用される。 */
        int goalIdx = FindGoalIndex();
        if (goalIdx >= 0 && g_windows[goalIdx].parentIdx >= 0 && !g_windows[goalIdx].minimized &&
            Hierarchy_ChainContains(g_windows[goalIdx].parentIdx, index))
        {
            RECT gb;
            GetWindowFullBounds(g_windows[goalIdx].hwnd, &gb);
            SetWindowPos(g_windows[goalIdx].hwnd, NULL, gb.left + actualDx, gb.top + actualDy, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            InvalidateRect(g_windows[goalIdx].hwnd, NULL, FALSE);
        }

        /* 現在、移動したウィンドウの下のどこかに親を持つすべてのボタンに
           ついても同じ考え方を適用する。 */
        for (int b = 0; b < g_windowCount; b++)
        {
            GameWindowData *btn = &g_windows[b];
            if (!IsButtonWindowKind(btn->kind) || btn->parentIdx < 0 || btn->minimized)
                continue;
            if (!Hierarchy_ChainContains(btn->parentIdx, index))
                continue;
            RECT bb;
            GetWindowFullBounds(btn->hwnd, &bb);
            SetWindowPos(btn->hwnd, NULL, bb.left + actualDx, bb.top + actualDy, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            InvalidateRect(btn->hwnd, NULL, FALSE);
        }

        InvalidateRect(data->hwnd, NULL, FALSE);
    }

    /* data->lastMouseはここで更新しない -- StartDragging相当のStrategy_HandleMouseDown
       でのみ記録され、ドラッグ終了までドラッグ開始時の値を保持し続ける。 */
    Hierarchy_CheckAndUpdate(index);
}

static void UpdateResizable(int index, GameWindowData *data)
{
    POINT cur;
    GetCursorPos(&cur);

    int dx = cur.x - data->resizeDragStart.x;
    int dy = cur.y - data->resizeDragStart.y;

    RECT current;
    GetWindowFullBounds(data->hwnd, &current);

    SIZE proposed;
    proposed.cx = data->resizeOrigSize.cx + dx;
    proposed.cy = data->resizeOrigSize.cy + dy;
    if (proposed.cx < MIN_WINDOW_SIZE)
        proposed.cx = MIN_WINDOW_SIZE;
    if (proposed.cy < MIN_WINDOW_SIZE)
        proposed.cy = MIN_WINDOW_SIZE;

    CollisionOptions opts;
    opts.excludeIndex = index;
    opts.excludeChildren = 1;
    opts.checkNormalWindows = data->isNoEntry;

    SIZE validated = Collision_ValidateSize(current, proposed, opts);

    /* `current`（したがってその幅/高さ）はCollisionBoundsと等価 -- 4辺のうち
       3辺はクライアントベース -- であり、これは上の衝突比較には正しいが、
       ウィンドウの実際の外枠サイズが変化したかどうかを検出するには適さない:
       `validated`/`resizeOrigSize`は外枠ベース（ドラッグ開始時にGetWindowRect
       から構築）であるため、これらをクライアントベースのoldW/oldHと比較すると、
       実際のマウス移動の有無に関わらず毎フレーム「変化した」と判定されてしまい、
       スケール伝播が繰り返しトリガーされてしまう。 */
    RECT outer;
    GetWindowRect(data->hwnd, &outer);
    int oldW = outer.right - outer.left;
    int oldH = outer.bottom - outer.top;

    if (validated.cx != oldW || validated.cy != oldH)
    {
        SetWindowPos(data->hwnd, NULL, 0, 0, validated.cx, validated.cy,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

        /* スケールは常にドラッグ開始時に記録されたサイズを基準とし、直前の
           フレームは基準にしない -- ResizableWindowStrategyがジェスチャー
           全体を通して単一のキャッシュされた`originalSize`を使う挙動と
           一致させている。Hierarchy_ApplyScale自体がすべての再帰レベルで
           （ここではrootIndex==indexから始まる）プレイヤーとゴールをチェック
           するため、このトップレベルで別途呼び出す必要はない。 */
        float scaleX = (float)validated.cx / (float)data->resizeOrigSize.cx;
        float scaleY = (float)validated.cy / (float)data->resizeOrigSize.cy;
        Hierarchy_ApplyScale(index, scaleX, scaleY);

        InvalidateRect(data->hwnd, NULL, FALSE);
        /* 拡大直後、実際にWM_PAINTで塗りつぶされるまで新しい領域が黒く
           フラッシュするのを防ぐため同期的に再描画する（ゴール/子ウィンドウ/
           プレイヤーと同じ対策）。 */
        UpdateWindow(data->hwnd);
    }
}

/* WT_UNCONSTRAINED/WT_UNCONSTRAINED_NOENTRY専用のリサイズ更新。UpdateResizable
   と違い上限/下限をMIN_WINDOW_SIZE/MAX_WINDOW_SIZEではなく
   UNCONSTRAINED_MIN_ABS_SIZE/MAX_WINDOW_SIZEにし、論理サイズが0を跨ぐと
   反転（見た目のミラーのみ、実HWNDは常に正サイズ）する。

   衝突判定は「アンカー点を固定した正方向（右/下）への伸長」としてのみ
   評価する（logicalCurrent参照）。これは反転中の軸について、アンカーの
   反対側に既にある障害物を検出できないという既知の制約だが、反転は
   見た目のみの効果でありプレイヤーの接地判定には影響しないため許容する。 */
static void UpdateUnconstrained(int index, GameWindowData *data)
{
    POINT cur;
    GetCursorPos(&cur);

    int dx = cur.x - data->resizeDragStart.x;
    int dy = cur.y - data->resizeDragStart.y;

    int newLogicalW = data->resizeOrigSize.cx + dx;
    int newLogicalH = data->resizeOrigSize.cy + dy;

    /* 0付近の不感帯: 絶対値がUNCONSTRAINED_MIN_ABS_SIZE未満にならないように
       符号を保ったままクランプする。これにより実ウィンドウが完全に潰れず、
       ユーザーがドラッグし続ければ自然に符号が反転する。 */
    if (newLogicalW >= 0 && newLogicalW < UNCONSTRAINED_MIN_ABS_SIZE)
        newLogicalW = UNCONSTRAINED_MIN_ABS_SIZE;
    else if (newLogicalW < 0 && newLogicalW > -UNCONSTRAINED_MIN_ABS_SIZE)
        newLogicalW = -UNCONSTRAINED_MIN_ABS_SIZE;
    if (newLogicalH >= 0 && newLogicalH < UNCONSTRAINED_MIN_ABS_SIZE)
        newLogicalH = UNCONSTRAINED_MIN_ABS_SIZE;
    else if (newLogicalH < 0 && newLogicalH > -UNCONSTRAINED_MIN_ABS_SIZE)
        newLogicalH = -UNCONSTRAINED_MIN_ABS_SIZE;

    int flipX = newLogicalW < 0;
    int flipY = newLogicalH < 0;
    int absW = abs(newLogicalW);
    int absH = abs(newLogicalH);

    int prevAbsW = abs(data->logicalW);
    int prevAbsH = abs(data->logicalH);
    /* 反転イベント(前フレームまでの符号と今回の符号が食い違う)の検出用。
       子孫のinheritedFlipX/Yは「今その内部にいるか」のライブ判定ではなく、
       実際に反転が起きた瞬間だけXORで積算する永続フラグのため、コミット前の
       符号をここで保持しておく必要がある。 */
    int wasFlippedX = data->logicalW < 0;
    int wasFlippedY = data->logicalH < 0;

    CollisionOptions opts;
    opts.excludeIndex = index;
    opts.excludeChildren = 1;
    /* 制限なしリサイズは通常ウィンドウ用のNoEntry判定(isNoEntryの場合のみ)
       とは無関係に、不可侵ゾーン/不可侵ウィンドウ境界には常にぶつかる
       -- GatherObstaclesはNoEntry系をcheckNormalWindowsの値に関わらず
       常に収集するため、この設定のままでよい。 */
    opts.checkNormalWindows = data->isNoEntry;

    SIZE currentAbs = {prevAbsW, prevAbsH};
    SIZE proposed = {absW, absH};
    /* Collision_ValidateSizeExではなくこちらを使う: flip中の軸はアンカーを
       右/下端として固定し左/上方向へ伸びるため、実際に伸びている側の
       障害物（不可侵ゾーン等）を正しく検出できる（アンカーから常に正方向
       へ伸びる前提のExでは反転側の障害物を見逃す既知の制約があった）。 */
    SIZE validated = Collision_ValidateSizeFromAnchor(data->unconstrainedAnchor, flipX, flipY,
                                                       currentAbs, proposed, opts,
                                                       UNCONSTRAINED_MIN_ABS_SIZE, MAX_WINDOW_SIZE);

    int visualLeft = flipX ? (data->unconstrainedAnchor.x - validated.cx) : data->unconstrainedAnchor.x;
    int visualTop = flipY ? (data->unconstrainedAnchor.y - validated.cy) : data->unconstrainedAnchor.y;

    RECT outer;
    GetWindowRect(data->hwnd, &outer);
    if (outer.left != visualLeft || outer.top != visualTop ||
        (outer.right - outer.left) != validated.cx || (outer.bottom - outer.top) != validated.cy)
    {
        SetWindowPos(data->hwnd, NULL, visualLeft, visualTop, validated.cx, validated.cy,
                     SWP_NOZORDER | SWP_NOACTIVATE);

        /* 子の追従はHierarchy_ApplyScaleではなくHierarchy_ApplyRelativeTransform
           を使う: 通常のResizableと違い、アンカー基準の反転で親の可視矩形の
           左上そのものが動く/反転しうるため、子の絶対位置を固定したまま
           サイズだけ変えると相対配置が崩れる（実際に発生した不具合）。
           子は親に対する相対位置・相対サイズを保ったまま追従させる。
           スケール基準は（通常のResizableと同じく）ドラッグ開始時点の
           サイズ・位置＝ジェスチャー全体を通して固定のrootOrigRect。 */
        int origAbsW = abs(data->resizeOrigSize.cx);
        int origAbsH = abs(data->resizeOrigSize.cy);
        int origFlipX = data->resizeOrigSize.cx < 0;
        int origFlipY = data->resizeOrigSize.cy < 0;
        int origVisualLeft = origFlipX ? (data->unconstrainedAnchor.x - origAbsW) : data->unconstrainedAnchor.x;
        int origVisualTop = origFlipY ? (data->unconstrainedAnchor.y - origAbsH) : data->unconstrainedAnchor.y;
        RECT rootOrigRect = {origVisualLeft, origVisualTop, origVisualLeft + origAbsW, origVisualTop + origAbsH};
        RECT rootNewRect = {visualLeft, visualTop, visualLeft + validated.cx, visualTop + validated.cy};
        Hierarchy_ApplyRelativeTransform(index, rootOrigRect, rootNewRect);

        /* 反転イベントが起きた軸だけ、その時点の全子孫のinheritedFlipX/Yを
           永続的にXORで反転させる。親から切り離された後もこの見た目は
           元に戻らず、再度いずれかの祖先が反転した時だけ変化する。 */
        if (flipX != wasFlippedX || flipY != wasFlippedY)
            Hierarchy_ToggleInheritedFlip(index, flipX != wasFlippedX, flipY != wasFlippedY);

        data->logicalW = flipX ? -validated.cx : validated.cx;
        data->logicalH = flipY ? -validated.cy : validated.cy;

        InvalidateRect(data->hwnd, NULL, FALSE);
        UpdateWindow(data->hwnd);
    }
}

void Strategy_UpdateAll(float dt)
{
    (void)dt;
    for (int i = 0; i < g_windowCount; i++)
    {
        GameWindowData *data = &g_windows[i];
        if (data->dragging)
            UpdateMovable(i, data);
        else if (data->resizing)
        {
            if (data->kind == WT_UNCONSTRAINED || data->kind == WT_UNCONSTRAINED_NOENTRY)
                UpdateUnconstrained(i, data);
            else
                UpdateResizable(i, data);
        }
    }
}
