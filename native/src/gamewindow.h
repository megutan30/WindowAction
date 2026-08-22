#ifndef GAMEWINDOW_H
#define GAMEWINDOW_H

#include <windows.h>

#define MAX_WINDOWS 32
#define MAX_CHILDREN 8
#define MAX_NOENTRY_ZONES 8
#define MIN_WINDOW_SIZE 100
/* オリジナルのGameWindow.GetMaximumSize()はint.MaxValueを返す -- 実質的な上限は存在しない
   （settings.jsonにWindow.MaximumSizeの項目自体が無い）。width+delta演算がオーバーフローで
   ラップしないよう、INT_MAXの代わりに十分大きくかつ安全な値を使う。 */
#define MAX_WINDOW_SIZE 1000000

typedef enum {
    WT_NORMAL_BLACK,
    WT_NORMAL_WHITE,
    WT_TEXT_DISPLAY,
    WT_MOVABLE,
    WT_RESIZABLE,
    WT_DELETABLE,
    WT_MINIMIZABLE,
    WT_NORMAL_BLACK_NOENTRY,
    WT_NORMAL_WHITE_NOENTRY,
    WT_RESIZABLE_NOENTRY,
    WT_MOVABLE_NOENTRY,
    WT_MINIMIZABLE_NOENTRY,
    WT_GOAL,
    WT_BTN_START,
    WT_BTN_RETRY,
    WT_BTN_TOTITLE,
    WT_BTN_EXIT
} WindowKind;

typedef struct {
    HWND hwnd;
    WindowKind kind;
    COLORREF bg;
    COLORREF fg;
    char text[64];

    int solid;   /* プレイヤーが上に乗れるか */
    int isNoEntry;
    int minimized;
    int hidden;  /* 完全に非表示（最小化ウィンドウ用） */

    /* 階層構造 */
    int parentIdx; /* -1 = 親なし */
    int childIdx[MAX_CHILDREN];
    int childCount;
    SIZE origSize;      /* リサイズ操作開始時点のサイズ（スケール伝播用） */
    int origSizeGen;    /* origSizeが最後に確立された時点のg_resizeGeneration値 -- Hierarchy_ApplyScale参照 */
    RECT origBoundsAtResizeStart;

    /* Movable（移動可能）ドラッグ状態。lastMouseはMovableWindowStrategy.lastMousePos
       と同じく、ドラッグ開始時（Strategy_HandleMouseDown）に一度だけ記録され、
       ドラッグ中は更新しない -- 「前フレームのカーソル位置」ではなく
       「ドラッグ開始時のカーソル位置」を意味する。毎フレームの移動量は
       常にこの固定点からの累積差分として計算される（CalculateMovement参照）。 */
    int dragging;
    POINT lastMouse;
    int blockedL, blockedR, blockedU, blockedD;

    /* Resizable（リサイズ可能）ドラッグ状態 */
    int resizing;
    POINT resizeDragStart;
    SIZE resizeOrigSize;

    /* NoEntry縞模様アニメーションの位相（isNoEntryの場合のみ意味を持つ） */
    float stripeOffset;
} GameWindowData;

extern GameWindowData g_windows[MAX_WINDOWS];
extern int g_windowCount;

/* リサイズ操作が開始するたびに1回インクリメントされる（Strategy_HandleMouseDownの
   WT_RESIZABLEケース）。ResizableWindowStrategy.ApplyScaleToHierarchyは、操作の開始時点で
   常に有効だった値を使うのではなく、操作中に遅れて子・ゴール・プレイヤー・ボタンとして
   現れた要素のorigSizeを、そのエンティティを操作中に初めて見た時点でcurrentSize/scale
   として遅延的に逆算する（操作ごとの`originalSizes`辞書は空から始まりオンデマンドで
   埋まっていく）。そうしないと、ウィンドウが既にある程度拡大/縮小した後にドラッグ中に
   子になった要素が、初めて触れられた瞬間にcurrentSize * (その時点までに既に経過した
   スケール)へ飛んでしまう。各エンティティ自身のorigSizeGenをこのカウンタと比較することで、
   ネイティブ移植版は「このエンティティが現在の操作に対して確立済みかどうか」を判定する。 */
extern int g_resizeGeneration;

extern RECT g_noEntryZones[MAX_NOENTRY_ZONES];
extern int g_noEntryZoneCount;

void RegisterGameWindowClass(HINSTANCE hInstance);
void ResetWindowRegistry(void); /* 既存のゲームウィンドウを全て破棄し、配列をクリアする */

int CreateGameWindowIndexed(HINSTANCE hInstance, WindowKind kind, int x, int y, int w, int h, const char *text);
HWND CreateGameWindow(HINSTANCE hInstance, WindowKind kind, int x, int y, int w, int h, const char *text);

void GetWindowFullBounds(HWND hwnd, RECT *out);
int FindWindowIndex(HWND hwnd);
GameWindowData *GetWindowData(int index);

void SetWindowMinimized(int index, int minimized);

/* Deletable戦略: 子要素を切り離し（破棄はせず親なし状態にする）、ウィンドウ自身も
   その親から切り離した上で破棄する。DeletableWindowStrategy.RemoveAndCloseと一致させる。
   既にトゥームストーン化済み（hwnd==NULL）のインデックスに対して呼んでも安全 -- 何もしない。 */
void DeleteWindow(int index);

/* 実際のプラットフォーム/ゲームウィンドウ（Normal*, Movable, Resizable, Minimizable,
   NoEntry系）に対してtrueを返す -- ボタンとゴールマーカーは除外する。 */
int IsQueryableWindow(WindowKind kind);
int IsButtonWindowKind(WindowKind kind);

/* g_windows内のWT_GOALエントリのインデックス。現在のステージにゴールが無ければ-1。 */
int FindGoalIndex(void);

/* ゴールを現在完全に含んでいるqueryableウィンドウ（最前面が優先）に基づいて、
   ゴールの親を毎フレーム再評価し、必要に応じてアタッチ/デタッチする。
   Goal.UpdateAsyncから毎フレーム呼ばれるGoal.CheckParentWindowと一致させる --
   これにより、プレイヤーと同様に、ゴールもたまたま乗っているウィンドウに
   追従（かつその中に隠れることが可能）する。 */
void Goal_UpdateParent(void);

/* Goal_UpdateParentと同じ考え方を全てのボタンウィンドウに適用したもの:
   UpdateAsyncから毎フレーム呼ばれるGameButton.CheckParentWindowと一致させる。
   Movable/Resizableウィンドウがボタンの固定位置の上にドラッグされると、
   そのボタンはゴールと同様にそのウィンドウの子となり（そのまま維持され）、
   追従するようになる。 */
void Button_UpdateParent(void);

/* OutlineRenderer.CalculateOutlineColorと一致させる: 暗い親色は明るく、
   明るい親色は暗くすることで、アウトラインが常に背景に対して視認可能な状態を保つ。 */
COLORREF CalculateOutlineColor(COLORREF bg);

#endif
