#include "editor.h"

#ifdef ENABLE_STAGE_EDITOR

#include "player.h"
#include "hierarchy.h"
#include "noentry.h"
#include <stdio.h>

int g_requestTest = 0;

#define EDITOR_DEFAULT_SIZE 250
#define EDITOR_GOAL_SIZE 64
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

/* ドロップ時のTitle/Retryボタンの実サイズ。stage.cのMakeButton(BTN_W/BTN_H)と
   一致させ、実際のステージで使われるのと同じ見た目・当たり判定で試せるように
   する（他の窓種別のような正方形EDITOR_DEFAULT_SIZEにはしない）。 */
#define EDITOR_BTN_W 150
#define EDITOR_BTN_H 40

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
    const char *label;
    /* trueなら、このパレット項目は実際にはGameWindowを生成しない特殊項目
       （現状は静的NoEntryZoneのみ）。kindは見た目（背景色/縞模様枠）を
       借りるためだけに使う。省略時（末尾の値を書かない行）はCの集成体
       初期化規則により自動的に0になる。 */
    int isZone;
} PaletteEntry;

/* パレットに並べる配置候補。ボタン/キャンバス自身以外の
   「実際にステージへ置きうる」全種別を網羅する（Goal、Title/Retryボタン、
   静的NoEntryZoneも含む）。 */
static const PaletteEntry kPalette[] = {
    {WT_NORMAL_BLACK, "Normal(Blk)"},
    {WT_NORMAL_WHITE, "Normal(Wht)"},
    {WT_TEXT_DISPLAY, "TextDisplay"},
    {WT_MOVABLE, "Movable"},
    {WT_RESIZABLE, "Resizable"},
    {WT_DELETABLE, "Deletable"},
    {WT_MINIMIZABLE, "Minimizable"},
    {WT_UNCONSTRAINED, "Unconstrained"},
    {WT_NORMAL_BLACK_NOENTRY, "Blk+NoEntry"},
    {WT_NORMAL_WHITE_NOENTRY, "Wht+NoEntry"},
    {WT_RESIZABLE_NOENTRY, "Resize+NoEntry"},
    {WT_MOVABLE_NOENTRY, "Move+NoEntry"},
    {WT_MINIMIZABLE_NOENTRY, "Mini+NoEntry"},
    {WT_UNCONSTRAINED_NOENTRY, "Unc+NoEntry"},
    {WT_GOAL, "Goal"},
    {WT_BTN_TOTITLE, "Title Btn"},
    {WT_BTN_RETRY, "Retry Btn"},
    {WT_NORMAL_BLACK_NOENTRY, "NoEntry Zone", 1},
};
#define PALETTE_COUNT (sizeof(kPalette) / sizeof(kPalette[0]))

int Editor_IsTestStage(void) { return g_isTestStage; }

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
       何も置かれていない場所ではプレイヤーは画面下端まで落ちる）。 */

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

        int idx = CreateGameWindowIndexed(hInstance, WT_BTN_PALETTE, x, y,
                                           PALETTE_ICON_SIZE, PALETTE_ICON_SIZE, kPalette[i].label);
        if (idx >= 0)
        {
            g_windows[idx].paletteKind = kPalette[i].kind;
            g_windows[idx].paletteIsNoEntry = WindowKind_IsNoEntry(kPalette[i].kind) || kPalette[i].isZone;
            g_windows[idx].paletteIsZone = kPalette[i].isZone;
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
    int exportIdx = CreateGameWindowIndexed(hInstance, WT_BTN_EXPORT, PALETTE_X, py, TOOLBAR_BTN_W, TOOLBAR_BTN_H, "Export");
    py += TOOLBAR_BTN_H + TOOLBAR_GAP;
    int resetIdx = CreateGameWindowIndexed(hInstance, WT_BTN_RESET, PALETTE_X, py, TOOLBAR_BTN_W, TOOLBAR_BTN_H, "Reset");
    py += TOOLBAR_BTN_H + TOOLBAR_GAP;
    int titleIdx = CreateGameWindowIndexed(hInstance, WT_BTN_TOTITLE, PALETTE_X, py, TOOLBAR_BTN_W, TOOLBAR_BTN_H, "Title");
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
   Title/Retryボタンはstage.cのMakeButtonと同じ150x40、Goalは正方形の
   EDITOR_GOAL_SIZE、それ以外（NoEntry Zoneを含む）は正方形のEDITOR_DEFAULT_SIZE。 */
static SIZE DragFullSize(WindowKind kind)
{
    if (kind == WT_GOAL)
        return (SIZE){EDITOR_GOAL_SIZE, EDITOR_GOAL_SIZE};
    if (kind == WT_BTN_TOTITLE || kind == WT_BTN_RETRY)
        return (SIZE){EDITOR_BTN_W, EDITOR_BTN_H};
    return (SIZE){EDITOR_DEFAULT_SIZE, EDITOR_DEFAULT_SIZE};
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
    WindowKind kind = d->paletteKind;

    DeleteWindow(committedIndex);

    if (isZone)
    {
        /* 静的NoEntryZoneはGameWindowではないため、CreateGameWindowIndexed
           ではなくNoEntry_AddZoneで直接追加する（クリックスルーの縞模様
           マーカーが生成される。実ゲームプレイでの静的不可侵領域と全く
           同じ仕組み）。 */
        NoEntry_AddZone(g_hInstance, x, y, w, h);
    }
    else
    {
        int newIdx = CreateGameWindowIndexed(g_hInstance, kind, x, y, w, h, DefaultDropText(kind));
        /* WindowMessageHandler.HandleLeftButtonUpと同じく、配置直後に一度だけ
           親子判定を行う -- そうしないとドラッグ&ドロップで置いたウィンドウは
           他のウィンドウの中に完全に収まっていても親子付けされず、その場で
           少し動かす（ナッジドラッグ）まで親子関係が確立しなかった
           （実際に報告された不具合）。 */
        if (newIdx >= 0 && IsQueryableWindow(kind))
            Hierarchy_CheckAndUpdate(newIdx);
    }
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
        int inCorner = cur.x >= r.right - PENDING_RESIZE_HANDLE && cur.x <= r.right &&
                        cur.y >= r.bottom - PENDING_RESIZE_HANDLE && cur.y <= r.bottom;

        d->pendingGesture = inCorner ? 2 : 1;
        d->pendingGestureStart = cur;
        d->pendingGestureOrigRect = r;
        SetCapture(d->hwnd);
        return;
    }

    /* 既に別のアイテムが配置待ちの状態で新しいアイコンのドラッグを始めた
       場合、前のものを宙ぶらりんにせず自動的にその時点の大きさで確定する。 */
    if (g_pendingIndex >= 0)
        Editor_CommitPending();

    d->paletteDragging = 1;
    SetCapture(d->hwnd);

    /* ドラッグ中は「配置されるものそのもの」を実サイズで半透明表示する
       （小さなボタンのままカーソルに追従するのではなく）。この時点での
       サイズはあくまで既定値 -- ドロップ後、角ドラッグで自由に調整できる。 */
    SIZE size = DragFullSize(d->paletteKind);
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
        SIZE size = DragFullSize(d->paletteKind);
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
   (stage_export.txt)がstage.cへそのまま貼り付け可能なコードになるように。 */
static const char *WindowKindName(WindowKind kind)
{
    switch (kind)
    {
    case WT_NORMAL_BLACK: return "WT_NORMAL_BLACK";
    case WT_NORMAL_WHITE: return "WT_NORMAL_WHITE";
    case WT_TEXT_DISPLAY: return "WT_TEXT_DISPLAY";
    case WT_MOVABLE: return "WT_MOVABLE";
    case WT_RESIZABLE: return "WT_RESIZABLE";
    case WT_DELETABLE: return "WT_DELETABLE";
    case WT_MINIMIZABLE: return "WT_MINIMIZABLE";
    case WT_NORMAL_BLACK_NOENTRY: return "WT_NORMAL_BLACK_NOENTRY";
    case WT_NORMAL_WHITE_NOENTRY: return "WT_NORMAL_WHITE_NOENTRY";
    case WT_RESIZABLE_NOENTRY: return "WT_RESIZABLE_NOENTRY";
    case WT_MOVABLE_NOENTRY: return "WT_MOVABLE_NOENTRY";
    case WT_MINIMIZABLE_NOENTRY: return "WT_MINIMIZABLE_NOENTRY";
    case WT_UNCONSTRAINED: return "WT_UNCONSTRAINED";
    case WT_UNCONSTRAINED_NOENTRY: return "WT_UNCONSTRAINED_NOENTRY";
    case WT_GOAL: return "WT_GOAL";
    case WT_BTN_TOTITLE: return "WT_BTN_TOTITLE";
    case WT_BTN_RETRY: return "WT_BTN_RETRY";
    default: return "WT_NORMAL_BLACK";
    }
}

void Editor_ExportStage(void)
{
    FILE *f = fopen("stage_export.txt", "w");
    if (!f)
        return;

    fprintf(f, "/* Editor_ExportStage の出力。stage.c の Stage_Load 内の\n"
               "   目的の case へそのまま貼り付け可能。座標/サイズは配置時点の実際の\n"
               "   ウィンドウ矩形(GetWindowRect)から取得している。 */\n");

    for (int i = 0; i < g_windowCount; i++)
    {
        GameWindowData *d = &g_windows[i];
        /* isEditorChromeはパレットアイコン/ツールバーボタン自身（ユーザーが
           配置したものではない）を除外する -- WT_BTN_TOTITLEのようにツール
           バーのナビゲーションボタンとパレットから配置可能な実ゲームプレイ
           種別とでkindを使い回しているため、kindだけでは区別できない。 */
        if (!d->hwnd || d->isEditorChrome)
            continue;

        RECT r;
        GetWindowRect(d->hwnd, &r);
        int w = r.right - r.left;
        int h = r.bottom - r.top;

        if (d->kind == WT_GOAL)
            fprintf(f, "        MakeGoal(h, %d, %d);\n", r.left, r.top);
        else if (d->text[0] != '\0')
            fprintf(f, "        CreateGameWindow(h, %s, %d, %d, %d, %d, \"%s\");\n",
                    WindowKindName(d->kind), r.left, r.top, w, h, d->text);
        else
            fprintf(f, "        CreateGameWindow(h, %s, %d, %d, %d, %d, NULL);\n",
                    WindowKindName(d->kind), r.left, r.top, w, h);
    }

    /* 静的NoEntryZoneはg_windows[]の対象外(GameWindowではない)なので別途
       書き出す。実際のステージコードと同じくNoEntry_AddZone呼び出し列。 */
    for (int i = 0; i < g_noEntryZoneCount; i++)
    {
        RECT z = g_noEntryZones[i];
        fprintf(f, "        NoEntry_AddZone(h, %d, %d, %d, %d);\n",
                z.left, z.top, z.right - z.left, z.bottom - z.top);
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
