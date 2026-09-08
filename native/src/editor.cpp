#include "editor.h"

#ifdef ENABLE_STAGE_EDITOR

#include "player.h"
#include "hierarchy.h"
#include "noentry.h"
#include "zorder.h"
#include "desktopicon.h"
#include "stage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int g_requestTest = 0;

#define EDITOR_DEFAULT_SIZE 250
#define PALETTE_DRAG_ALPHA 170 /* ドラッグ中の半透明度(0-255、小さいほど透明) */

/* パレットは正方形アイコンをグリッド状に並べる（ボタンの縦一列ではなく）。 */
#define PALETTE_ICON_SIZE 70
#define PALETTE_COLS 3
#define PALETTE_GAP 6
#define PALETTE_X 10
#define PALETTE_Y 10

/* ツールバー（Export/Reset/Title）は従来通りテキストボタン。 */
#define TOOLBAR_BTN_W 150
#define TOOLBAR_BTN_H 34
#define TOOLBAR_GAP 4

/* ドロップ時のTitle/Retryボタン、およびGoalの実サイズはstage.hのBTN_W/BTN_H/
   GOAL_SIZEをそのまま使う -- 実際のステージで使われるのと同じ見た目・
   当たり判定で試せるようにする（他の窓種別のような正方形EDITOR_DEFAULT_SIZE
   にはしない）。以前はEDITOR_GOAL_SIZE/EDITOR_BTN_W/EDITOR_BTN_Hという
   独自の複製定義を持ち、stage.c側の値と手動で同期する前提になっていた。 */

/* 配置待ち（半透明の仮状態）アイテムの右下角にある、大きさ調整用の当たり
   判定領域の一辺の長さ。見た目上のハンドル表示は行わず、座標判定のみ。 */
#define PENDING_RESIZE_HANDLE 24
/* 配置待ちアイテムの角ドラッグでの最小サイズ。実配置後のMIN_WINDOW_SIZE
   （100）とは無関係 -- Goal(64x64)やRetryボタン(150x40)のような、実配置後の
   通常ウィンドウより小さい既定サイズを持つ種別も自由に縮小できるようにする
   ため、単に矩形が潰れない程度の小さな値にとどめる。 */
#define PENDING_MIN_SIZE 20

static int g_isTestStage = 0;
static HINSTANCE g_hInstance = NULL;
/* パレット+ツールバー全体を囲む矩形（Editor_LoadTestStageで一度だけ計算）。
   ドロップ判定（この外に離されたら実際に配置する）に使う。 */
static RECT g_panelBounds;
/* ドロップ済み・大きさ調整済みで、Enterキーによる確定を待っている
   g_windows[]のインデックス。無ければ-1。同時に配置待ちにできるのは
   1つだけ（Editor_StartPaletteDrag参照: 新しいドラッグを始めると自動的に
   確定される）。 */
static int g_pendingIndex = -1;

typedef struct
{
    WindowKind kind;
    WindowCapabilities caps;
    const char *label;
    /* trueなら、このパレット項目は実際にはGameWindowを生成しない特殊項目
       （静的NoEntryZone、またはプレイヤー開始位置）。kindは見た目（背景色/
       縞模様枠）を借りるためだけに使う。省略時（末尾の値を書かない行）は
       Cの集成体初期化規則により自動的に0になる。 */
    int isZone;
    /* trueなら、この項目はGameWindow/NoEntryZoneのどちらでもなく、ドロップ
       位置を新しいプレイヤー開始位置として使う特殊項目（isPlayerStart、
       Editor_CommitPending参照）。 */
    int isPlayerStart;
    /* trueなら、この項目は単体で仮置きにならない「能力パーツ」であることを
       示す（isPart、Editor_EndPaletteDrag参照）。仮置き中のウィンドウに
       重ねてドロップすると、そのcapsが仮置き先へOR結合される。 */
    int isPart;
} PaletteEntry;

/* パレットに並べる配置候補。前半は「土台」（単体でドロップすると能力を
   何も持たない仮置き状態になる）、後半は「能力パーツ」（仮置き中の
   ウィンドウに重ねてドロップすることでcapabilitiesを1つずつ追加していく）。
   以前は「Movable」「Resizable」のように土台と能力の組み合わせごとに項目を
   用意していたが、コンポーネント化の趣旨（機能をパーツとして重ねて自由に
   組み合わせる）に合わせ、土台と能力を完全に分離した。移動・リサイズ・
   反転修飾はX/Y軸ごとに独立したパーツにしてあるため、例えば「Move X」と
   「Resize Y」だけを重ねるといった軸ごとの組み合わせもそのまま試せる。 */
static const PaletteEntry kPalette[] = {
    /* 土台: 単体でドロップすると、能力を持たない仮置き状態になる。 */
    {WT_NORMAL_BLACK, WC_NONE, "Normal(Blk)"},
    {WT_NORMAL_WHITE, WC_NONE, "Normal(Wht)"},
    {WT_TEXT_DISPLAY, WC_NONE, "TextDisplay"},
    {WT_GOAL, WC_NONE, "Goal"},
    {WT_BTN_TOTITLE, WC_NONE, "Title Btn"},
    {WT_BTN_RETRY, WC_NONE, "Retry Btn"},
    {WT_NORMAL_BLACK, WC_NONE, "NoEntry Zone", 1, 0, 0},
    {WT_NORMAL_BLACK, WC_NONE, "Player", 0, 1, 0},
    /* 能力パーツ: 仮置き中のウィンドウに重ねてドロップすると、その能力が
       追加される（Zone/Playerの仮置きには効果なし）。 */
    {WT_NORMAL_BLACK, WC_MOVE_X, "Move X", 0, 0, 1},
    {WT_NORMAL_BLACK, WC_MOVE_Y, "Move Y", 0, 0, 1},
    {WT_NORMAL_BLACK, WC_RESIZE_X, "Resize X", 0, 0, 1},
    {WT_NORMAL_BLACK, WC_RESIZE_Y, "Resize Y", 0, 0, 1},
    /* WC_RESIZE_FLIP_X/Yは対応するWC_RESIZE_X/Yが立っていて初めて意味を持つ
       修飾フラグ（gamewindow.h参照）。Flipパーツ単体をResize X/Yパーツ抜きで
       重ねただけでは何も起きない（マークも出ず、リサイズも一切できない）と
       いう分かりにくい状態になってしまうため、Flipパーツ自体に対応する
       Resizeビットを含めておき、単体で常に完結した能力になるようにする
       （実際に報告された不具合）。 */
    {WT_NORMAL_BLACK, (WindowCapabilities)(WC_RESIZE_X | WC_RESIZE_FLIP_X), "Flip X", 0, 0, 1},
    {WT_NORMAL_BLACK, (WindowCapabilities)(WC_RESIZE_Y | WC_RESIZE_FLIP_Y), "Flip Y", 0, 0, 1},
    {WT_NORMAL_BLACK, WC_MINIMIZE, "Minimize", 0, 0, 1},
    {WT_NORMAL_BLACK, WC_DELETE, "Delete", 0, 0, 1},
    {WT_NORMAL_BLACK, WC_NOENTRY, "NoEntry", 0, 0, 1},
};
#define PALETTE_COUNT (sizeof(kPalette) / sizeof(kPalette[0]))

int Editor_IsTestStage(void) { return g_isTestStage; }

void Editor_LeaveTestStage(void) { g_isTestStage = 0; }

void Editor_LoadTestStage(HINSTANCE hInstance)
{
    ResetWindowRegistry();
    /* ResetWindowRegistryがg_windows[]を全て破棄する（g_windowCount=0）ため、
       直前の配置待ちインデックスは必ず無効になる。 */
    g_pendingIndex = -1;
    g_isTestStage = 1;
    g_hInstance = hInstance;

    /* 背景用の全画面ウィンドウは持たない -- 本物のデスクトップがそのまま
       見える/床になる（通常プレイでウィンドウの外にいる場合と同じ挙動、
       何も置かれていない場所ではプレイヤーは画面下端まで落ちる）。デスクトップ
       が直接見えるテストモードは、デスクトップアイコンとの当たり判定を
       試すのに最適な場所でもあるため、Stage_DesktopIconsEnabledの値に
       関わらずここで常に取得し直す（player.cのCheckGroundedNormalは
       Editor_IsTestStage()中もアイコンを床として扱う）。 */
    DesktopIcon_Refresh(hInstance);

    /* 配置パレット（画面左上にグリッド状、ドラッグ&ドロップ用の小さな
       正方形アイコン）。各アイコンは実際に配置される種別の背景色/マークを
       そのまま表示し、灰色ボタン+文字だけには頼らない（DrawKindMark/
       GetKindAppearance参照）。ドラッグ開始時にホームポジション
       (paletteHomePos)を離れ、ドロップ完了後は常にこの位置・サイズへ戻る。 */
    for (size_t i = 0; i < PALETTE_COUNT; i++)
    {
        int col = (int)(i % PALETTE_COLS);
        int row = (int)(i / PALETTE_COLS);
        int x = PALETTE_X + col * (PALETTE_ICON_SIZE + PALETTE_GAP);
        int y = PALETTE_Y + row * (PALETTE_ICON_SIZE + PALETTE_GAP);

        int idx = CreateGameWindowIndexed(hInstance, WT_BTN_PALETTE, WC_NONE, x, y,
                                           PALETTE_ICON_SIZE, PALETTE_ICON_SIZE, kPalette[i].label);
        if (idx >= 0)
        {
            g_windows[idx].paletteKind = kPalette[i].kind;
            g_windows[idx].paletteCaps = kPalette[i].caps;
            g_windows[idx].paletteIsZone = kPalette[i].isZone;
            g_windows[idx].paletteIsPlayerStart = kPalette[i].isPlayerStart;
            g_windows[idx].paletteIsPart = kPalette[i].isPart;
            g_windows[idx].paletteHomePos.x = x;
            g_windows[idx].paletteHomePos.y = y;
            g_windows[idx].paletteIconSize.cx = PALETTE_ICON_SIZE;
            g_windows[idx].paletteIconSize.cy = PALETTE_ICON_SIZE;
            g_windows[idx].isEditorChrome = 1;
        }
    }

    int gridRows = ((int)PALETTE_COUNT + PALETTE_COLS - 1) / PALETTE_COLS;
    int gridRight = PALETTE_X + PALETTE_COLS * (PALETTE_ICON_SIZE + PALETTE_GAP);
    int gridBottom = PALETTE_Y + gridRows * (PALETTE_ICON_SIZE + PALETTE_GAP);

    /* ツールバー（グリッドの下に続けて配置）。WT_BTN_TOTITLEはパレットからも
       配置できる実ゲームプレイ種別と同じkindを使い回すため、isEditorChrome
       フラグで「これはエディター自身のナビゲーションボタンであり、
       ユーザーが配置したものではない」ことを明示しておく（そうしないと
       Export/Deleteの対象外判定がパレット由来のTitleボタンと区別できない）。 */
    int py = gridBottom + TOOLBAR_GAP;
    int exportIdx = CreateGameWindowIndexed(hInstance, WT_BTN_EXPORT, WC_NONE, PALETTE_X, py, TOOLBAR_BTN_W, TOOLBAR_BTN_H, "Export");
    py += TOOLBAR_BTN_H + TOOLBAR_GAP;
    int resetIdx = CreateGameWindowIndexed(hInstance, WT_BTN_RESET, WC_NONE, PALETTE_X, py, TOOLBAR_BTN_W, TOOLBAR_BTN_H, "Reset");
    py += TOOLBAR_BTN_H + TOOLBAR_GAP;
    int titleIdx = CreateGameWindowIndexed(hInstance, WT_BTN_TOTITLE, WC_NONE, PALETTE_X, py, TOOLBAR_BTN_W, TOOLBAR_BTN_H, "Title");
    py += TOOLBAR_BTN_H;
    if (exportIdx >= 0) g_windows[exportIdx].isEditorChrome = 1;
    if (resetIdx >= 0) g_windows[resetIdx].isEditorChrome = 1;
    if (titleIdx >= 0) g_windows[titleIdx].isEditorChrome = 1;

    g_panelBounds.left = 0;
    g_panelBounds.top = 0;
    g_panelBounds.right = (gridRight > PALETTE_X + TOOLBAR_BTN_W) ? gridRight : (PALETTE_X + TOOLBAR_BTN_W);
    g_panelBounds.bottom = py;

    /* プレイヤーはパレット列に被らない画面中央付近から開始する。 */
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    Player *p = Player_GetActive();
    if (p)
    {
        Player_Reset(p, vx + vw / 2, vy + vh / 2);
        Player_AssignInitialParent(p);
    }
}

/* ドラッグ中のアイコンの実サイズ。配置先に実際どの大きさで置かれるかが
   一目で分かるよう、待機時の小さなアイコンではなく実配置サイズまで拡大する。
   Title/RetryボタンとGoalはstage.hのBTN_W/BTN_H/GOAL_SIZE（stage.cの
   MakeButton/MakeGoalと同じ値）、プレイヤー開始位置は実際のプレイヤーと
   同じPLAYER_SIZE、それ以外（NoEntry Zoneを含む）は正方形のEDITOR_DEFAULT_SIZE。 */
static SIZE DragFullSize(WindowKind kind, int isPlayerStart)
{
    if (isPlayerStart)
        return SIZE{PLAYER_SIZE, PLAYER_SIZE};
    if (kind == WT_GOAL)
        return SIZE{GOAL_SIZE, GOAL_SIZE};
    if (kind == WT_BTN_TOTITLE || kind == WT_BTN_RETRY)
        return SIZE{BTN_W, BTN_H};
    return SIZE{EDITOR_DEFAULT_SIZE, EDITOR_DEFAULT_SIZE};
}

/* ドロップ時に生成するウィンドウの初期表示テキスト。TextDisplay等ほとんどの
   種別はNULL（kind別の既定文字列に任せる）のままだが、Title/Retryボタンは
   実際のステージ(stage.cのMakeButton呼び出し)と同じキャプションが最初から
   付いていないと、ボタンとして何のためのものか一見して分からない。 */
static const char *DefaultDropText(WindowKind kind)
{
    switch (kind)
    {
    case WT_BTN_TOTITLE: return "Title";
    case WT_BTN_RETRY: return "Retry";
    default: return NULL;
    }
}

/* 配置待ち(g_pendingIndex)のアイテムを、その時点の位置・大きさで確定する。
   半透明のパレットアイコン(WT_BTN_PALETTE)自身を、実際の種別のGameWindow
   （またはNoEntry Zone）に「実体化」させる -- 既存のCreateGameWindowIndexed/
   NoEntry_AddZoneをそのまま使うため、パレットアイコンは一旦DeleteWindowで
   破棄し、確定後の実サイズで新規に生成し直す（在席中のkindを直接書き換える
   方式だと、生成時にkind別に固定されるexStyle/WS_EX_TOOLWINDOW等を実行時に
   安全に付け替える手段がなく、タスクバー表示等が不安定になるため避けた）。
   配置待ちが無ければ何もしない。 */
static void Editor_CommitPending(void)
{
    if (g_pendingIndex < 0)
        return;
    GameWindowData *d = GetWindowData(g_pendingIndex);
    int committedIndex = g_pendingIndex;
    g_pendingIndex = -1;
    if (!d || !d->hwnd)
        return;

    ReleaseCapture();
    d->pendingGesture = 0;

    RECT r;
    GetWindowRect(d->hwnd, &r);
    int x = r.left, y = r.top;
    int w = r.right - r.left, h = r.bottom - r.top;
    int isZone = d->paletteIsZone;
    int isPlayerStart = d->paletteIsPlayerStart;
    WindowKind kind = d->paletteKind;
    WindowCapabilities caps = d->paletteCaps;

    DeleteWindow(committedIndex);

    if (isPlayerStart)
    {
        /* プレイヤーはGameWindowでもNoEntryZoneでもなく、g_playerという
           シングルトンとして既に存在している -- ここではその位置をドロップ
           位置へ再確立するだけでよい。角ドラッグを無効化しているためw/hは
           常にPLAYER_SIZEのまま（Editor_StartPaletteDrag参照）。 */
        Player *p = Player_GetActive();
        if (p)
        {
            Player_Reset(p, x, y);
            Player_AssignInitialParent(p);
        }
    }
    else if (isZone)
    {
        /* 静的NoEntryZoneはGameWindowではないため、CreateGameWindowIndexed
           ではなくNoEntry_AddZoneで直接追加する（クリックスルーの縞模様
           マーカーが生成される。実ゲームプレイでの静的不可侵領域と全く
           同じ仕組み）。 */
        NoEntry_AddZone(g_hInstance, x, y, w, h);
    }
    else
    {
        int newIdx = CreateGameWindowIndexed(g_hInstance, kind, caps, x, y, w, h, DefaultDropText(kind));
        /* WindowMessageHandler.HandleLeftButtonUpと同じく、配置直後に一度だけ
           親子判定を行う -- そうしないとドラッグ&ドロップで置いたウィンドウは
           他のウィンドウの中に完全に収まっていても親子付けされず、その場で
           少し動かす（ナッジドラッグ）まで親子関係が確立しなかった
           （実際に報告された不具合）。 */
        if (newIdx >= 0 && IsQueryableWindow(kind))
            Hierarchy_CheckAndUpdate(newIdx);
    }
}

/* パレットから1個取り出す（ドラッグを開始する）瞬間に、同じ種別のアイコンを
   ホームポジションへ複製しておく。そうしないと1回配置するたびにそのマスが
   空になり、同じ種別を続けて何度も置きたい場合にいちいちパレット全体を
   作り直す（Resetする）羽目になる -- 実際に配置作業がやりにくいと報告された。
   ドラッグ/配置待ち/確定の一連の処理は元のindexをそのまま辿り続けるので、
   ここで作る複製は一切それらに関与させず、単にホームポジションに留まる
   新しいWT_BTN_PALETTEインスタンスとして存在するだけでよい。 */
static void RespawnPaletteIcon(const GameWindowData *src)
{
    int idx = CreateGameWindowIndexed(g_hInstance, WT_BTN_PALETTE, WC_NONE,
                                       src->paletteHomePos.x, src->paletteHomePos.y,
                                       src->paletteIconSize.cx, src->paletteIconSize.cy,
                                       src->text);
    if (idx < 0)
        return;
    g_windows[idx].paletteKind = src->paletteKind;
    g_windows[idx].paletteCaps = src->paletteCaps;
    g_windows[idx].paletteIsZone = src->paletteIsZone;
    g_windows[idx].paletteIsPlayerStart = src->paletteIsPlayerStart;
    g_windows[idx].paletteIsPart = src->paletteIsPart;
    g_windows[idx].paletteHomePos = src->paletteHomePos;
    g_windows[idx].paletteIconSize = src->paletteIconSize;
    g_windows[idx].isEditorChrome = 1;
}

void Editor_StartPaletteDrag(int index)
{
    GameWindowData *d = GetWindowData(index);
    if (!d)
        return;

    if (index == g_pendingIndex)
    {
        /* 既に位置が決まっている配置待ち(半透明)アイテム -- 右下角の
           ハンドル領域内でのクリックは大きさ調整、それ以外の場所への
           クリックは位置移動のジェスチャーを開始する。Enterキーで確定する
           までは、どちらも何度でも自由にやり直せる。 */
        RECT r;
        GetWindowRect(d->hwnd, &r);
        POINT cur;
        GetCursorPos(&cur);
        /* プレイヤー開始位置はサイズ調整の概念が無い（実際のプレイヤーは
           常にPLAYER_SIZE固定）ため、角のリサイズ判定自体を無効化し、
           常に位置移動ジェスチャーのみにする。 */
        int inCorner = !d->paletteIsPlayerStart &&
                        cur.x >= r.right - PENDING_RESIZE_HANDLE && cur.x <= r.right &&
                        cur.y >= r.bottom - PENDING_RESIZE_HANDLE && cur.y <= r.bottom;

        d->pendingGesture = inCorner ? 2 : 1;
        d->pendingGestureStart = cur;
        d->pendingGestureOrigRect = r;
        SetCapture(d->hwnd);
        return;
    }

    /* 既に別のアイテムが配置待ちの状態で新しい「土台」のドラッグを始めた
       場合、前のものを宙ぶらりんにせず自動的にその時点の大きさで確定する。
       「能力パーツ」のドラッグ開始ではこれを行わない -- パーツは仮置き中の
       ウィンドウへ重ねて追加するためのものなので、ここで自動確定してしまうと
       重ねる相手が消えてしまい本来の使い方ができなくなる。 */
    if (g_pendingIndex >= 0 && !d->paletteIsPart)
        Editor_CommitPending();

    RespawnPaletteIcon(d);

    d->paletteDragging = 1;
    SetCapture(d->hwnd);

    /* ドラッグ中は「配置されるものそのもの」を実サイズで半透明表示する
       （小さなボタンのままカーソルに追従するのではなく）。この時点での
       サイズはあくまで既定値 -- ドロップ後、角ドラッグで自由に調整できる。
       能力パーツは実体化する「もの」を持たないため、拡大せずパレット
       アイコンそのままの小さいサイズでカーソルに追従させる。 */
    SIZE size = d->paletteIsPart ? d->paletteIconSize : DragFullSize(d->paletteKind, d->paletteIsPlayerStart);
    POINT cur;
    GetCursorPos(&cur);
    SetWindowPos(d->hwnd, NULL, cur.x - size.cx / 2, cur.y - size.cy / 2, size.cx, size.cy,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    SetLayeredWindowAttributes(d->hwnd, 0, PALETTE_DRAG_ALPHA, LWA_ALPHA);
}

/* 配置待ちアイテムの位置移動/大きさ調整ジェスチャーを1フレーム分進める。
   Strategy.cのUpdateMovable/UpdateResizableは流用しない -- あちらはMIN_
   WINDOW_SIZE(100)やNoEntry衝突回避、階層更新など実ゲームプレイの戦略に
   合わせた制約を持ち、Goal(64x64)やRetryボタン(150x40)のような小さい既定
   サイズの種別には厳しすぎる。ここでは単純にジェスチャー開始時点からの
   カーソル移動量をそのまま位置/サイズへ適用するだけの軽量な実装にとどめる。 */
static void UpdatePendingGesture(GameWindowData *d)
{
    if (d->pendingGesture == 0)
        return;

    POINT cur;
    GetCursorPos(&cur);
    int dx = cur.x - d->pendingGestureStart.x;
    int dy = cur.y - d->pendingGestureStart.y;

    if (d->pendingGesture == 2) /* 右下角ドラッグ: 左上を固定して大きさを追従 */
    {
        int newW = (d->pendingGestureOrigRect.right - d->pendingGestureOrigRect.left) + dx;
        int newH = (d->pendingGestureOrigRect.bottom - d->pendingGestureOrigRect.top) + dy;
        if (newW < PENDING_MIN_SIZE)
            newW = PENDING_MIN_SIZE;
        if (newH < PENDING_MIN_SIZE)
            newH = PENDING_MIN_SIZE;
        SetWindowPos(d->hwnd, NULL, 0, 0, newW, newH, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    else /* 本体ドラッグ: 位置移動 */
    {
        int newX = d->pendingGestureOrigRect.left + dx;
        int newY = d->pendingGestureOrigRect.top + dy;
        SetWindowPos(d->hwnd, NULL, newX, newY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    InvalidateRect(d->hwnd, NULL, FALSE);
}

void Editor_UpdatePaletteDrags(void)
{
    for (int i = 0; i < g_windowCount; i++)
    {
        GameWindowData *d = &g_windows[i];
        if (d->kind != WT_BTN_PALETTE || !d->paletteDragging || !d->hwnd)
            continue;
        SIZE size = d->paletteIsPart ? d->paletteIconSize : DragFullSize(d->paletteKind, d->paletteIsPlayerStart);
        POINT cur;
        GetCursorPos(&cur);
        SetWindowPos(d->hwnd, NULL, cur.x - size.cx / 2, cur.y - size.cy / 2, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    if (g_pendingIndex >= 0)
    {
        GameWindowData *d = GetWindowData(g_pendingIndex);
        if (d)
            UpdatePendingGesture(d);
    }
}

void Editor_EndPaletteDrag(int index)
{
    GameWindowData *d = GetWindowData(index);
    if (!d)
        return;

    if (d->pendingGesture != 0)
    {
        /* 配置待ちアイテムの位置移動/大きさ調整ジェスチャー終了。キャプチャ
           解放+フラグクリアのみ行い、まだ確定（Enterキー）はしない --
           半透明のまま、新しい位置/大きさで残る。 */
        ReleaseCapture();
        d->pendingGesture = 0;
        return;
    }

    if (!d->paletteDragging)
        return;
    d->paletteDragging = 0;
    ReleaseCapture();

    if (d->paletteIsPart)
    {
        /* 能力パーツ: 単体では何も配置しない消費型の操作。その時点でカーソル
           直下にあるウィンドウへ自身の能力ビットをOR結合する -- 対象は
           仮置き中のアイテム（paletteCapsへ、Enterで確定するまで反映を
           見た目だけ先取りする）と、既に確定済みの実ウィンドウ
           （capabilitiesへ直接、その場で能力が有効になる）の両方を許す
           （実際に要望された挙動: 仮置き中でなくても後から能力を追加
           できるようにする）。WindowFromPointはZ-order的に最前面の
           ウィンドウを返すため、重なっている場合は見えている方が対象になる。
           カーソル直下に何も無い、パレット/ツールバー自身、Zone/PlayerStart
           （どちらもGameWindowではないためそもそもヒットしない）の場合は
           何も起きない -- いずれの場合もパーツ自身は常にホームポジションへ
           戻る（土台と違い、パレット外に置いても仮置き状態にはならない）。 */
        POINT cur;
        GetCursorPos(&cur);
        HWND hit = WindowFromPoint(cur);
        int hitIndex = FindWindowIndex(hit);
        if (hitIndex >= 0)
        {
            GameWindowData *target = &g_windows[hitIndex];
            if (hitIndex == g_pendingIndex)
            {
                if (!target->paletteIsZone && !target->paletteIsPlayerStart)
                {
                    target->paletteCaps |= d->paletteCaps;
                    InvalidateRect(target->hwnd, NULL, FALSE);
                }
            }
            else if (!target->isEditorChrome && target->kind != WT_BTN_PALETTE)
            {
                target->capabilities |= d->paletteCaps;
                InvalidateRect(target->hwnd, NULL, FALSE);
            }
        }
        SetLayeredWindowAttributes(d->hwnd, 0, 255, LWA_ALPHA);
        SetWindowPos(d->hwnd, NULL, d->paletteHomePos.x, d->paletteHomePos.y,
                     d->paletteIconSize.cx, d->paletteIconSize.cy, SWP_NOZORDER | SWP_NOACTIVATE);
        return;
    }

    /* パレット+ツールバーの矩形の外に離されていれば、位置はそこに確定する。
       中で離された場合（ドラッグせず単に離した等）は配置キャンセル。 */
    POINT cur;
    GetCursorPos(&cur);
    int outside = cur.x < g_panelBounds.left || cur.x > g_panelBounds.right ||
                  cur.y < g_panelBounds.top || cur.y > g_panelBounds.bottom;
    if (outside)
    {
        /* まだ実体化はしない -- 角のハンドルで大きさを調整してからEnterキーで
           確定する「配置待ち」の半透明状態に移行するだけ（Editor_CommitPending/
           Editor_HandlePendingCommit参照）。アイコン自身（同じhwnd）はここに
           留まり続けるため、ホームポジションへは戻さない。 */
        g_pendingIndex = index;
        return;
    }

    /* パレット内でキャンセルされた場合のみ、アイコンを元のサイズ・不透明度・
       ホームポジションへ戻す。 */
    SetLayeredWindowAttributes(d->hwnd, 0, 255, LWA_ALPHA);
    SetWindowPos(d->hwnd, NULL, d->paletteHomePos.x, d->paletteHomePos.y,
                 d->paletteIconSize.cx, d->paletteIconSize.cy, SWP_NOZORDER | SWP_NOACTIVATE);
}

/* WindowKindをそのままC識別子名の文字列にする。Editor_ExportStageの出力先
   (stage_export.txt)がstage.cへそのまま貼り付け可能なコードになるように。
   コンポーネント化により移動/リサイズ/最小化/削除/NoEntryはkindではなく
   capabilitiesビットへ移ったため、ここで扱うのは残った「見た目の土台」
   だけでよい。 */
static const char *WindowKindName(WindowKind kind)
{
    switch (kind)
    {
    case WT_NORMAL_BLACK: return "WT_NORMAL_BLACK";
    case WT_NORMAL_WHITE: return "WT_NORMAL_WHITE";
    case WT_TEXT_DISPLAY: return "WT_TEXT_DISPLAY";
    case WT_GOAL: return "WT_GOAL";
    case WT_BTN_TOTITLE: return "WT_BTN_TOTITLE";
    case WT_BTN_RETRY: return "WT_BTN_RETRY";
    default: return "WT_NORMAL_BLACK";
    }
}

/* WindowCapabilitiesをstage.cへ貼り付け可能なC式の文字列にする
   （stage.cのCreateGameWindow呼び出しで実際に使っている書式と揃える:
   単一ビットはそのまま、複数ビットは`(WindowCapabilities)(A | B | ...)`）。
   呼び出しごとに使い捨てるだけなので静的バッファ1個で足りる（同一呼び出し内で
   2回目の呼び出し結果を保持する必要がないfprintf直前の使い方のみ）。 */
static const char *CapsToExprString(WindowCapabilities caps)
{
    static char buf[256];
    struct { WindowCapabilities bit; const char *name; } bits[] = {
        {WC_MOVE_X, "WC_MOVE_X"},
        {WC_MOVE_Y, "WC_MOVE_Y"},
        {WC_RESIZE_X, "WC_RESIZE_X"},
        {WC_RESIZE_Y, "WC_RESIZE_Y"},
        {WC_RESIZE_FLIP_X, "WC_RESIZE_FLIP_X"},
        {WC_RESIZE_FLIP_Y, "WC_RESIZE_FLIP_Y"},
        {WC_MINIMIZE, "WC_MINIMIZE"},
        {WC_DELETE, "WC_DELETE"},
        {WC_NOENTRY, "WC_NOENTRY"},
    };

    if (caps == WC_NONE)
    {
        sprintf_s(buf, sizeof(buf), "WC_NONE");
        return buf;
    }

    char names[9][32];
    int count = 0;
    for (size_t i = 0; i < sizeof(bits) / sizeof(bits[0]); i++)
    {
        if (HasCapability(caps, bits[i].bit))
            sprintf_s(names[count++], sizeof(names[0]), "%s", bits[i].name);
    }

    if (count == 1)
    {
        sprintf_s(buf, sizeof(buf), "%s", names[0]);
        return buf;
    }

    sprintf_s(buf, sizeof(buf), "(WindowCapabilities)(");
    for (int i = 0; i < count; i++)
    {
        strcat_s(buf, sizeof(buf), names[i]);
        if (i + 1 < count)
            strcat_s(buf, sizeof(buf), " | ");
    }
    strcat_s(buf, sizeof(buf), ")");
    return buf;
}

/* qsort comparator: sort g_windows[] indices by current Z-order (back to front). */
static int CompareByZOrder(const void *pa, const void *pb)
{
    int ia = *(const int *)pa;
    int ib = *(const int *)pb;
    return ZOrder_GetIndex(g_windows[ia].hwnd) - ZOrder_GetIndex(g_windows[ib].hwnd);
}

/* stage_export.txtを毎回上書きすると、直前のExportで書き出した内容が次の
   Exportで消えてしまい、少し配置を変えて何度か試した結果を後から見比べたり
   出し直したりできなかった -- 末尾に連番を付け、既存のファイルを上書きしない
   最初の番号を使う（実際に要望された挙動）。カレントディレクトリに残った
   前回セッション分のファイルも数えるため、実行のたびに1から始まるとは
   限らない。 */
static void BuildNumberedExportPath(char *out, size_t outSize)
{
    for (int n = 1;; n++)
    {
        sprintf_s(out, outSize, "stage_export_%d.txt", n);
        if (GetFileAttributesA(out) == INVALID_FILE_ATTRIBUTES)
            return;
    }
}

void Editor_ExportStage(void)
{
    char path[64];
    BuildNumberedExportPath(path, sizeof(path));

    FILE *f = fopen(path, "w");
    if (!f)
        return;

    fprintf(f, "/* Editor_ExportStage の出力。stage.c の Stage_Load 内の\n"
               "   目的の case へそのまま貼り付け可能。座標/サイズは配置時点の実際の\n"
               "   ウィンドウ矩形(GetWindowRect)から取得している。 */\n");

    /* Export in current Z-order (not creation order) since the user may have
       clicked windows to front during testing; parent detection prefers the
       frontmost candidate, so this keeps the re-created hierarchy consistent. */
    int order[MAX_WINDOWS];
    int orderCount = 0;
    for (int i = 0; i < g_windowCount; i++)
    {
        GameWindowData *d = &g_windows[i];
        /* isEditorChromeはパレットアイコン/ツールバーボタン自身（ユーザーが
           配置したものではない）を除外する -- WT_BTN_TOTITLEのようにツール
           バーのナビゲーションボタンとパレットから配置可能な実ゲームプレイ
           種別とでkindを使い回しているため、kindだけでは区別できない。 */
        if (!d->hwnd || d->isEditorChrome)
            continue;
        order[orderCount++] = i;
    }
    qsort(order, orderCount, sizeof(int), CompareByZOrder);

    for (int oi = 0; oi < orderCount; oi++)
    {
        GameWindowData *d = &g_windows[order[oi]];

        RECT r;
        GetWindowRect(d->hwnd, &r);
        int w = r.right - r.left;
        int h = r.bottom - r.top;

        if (d->kind == WT_GOAL)
        {
            /* stage.cのMakeGoalヘルパーはx/yしか受け取らず、サイズは常に
               固定のGOAL_SIZE(64x64)になる -- テストモードでGoalを既定
               サイズから変更していても、その大きさはこの呼び出し形式では
               再現できない。気づかず貼り付けてしまわないよう警告を残す。 */
            if (w != 64 || h != 64)
                fprintf(f, "        /* 警告: このGoalは%dx%dにリサイズされていましたが、MakeGoalはサイズを指定できないため64x64で貼り付けられます。 */\n",
                        w, h);
            fprintf(f, "        MakeGoal(h, %d, %d);\n", r.left, r.top);
        }
        else if (d->text[0] != '\0')
            fprintf(f, "        CreateGameWindow(h, %s, %s, %d, %d, %d, %d, \"%s\");\n",
                    WindowKindName(d->kind), CapsToExprString(d->capabilities), r.left, r.top, w, h, d->text);
        else
            fprintf(f, "        CreateGameWindow(h, %s, %s, %d, %d, %d, %d, NULL);\n",
                    WindowKindName(d->kind), CapsToExprString(d->capabilities), r.left, r.top, w, h);
    }

    /* 静的NoEntryZoneはg_windows[]の対象外(GameWindowではない)なので別途
       書き出す。実際のステージコードと同じくNoEntry_AddZone呼び出し列。 */
    for (int i = 0; i < g_noEntryZoneCount; i++)
    {
        RECT z = g_noEntryZones[i];
        fprintf(f, "        NoEntry_AddZone(h, %d, %d, %d, %d);\n",
                z.left, z.top, z.right - z.left, z.bottom - z.top);
    }

    /* プレイヤーもg_windows[]の対象外なので別途書き出す。stage.cの各stage
       caseがg_playerStartX/Yへ直接代入しているのと同じ形式 -- テストモードで
       パレットからプレイヤーの位置を動かして確定した現在位置をそのまま
       貼り付けられる。 */
    Player *p = Player_GetActive();
    if (p)
    {
        RECT pb;
        Player_GetBounds(p, &pb);
        fprintf(f, "        g_playerStartX = %d;\n", pb.left);
        fprintf(f, "        g_playerStartY = %d;\n", pb.top);
    }

    fclose(f);
}

void Editor_HandleDeleteInput(void)
{
    if (!g_isTestStage)
        return;

    /* 押しっぱなしでカーソル直下のウィンドウを連続的に消し続けてしまわない
       よう、キーが実際に「今フレーム押された」エッジでのみ発火させる。 */
    static int wasDown = 0;
    int isDown = (GetAsyncKeyState(VK_DELETE) & 0x8000) != 0;
    if (isDown && !wasDown)
    {
        POINT cur;
        GetCursorPos(&cur);

        /* 静的NoEntryZoneのマーカーはクリックスルー(WS_EX_TRANSPARENT)の
           ためWindowFromPointでは検出できない。実ゲームプレイでも常に
           最前面(WS_EX_TOPMOST)に縞模様が描かれる=見た目上いちばん手前に
           あるため、まずゾーンを優先してヒットテストする。複数重なって
           いる場合は配列の後ろ（＝後から置いた方）を優先する。 */
        int zoneHit = -1;
        for (int i = g_noEntryZoneCount - 1; i >= 0; i--)
        {
            RECT z = g_noEntryZones[i];
            if (cur.x >= z.left && cur.x < z.right && cur.y >= z.top && cur.y < z.bottom)
            {
                zoneHit = i;
                break;
            }
        }

        if (zoneHit >= 0)
        {
            NoEntry_RemoveZone(zoneHit);
        }
        else
        {
            HWND hit = WindowFromPoint(cur);
            int index = FindWindowIndex(hit);
            /* パレットアイコン/ツールバーボタン自身は削除対象から除外する --
               これらはエディターのUIそのものであり「配置したウィンドウ」では
               ない。それ以外は種別を問わず（Goalも含め）削除できる。 */
            if (index >= 0 && !g_windows[index].isEditorChrome)
                DeleteWindow(index);
        }
    }
    wasDown = isDown;
}

void Editor_HandlePendingCommit(void)
{
    if (!g_isTestStage)
        return;

    /* Deleteキーと同じくエッジ検出: 押しっぱなしで連続確定してしまわない
       ようにする（もっとも、確定後はg_pendingIndexが-1になるため実害は
       無いが、他のエッジ検出処理と挙動を揃えておく）。 */
    static int wasDown = 0;
    int isDown = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
    if (isDown && !wasDown && g_pendingIndex >= 0)
        Editor_CommitPending();
    wasDown = isDown;
}

#endif /* ENABLE_STAGE_EDITOR */
