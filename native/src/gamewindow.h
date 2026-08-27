#ifndef GAMEWINDOW_H
#define GAMEWINDOW_H

#include <windows.h>

#define MAX_WINDOWS 32
#define MAX_CHILDREN 8
#define MAX_NOENTRY_ZONES 8
#define MIN_WINDOW_SIZE 100
/* ゲーム描画のカスタムタイトルバーの高さ(px)。WS_CAPTIONを使わず、
   クライアント領域最上部にこの高さ分のタイトルバー帯を自前で描画する
   （PaintGameWindow参照）。 */
#define TITLE_BAR_HEIGHT 30
/* 制限なしリサイズ+反転ウィンドウ(WT_UNCONSTRAINED*)専用の絶対サイズ下限。
   実HWNDは常に正サイズを維持する必要があるため、論理サイズが0を跨いでも
   実際のウィンドウが完全に潰れないよう、MIN_WINDOW_SIZEより小さいこの値を
   絶対値の下限として使う。 */
#define UNCONSTRAINED_MIN_ABS_SIZE 20
/* 最小化/復元カスタムアニメーションの再生時間(秒)とアニメーション先の
   点サイズ(px)。0x0だとSetWindowPos/GDIが扱いにくいため小さな正方形にする。 */
#define MINIMIZE_ANIM_DURATION 0.18f
#define MINIMIZE_ANIM_POINT_SIZE 4
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
    WT_UNCONSTRAINED,        /* 制限なしリサイズ+反転ウィンドウ */
    WT_UNCONSTRAINED_NOENTRY,
    WT_GOAL,
    WT_BTN_START,
    WT_BTN_RETRY,
    WT_BTN_TOTITLE,
    WT_BTN_EXIT,
#ifdef ENABLE_STAGE_EDITOR
    /* ステージエディター（テストステージ）専用。開発ビルド(build.bat dev)
       でのみ存在する -- 本番ビルドにはコンパイル自体されない。
       背景用のキャンバスは廃止し、本物のデスクトップがそのまま見える/床になる
       （通常プレイでウィンドウの外にいる場合と同じ挙動）。 */
    WT_BTN_PALETTE,    /* ドラッグ&ドロップで配置する小さなパレットアイコン。
                          実際の種別はGameWindowData.paletteKind */
    WT_BTN_TEST,       /* タイトル画面の「Test」ボタン */
    WT_BTN_EXPORT,     /* 配置内容をstage_export.txtへ書き出す */
    WT_BTN_RESET,      /* テストステージを空の状態に作り直す */
#endif
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

    /* ウィンドウ外観カスタマイズ（SetWindowAppearance）。hasCustomAppearance=0の
       間はkind別のデフォルト配色（DrawTitleBar/アウトライン描画側の分岐参照）を使う。
       CreateGameWindowIndexedのZeroMemoryにより未設定時は自動的に0/空文字になる。 */
    int hasCustomAppearance;
    COLORREF titleBarBg;
    COLORREF titleBarFg;
    COLORREF outlineColor;
    char titleText[64];

    /* 制限なしリサイズ+反転ウィンドウ(WT_UNCONSTRAINED*)専用。実HWNDのサイズは
       常にabs(logicalW), abs(logicalH)。符号が負の軸はミラー描画される
       （PaintGameWindowのStretchBlt参照）。それ以外の種別では常に0以上。 */
    int logicalW, logicalH;
    POINT unconstrainedAnchor; /* ドラッグ開始時に固定される左上アンカー点（スクリーン座標） */

    /* 祖先の制限なしリサイズが反転する「たびに」XORで積算される見た目だけの
       ミラーフラグ。ウィンドウ自身の種別に関わらず持つ（子孫すべてが対象）。
       親から切り離されても値はそのまま保持され、再度いずれかの祖先が反転
       イベントを起こすまで変化しない（Hierarchy_ToggleInheritedFlip参照）。
       描画時はこれと自分自身の反転状態(WT_UNCONSTRAINED*かつlogicalW/H<0)を
       XORした結果を最終的な見た目の反転として使う（PaintGameWindow参照）。 */
    int inheritedFlipX, inheritedFlipY;

    /* 最小化/復元のカスタム縮小・拡大アニメーション状態。WS_CAPTIONを外した
       ことでOS標準の最小化ジーニーアニメーションが使えなくなった代替
       （MinimizeAnim_UpdateAll参照）。0=アニメーションなし、1=最小化中
       （縮小してから実際にOS最小化する）、2=復元中（既にOS復元済みで
       見た目だけ拡大する）。 */
    int minimizeAnimState;
    float minimizeAnimT;
    RECT minimizeAnimFrom;
    RECT minimizeAnimTo;

#ifdef ENABLE_STAGE_EDITOR
    /* WT_BTN_PALETTEの場合のみ意味を持つ: ドラッグでどの種別を配置するか、
       ドラッグ終了後にどの位置・サイズへ戻るか。paletteIsNoEntryは
       paletteKindがNoEntry系かどうか（アイコンに縞模様枠を描くかの判定用、
       GetKindAppearanceの結果をEditor_LoadTestStageで一度だけ計算して
       キャッシュしておく）。paletteIconSizeはドラッグしていない時の
       アイコンサイズ、ドラッグ中はEDITOR_DEFAULT_SIZE相当まで拡大される。 */
    WindowKind paletteKind;
    int paletteIsNoEntry;
    POINT paletteHomePos;
    SIZE paletteIconSize;
    int paletteDragging;
#endif
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

/* 最小化/復元中のウィンドウの縮小・拡大アニメーションを1フレーム分進める。
   メインループから毎フレーム呼ぶ（NoEntry_UpdateAnimationと同じ位置づけ）。 */
void MinimizeAnim_UpdateAll(float dt);

/* ウィンドウ外観をkind別デフォルトから上書きする。titleTextがNULLまたは
   空文字なら固定文字列"WindowAction"のまま。生成直後に必要なウィンドウ
   にだけ呼び出す（CreateGameWindow自体のシグネチャは変更しない）。 */
void SetWindowAppearance(int index, COLORREF titleBarBg, COLORREF titleBarFg,
                          COLORREF outlineColor, const char *titleText);

/* kind別のデフォルトisNoEntryだけを取り出す軽量アクセサ（ステージエディターの
   パレットアイコンが、実際には生成していないkindのNoEntry表示を借りるため）。 */
int WindowKind_IsNoEntry(WindowKind kind);

/* このウィンドウが現在、実際に見た目としてミラー描画されている軸を返す
   （PaintGameWindowのStretchBlt判定と厳密に一致させる）:
   自分自身がWT_UNCONSTRAINED*で負の論理サイズを持つ状態(own)と、
   祖先の反転イベントで積算された永続フラグ(inherited)のXOR。
   WindowQuery_GetClientBounds（タイトルバー帯を上端/下端どちらから
   除外するか）など、描画以外で「今の見た目の向き」が必要な箇所からも呼ぶ。 */
void GameWindow_GetEffectiveFlip(const GameWindowData *data, int *outFlipX, int *outFlipY);

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
