#include "editor.h"

#ifdef ENABLE_STAGE_EDITOR

#include "player.h"
#include "hierarchy.h"
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

static int g_isTestStage = 0;
static HINSTANCE g_hInstance = NULL;
/* パレット+ツールバー全体を囲む矩形（Editor_LoadTestStageで一度だけ計算）。
   ドロップ判定（この外に離されたら実際に配置する）に使う。 */
static RECT g_panelBounds;

typedef struct
{
    WindowKind kind;
    const char *label;
} PaletteEntry;

/* パレットに並べる配置候補。ボタン/キャンバス自身やGoal以外の
   「実際にステージへ置きうる」全種別を網羅する。 */
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
};
#define PALETTE_COUNT (sizeof(kPalette) / sizeof(kPalette[0]))

int Editor_IsTestStage(void) { return g_isTestStage; }

void Editor_LoadTestStage(HINSTANCE hInstance)
{
    ResetWindowRegistry();
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
            g_windows[idx].paletteIsNoEntry = WindowKind_IsNoEntry(kPalette[i].kind);
            g_windows[idx].paletteHomePos.x = x;
            g_windows[idx].paletteHomePos.y = y;
            g_windows[idx].paletteIconSize.cx = PALETTE_ICON_SIZE;
            g_windows[idx].paletteIconSize.cy = PALETTE_ICON_SIZE;
        }
    }

    int gridRows = ((int)PALETTE_COUNT + PALETTE_COLS - 1) / PALETTE_COLS;
    int gridRight = PALETTE_X + PALETTE_COLS * (PALETTE_ICON_SIZE + PALETTE_GAP);
    int gridBottom = PALETTE_Y + gridRows * (PALETTE_ICON_SIZE + PALETTE_GAP);

    /* ツールバー（グリッドの下に続けて配置）。 */
    int py = gridBottom + TOOLBAR_GAP;
    CreateGameWindow(hInstance, WT_BTN_EXPORT, PALETTE_X, py, TOOLBAR_BTN_W, TOOLBAR_BTN_H, "Export");
    py += TOOLBAR_BTN_H + TOOLBAR_GAP;
    CreateGameWindow(hInstance, WT_BTN_RESET, PALETTE_X, py, TOOLBAR_BTN_W, TOOLBAR_BTN_H, "Reset");
    py += TOOLBAR_BTN_H + TOOLBAR_GAP;
    CreateGameWindow(hInstance, WT_BTN_TOTITLE, PALETTE_X, py, TOOLBAR_BTN_W, TOOLBAR_BTN_H, "Title");
    py += TOOLBAR_BTN_H;

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

/* ドラッグ中のアイコンの一辺の長さ。配置先に実際どの大きさで置かれるかが
   一目で分かるよう、待機時の小さなアイコンではなく実配置サイズまで拡大する。 */
static int DragFullSize(WindowKind kind)
{
    return (kind == WT_GOAL) ? EDITOR_GOAL_SIZE : EDITOR_DEFAULT_SIZE;
}

void Editor_StartPaletteDrag(int index)
{
    GameWindowData *d = GetWindowData(index);
    if (!d)
        return;
    d->paletteDragging = 1;
    SetCapture(d->hwnd);

    /* ドラッグ中は「配置されるものそのもの」を実サイズで半透明表示する
       （小さなボタンのままカーソルに追従するのではなく）。 */
    int size = DragFullSize(d->paletteKind);
    POINT cur;
    GetCursorPos(&cur);
    SetWindowPos(d->hwnd, NULL, cur.x - size / 2, cur.y - size / 2, size, size,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    SetLayeredWindowAttributes(d->hwnd, 0, PALETTE_DRAG_ALPHA, LWA_ALPHA);
}

void Editor_UpdatePaletteDrags(void)
{
    for (int i = 0; i < g_windowCount; i++)
    {
        GameWindowData *d = &g_windows[i];
        if (d->kind != WT_BTN_PALETTE || !d->paletteDragging || !d->hwnd)
            continue;
        int size = DragFullSize(d->paletteKind);
        POINT cur;
        GetCursorPos(&cur);
        SetWindowPos(d->hwnd, NULL, cur.x - size / 2, cur.y - size / 2, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void Editor_EndPaletteDrag(int index)
{
    GameWindowData *d = GetWindowData(index);
    if (!d || !d->paletteDragging)
        return;
    d->paletteDragging = 0;
    ReleaseCapture();

    /* パレット+ツールバーの矩形の外に離されていれば、実際にそこへ配置する。
       中で離された場合（ドラッグせず単に離した等）は何も配置しない。 */
    POINT cur;
    GetCursorPos(&cur);
    int outside = cur.x < g_panelBounds.left || cur.x > g_panelBounds.right ||
                  cur.y < g_panelBounds.top || cur.y > g_panelBounds.bottom;
    if (outside)
    {
        WindowKind kind = d->paletteKind;
        int size = DragFullSize(kind);
        int newIdx = CreateGameWindowIndexed(g_hInstance, kind, cur.x - size / 2, cur.y - size / 2, size, size, NULL);
        /* WindowMessageHandler.HandleLeftButtonUpと同じく、配置直後に一度だけ
           親子判定を行う -- そうしないとドラッグ&ドロップで置いたウィンドウは
           他のウィンドウの中に完全に収まっていても親子付けされず、その場で
           少し動かす（ナッジドラッグ）まで親子関係が確立しなかった
           （実際に報告された不具合）。 */
        if (newIdx >= 0 && IsQueryableWindow(kind))
            Hierarchy_CheckAndUpdate(newIdx);
    }

    /* アイコン自身は常に元のサイズ・不透明度・ホームポジションへ戻す
       （配置有無に関わらず）。 */
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
    default: return "WT_NORMAL_BLACK";
    }
}

/* エディターのUI要素自身(パレット/ツールバー)を対象から除外するための判定。
   Editor_ExportStage（書き出し対象から除外）とEditor_HandleDeleteInput
   （Deleteキーでの削除対象から除外）の両方で使う。 */
static int IsEditorChromeKind(WindowKind kind)
{
    return kind == WT_BTN_PALETTE || kind == WT_BTN_TEST ||
           kind == WT_BTN_EXPORT || kind == WT_BTN_RESET || kind == WT_BTN_TOTITLE;
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
        if (!d->hwnd || IsEditorChromeKind(d->kind))
            continue;

        RECT r;
        GetWindowRect(d->hwnd, &r);
        int w = r.right - r.left;
        int h = r.bottom - r.top;

        if (d->kind == WT_GOAL)
            fprintf(f, "        MakeGoal(h, %d, %d);\n", r.left, r.top);
        else
            fprintf(f, "        CreateGameWindow(h, %s, %d, %d, %d, %d, NULL);\n",
                    WindowKindName(d->kind), r.left, r.top, w, h);
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
        HWND hit = WindowFromPoint(cur);
        int index = FindWindowIndex(hit);
        /* パレットアイコン/ツールバーボタン自身は削除対象から除外する --
           これらはエディターのUIそのものであり「配置したウィンドウ」では
           ない。それ以外は種別を問わず（Goalも含め）削除できる。 */
        if (index >= 0 && !IsEditorChromeKind(g_windows[index].kind))
            DeleteWindow(index);
    }
    wasDown = isDown;
}

#endif /* ENABLE_STAGE_EDITOR */
