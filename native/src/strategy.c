#include "strategy.h"
#include "collision.h"
#include "hierarchy.h"
#include "player.h"

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
        data->dragging = 1;
        SetCapture(data->hwnd);
        GetCursorPos(&data->lastMouse);
        data->blockedL = data->blockedR = data->blockedU = data->blockedD = 0;
        break;
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
    default:
        break;
    }
}

static void UpdateMovable(int index, GameWindowData *data)
{
    POINT cur;
    GetCursorPos(&cur);

    RECT current;
    GetWindowFullBounds(data->hwnd, &current);

    /* MovableWindowStrategy.CalculateMovementと厳密に一致させる: data->lastMouseは
       Strategy_HandleMouseDownでドラッグ開始時に一度だけ記録され、以後このドラッグ
       中は更新しない（末尾のdata->lastMouse = cur;を参照 -- 削除済み）。そのため
       dx/dyは「前フレームからのカーソル移動量」ではなく「ドラッグ開始からの
       カーソル累積移動量」になる。window.PointToClientがその時点のウィンドウ位置を
       使って変換する原本の実装では、この累積差分から現在位置の項が代数的に
       相殺され、結果的に「ドラッグ開始位置 + カーソル累積移動量」という絶対
       目標位置になる（本関数ではcurrent + dx/dyという形のまま計算するが、
       algebraically同じ結果になる）。これにより、ある1フレームで衝突により
       移動が一部しか適用できなくても、次のフレームは常にこの絶対目標へ向けて
       再計算されるため、カーソルとウィンドウの相対位置がドラッグ中にズレて
       いくことがない。 */
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

void Strategy_UpdateAll(float dt)
{
    (void)dt;
    for (int i = 0; i < g_windowCount; i++)
    {
        GameWindowData *data = &g_windows[i];
        if (data->dragging)
            UpdateMovable(i, data);
        else if (data->resizing)
            UpdateResizable(i, data);
    }
}
