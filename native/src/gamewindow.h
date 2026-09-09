#ifndef GAMEWINDOW_H
#define GAMEWINDOW_H

/* DWMのアイコン化サムネイル/ライブプレビューAPI(dwmapi.h、gamewindow.c参照)
   はWindows 7以降が対象。 */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <windows.h>
#include <vector>
#include <memory>

/* テストモードのパレット+ツールバー自体が常時22枠(#ifdef ENABLE_STAGE_EDITOR
   時点でのkPalette 19項目+ツールバー3項目、editor.c参照)を使い切ってしまう
   ため、以前の32では実際にテストで配置できる余地が10枠しか残らず、少し
   多めに配置しただけでCreateGameWindowIndexedが上限に達して新規ウィンドウ
   もパレットアイコンの複製(RespawnPaletteIcon)も生成できなくなり、パレット
   のマスが復活しなくなる不具合があった（実際に報告された不具合）。本番
   ビルドはこのパレットを一切持たないため実質的な下限は変わらない。 */
#define MAX_WINDOWS 64
#define MAX_CHILDREN 8
#define MAX_NOENTRY_ZONES 8
#define MIN_WINDOW_SIZE 100
/* 通常のResizableウィンドウ自身にはMIN_WINDOW_SIZEの下限がかかるが、その
   子（ネストされたウィンドウ・プレイヤー・Goal・ボタン）には課さない --
   Hierarchy_ApplyRelativeTransform/Player_ApplyParentRelativeTransformへ
   minSizeとして渡す。子は親が縮む分だけ比例して縮むため、結果的に親自身の
   サイズ制限は（親を超えては縮小/拡大できないという形で）間接的に受ける。
   0/負のサイズにだけはならないよう1を下限にする。 */
#define CHILD_UNBOUNDED_MIN_SIZE 1
/* ゲーム描画のカスタムタイトルバーの高さ(px)。WS_CAPTIONを使わず、
   クライアント領域最上部にこの高さ分のタイトルバー帯を自前で描画する
   （PaintGameWindow参照）。 */
#define TITLE_BAR_HEIGHT 30
/* 親を持たないウィンドウ（NoEntryでも外観カスタマイズ済みでもない）の既定
   アウトライン。OSの通常ウィンドウに近い控えめな見た目にするため、種別を
   問わず単一色の細い枠に統一する -- 地面（隣接ウィンドウとの境界）の位置が
   分かる程度の最小限の太さにとどめる。 */
#define DEFAULT_OUTLINE_COLOR RGB(120, 120, 120)
#define DEFAULT_OUTLINE_WIDTH_NO_PARENT 1
/* 親を持つウィンドウのアウトライン太さ。CalculateOutlineColorで親の背景色
   から算出した色を使い、親を持たないウィンドウよりはっきり太く描くことで、
   どのウィンドウがどれの子なのか一目で分かるようにする。 */
#define PARENT_OUTLINE_WIDTH 5
/* WC_RESIZE_FLIP_X/Yを持つ軸(反転可能なリサイズ)専用の絶対サイズ下限。
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

/* コンポーネント化(WindowCapabilities参照)により、以前あったWT_MOVABLE/
   WT_RESIZABLE/WT_MOVABLE_NOENTRY/WT_RESIZABLE_NOENTRY/WT_MINIMIZABLE/
   WT_MINIMIZABLE_NOENTRY/WT_UNCONSTRAINED/WT_UNCONSTRAINED_NOENTRY/
   WT_DELETABLE/WT_NORMAL_BLACK_NOENTRY/WT_NORMAL_WHITE_NOENTRYの11種は
   撤去した。kindは「見た目の土台」（黒/白/テキスト表示/ゴール/ボタン）
   だけを表し、振る舞いの組み合わせはCreateGameWindow(Indexed)の
   WindowCapabilities引数で指定する。 */
typedef enum {
    WT_NORMAL_BLACK,
    WT_NORMAL_WHITE,
    WT_TEXT_DISPLAY,
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

/* ウィンドウの振る舞いをパーツ単位で自由に組み合わせるためのビットフラグ。
   以前は「移動できる」「リサイズできる」「不可侵」等が全てWindowKindの
   別々のenum値(および対応する派生クラス)として存在し、組み合わせの数だけ
   enum値が必要だった（移動もリサイズもできるウィンドウは作れなかった）。
   移動・リサイズは全方位ではなく縦横independentのフラグにし、両方
   立てることで従来の全方位の挙動になる。WC_RESIZE_FLIP_X/Yは対応する
   WC_RESIZE_X/Yが立っていて初めて意味を持つ修飾フラグ（以前のWT_UNCONSTRAINED
   相当: サイズが0を跨いで反転できる）。WC_NOENTRYは以前のisNoEntryフィールドを
   統合したもの。 */
enum WindowCapabilities : unsigned int
{
    WC_NONE = 0,
    WC_MOVE_X = 1u << 0,
    WC_MOVE_Y = 1u << 1,
    WC_RESIZE_X = 1u << 2,
    WC_RESIZE_Y = 1u << 3,
    WC_RESIZE_FLIP_X = 1u << 4, /* WC_RESIZE_Xの修飾子: 論理サイズが負(反転)を跨げる */
    WC_RESIZE_FLIP_Y = 1u << 5, /* WC_RESIZE_Yの修飾子: 同上 */
    WC_MINIMIZE = 1u << 6,
    WC_DELETE = 1u << 7,
    WC_NOENTRY = 1u << 8,
};

inline WindowCapabilities operator|(WindowCapabilities a, WindowCapabilities b)
{
    return static_cast<WindowCapabilities>(static_cast<unsigned int>(a) | static_cast<unsigned int>(b));
}
inline WindowCapabilities operator&(WindowCapabilities a, WindowCapabilities b)
{
    return static_cast<WindowCapabilities>(static_cast<unsigned int>(a) & static_cast<unsigned int>(b));
}
inline WindowCapabilities &operator|=(WindowCapabilities &a, WindowCapabilities b) { return a = a | b; }

inline bool HasCapability(WindowCapabilities caps, WindowCapabilities flag)
{
    return (static_cast<unsigned int>(caps) & static_cast<unsigned int>(flag)) != 0;
}

struct GameWindowData
{
    HWND hwnd;
    WindowKind kind;
    COLORREF bg;
    COLORREF fg;
    char text[64];

    int solid;   /* プレイヤーが上に乗れるか */
    WindowCapabilities capabilities; /* 旧isNoEntryフィールドはWC_NOENTRYへ統合した */
    int minimized;
    int hidden;  /* 完全に非表示（最小化ウィンドウ用） */

    /* 階層構造 */
    int parentIdx; /* -1 = 親なし */
    int childIdx[MAX_CHILDREN];
    int childCount;
    SIZE origSize;      /* リサイズ操作開始時点のサイズ（スケール伝播用） */
    int origSizeGen;    /* origSizeが最後に確立された時点のg_resizeGeneration値 -- Hierarchy_ApplyRelativeTransform参照 */
    RECT origBoundsAtResizeStart;

    /* WC_MOVE_X/Yを持つウィンドウのドラッグ状態。lastMouseはMovableWindowStrategy.
       lastMousePosと同じく、ドラッグ開始時（OnMouseDown）に一度だけ記録され、
       ドラッグ中は更新しない -- 「前フレームのカーソル位置」ではなく
       「ドラッグ開始時のカーソル位置」を意味する。毎フレームの移動量は
       常にこの固定点からの累積差分として計算される（UpdateDrag参照）。 */
    int dragging;
    POINT lastMouse;
    int blockedL, blockedR, blockedU, blockedD;

    /* WC_RESIZE_X/Yを持つウィンドウのドラッグ状態。resizeOrigSizeは
       WC_RESIZE_FLIP_X/Yが立っていない軸では実outer矩形の幅/高さ、立っている
       軸では符号付きの論理サイズを意味する(UpdateResize参照)。 */
    int resizing;
    POINT resizeDragStart;
    SIZE resizeOrigSize;

    /* NoEntry縞模様アニメーションの位相（WC_NOENTRYを持つ場合のみ意味を持つ） */
    float stripeOffset;

    /* ウィンドウ外観カスタマイズ（SetWindowAppearance）。hasCustomAppearance=0の
       間はkind別のデフォルト配色（DrawTitleBar/アウトライン描画側の分岐参照）を使う。
       CreateGameWindowIndexedのZeroMemoryにより未設定時は自動的に0/空文字になる。 */
    int hasCustomAppearance;
    COLORREF titleBarBg;
    COLORREF titleBarFg;
    COLORREF outlineColor;
    char titleText[64];

    /* WC_RESIZE_FLIP_X/Yを持つウィンドウ専用。実HWNDのサイズは常にabs(logicalW),
       abs(logicalH)。符号が負の軸はミラー描画される（PaintGameWindowの
       StretchBlt参照）。反転を許可していない軸では常に0以上。 */
    int logicalW, logicalH;
    POINT unconstrainedAnchor; /* ドラッグ開始時に固定される左上アンカー点（スクリーン座標） */

    /* 祖先の反転可能リサイズが反転する「たびに」XORで積算される見た目だけの
       ミラーフラグ。ウィンドウ自身のcapabilitiesに関わらず持つ（子孫すべてが
       対象）。親から切り離されても値はそのまま保持され、再度いずれかの祖先が
       反転イベントを起こすまで変化しない（Hierarchy_ToggleInheritedFlip参照）。
       描画時はこれと自分自身の反転状態(WC_RESIZE_FLIP_X/Yを持ちlogicalW/H<0)を
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
    /* 縮小アニメーションを開始する直前にフルサイズの見た目を1回だけ
       キャプチャしておいたもの。DWMのタスクバーサムネイル/ライブプレビューは
       ウィンドウが最後に描画した内容をそのまま使うため、何もしないと
       数px四方まで縮んだ後の内容がサムネイルになってしまう
       （WM_DWMSENDICONICTHUMBNAIL/WM_DWMSENDICONICLIVEPREVIEWBITMAP参照）。 */
    HBITMAP iconicBitmap;

#ifdef ENABLE_STAGE_EDITOR
    /* WT_BTN_PALETTEの場合のみ意味を持つ: ドラッグでどの土台(kind)+
       どのcapabilities組み合わせを配置するか、ドラッグ終了後にどの位置・
       サイズへ戻るか。NoEntry込みかどうかは`HasCapability(paletteCaps,
       WC_NOENTRY)`で直接判定する（以前のpaletteIsNoEntryキャッシュ用
       フィールドは撤去、capabilitiesが既にビットとして持っているため
       冗長）。paletteIconSizeはドラッグしていない時のアイコンサイズ、
       ドラッグ中はEDITOR_DEFAULT_SIZE相当まで拡大される。paletteIsZoneは、
       このアイコンが実際にはGameWindowではなく静的NoEntryZone
       （NoEntry_AddZone、矩形+クリックスルーの縞模様マーカー）を配置する
       ための特殊なパレット項目であることを示す -- trueの場合、
       paletteKind/paletteCapsは見た目（背景色/縞模様枠）を借りるためだけに
       使われ、ドロップ時にCreateGameWindowIndexedではなくNoEntry_AddZoneが
       呼ばれる（Editor_EndPaletteDrag参照）。paletteIsPlayerStartは同様の
       特殊項目で、GameWindow/NoEntryZoneのどちらでもなく、既存のg_player
       （シングルトン）をドロップ位置へ移動させ、その位置を新しい初期
       出現位置として再確立する（Editor_CommitPending参照）。paletteIsPartは
       「土台」ではなく「能力パーツ」であることを示す -- trueの場合、単体
       ドロップでは仮置き状態に入らず、代わりにその時点で重なっている仮置き
       ウィンドウのpaletteCapsへ自身のpaletteCaps(単一または少数のビット)を
       OR結合してから、常にホームポジションへ戻る（消費型の操作。Editor_
       EndPaletteDrag参照）。仮置きが無い、または仮置き中のものが
       Zone/PlayerStartの場合は何も起きずホームへ戻るだけ。 */
    WindowKind paletteKind;
    WindowCapabilities paletteCaps;
    int paletteIsZone;
    int paletteIsPlayerStart;
    int paletteIsPart;
    POINT paletteHomePos;
    SIZE paletteIconSize;
    int paletteDragging;

    /* 配置待ち（ドロップ済み・半透明・Enterキー確定待ち）アイテムのマウス
       操作状態専用。0=無し、1=位置移動中（本体ドラッグ）、2=大きさ調整中
       （右下角ドラッグ）。実ゲームプレイのdragging/resizingフィールドとは
       意図的に分離している -- Strategy_UpdateAllはkindに関わらずdragging/
       resizingを毎フレーム見て自動的にUpdateMovable/UpdateResizableを
       呼んでしまうため、それらを流用すると実配置用の衝突判定・最小
       サイズ制約(MIN_WINDOW_SIZE)・階層更新が、まだ実体化していない仮
       アイテムにまで二重に適用されてしまう（Editor_StartPaletteDrag等参照）。 */
    int pendingGesture;
    POINT pendingGestureStart;   /* ジェスチャー開始時のカーソル位置（スクリーン座標） */
    RECT pendingGestureOrigRect; /* ジェスチャー開始時のウィンドウ矩形 */

    /* このウィンドウがエディター自身のUI要素（パレットアイコン/ツールバー
       ボタン）であり、ユーザーが「配置した」ものではないことを示す。
       WT_BTN_TOTITLE等、実ゲームプレイでも使う種別をツールバーの固定
       ナビゲーションボタンとパレットからの配置可能項目の両方に使い回す
       ため、kindだけでは区別できない -- Editor_ExportStage（書き出し対象
       から除外）とEditor_HandleDeleteInput（削除対象から除外）はこの
       フラグを見る。CreateGameWindowIndexedのZeroMemoryにより、通常の
       配置（ドラッグ&ドロップ）で生成されたウィンドウでは常に0のまま。 */
    int isEditorChrome;
#endif

    /* コンポーネント化により、以前ここにあったMovableWindow/ResizableWindow/
       UnconstrainedWindow/MinimizableWindow/DeletableWindowという「1振る舞い
       1派生クラス」の単一継承階層は撤去した -- 単一継承では「移動もリサイズも
       できるウィンドウ」を表現できないため。今はcapabilities(WindowCapabilities
       参照)のビットを見て分岐する通常のメンバ関数として実装する(strategy.cpp/
       gamewindow.cpp)。ボタン(下記)だけは個別のOnClick文言を持つため引き続き
       仮想関数のままにする。 */
    virtual ~GameWindowData() = default;
    void OnMouseDown(int index);
    void UpdateDrag(int index);
    void UpdateResize(int index);
    void DrawStrategyMark(HDC hdc, RECT rc, COLORREF color, COLORREF resizeColor) const;
    virtual void OnClick() {}
};

/* ---- Strategy_HandleButtonClickのswitch文を置き換えるボタン派生クラス群。 ---- */

struct StartButton : GameWindowData { void OnClick() override; };
struct RetryButton : GameWindowData { void OnClick() override; };
struct ToTitleButton : GameWindowData { void OnClick() override; };
struct ExitButton : GameWindowData { void OnClick() override; };
#ifdef ENABLE_STAGE_EDITOR
struct TestButton : GameWindowData { void OnClick() override; };
struct ExportButton : GameWindowData { void OnClick() override; };
struct ResetButton : GameWindowData { void OnClick() override; };
#endif

/* C++移行フェーズ2: 以前は固定長のGameWindowData g_windows[MAX_WINDOWS]配列
   だったものを、std::vector<std::unique_ptr<GameWindowData>>で保持する
   クラスに置き換える。ただしインデックスの安定性という核心的な性質は
   完全に維持する -- DeleteWindowは要素を削除(erase)せず、その場でhwnd等を
   クリアするだけの「墓標(tombstone)」方式であり、これはPlayer::parentIdxや
   GameWindowData::childIdx[]のような、フレームをまたいで保持され続ける
   インデックス参照が後から無効にならないために必須の設計（元のC実装から
   引き継いだもの）。operator[]がGameWindowData&を返すため、既存の
   g_windows[i]やg_windows[i].field、&g_windows[i]という呼び出し側の
   構文は一切変更せずそのまま動く。Add()/Clear()がg_windowCountの更新も
   内部で行うため、呼び出し側で二重管理する必要がない。 */
class WindowRegistry
{
public:
    GameWindowData &operator[](int i) { return *slots_[i]; }
    const GameWindowData &operator[](int i) const { return *slots_[i]; }

    /* kindに応じた派生クラス(ボタン系のみ、それ以外は基底GameWindowData)の
       インスタンスを新しいスロットとして追加し、ゼロ初期化済みのポインタを
       返す(std::make_uniqueによる値初期化は、フィールドをZeroMemoryしていた
       以前の実装と完全に等価 -- 仮想関数を持つようになった後もクラスに
       ユーザー定義コンストラクタが無い限りこの等価性は保たれる)。 */
    GameWindowData *Add(WindowKind kind);

    /* 全スロットを破棄し空にする(ResetWindowRegistry専用)。 */
    void Clear();

private:
    std::vector<std::unique_ptr<GameWindowData>> slots_;
};

extern WindowRegistry g_windows;
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

int CreateGameWindowIndexed(HINSTANCE hInstance, WindowKind kind, WindowCapabilities caps,
                             int x, int y, int w, int h, const char *text);
HWND CreateGameWindow(HINSTANCE hInstance, WindowKind kind, WindowCapabilities caps,
                      int x, int y, int w, int h, const char *text);

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

/* このウィンドウが現在、実際に見た目としてミラー描画されている軸を返す
   （PaintGameWindowのStretchBlt判定と厳密に一致させる）:
   自分自身がWC_RESIZE_FLIP_X/Yを持ち負の論理サイズになっている状態(own)と、
   祖先の反転イベントで積算された永続フラグ(inherited)のXOR。
   WindowQuery_GetClientBounds（タイトルバー帯を上端/下端どちらから
   除外するか）など、描画以外で「今の見た目の向き」が必要な箇所からも呼ぶ。 */
void GameWindow_GetEffectiveFlip(const GameWindowData *data, int *outFlipX, int *outFlipY);

/* WC_MINIMIZEを持つウィンドウのタイトルバー右上に置く、最小化専用の当たり
   判定/描画領域。ウィンドウ全体クリックでの最小化トグルは(Movable/Resizable
   と同時に持てるようにするため)廃止し、この小さな領域をクリックした時だけ
   最小化する。`fullBounds`はGetWindowFullBoundsの結果を渡す。 */
RECT GetMinimizeButtonRect(RECT fullBounds);

/* 片軸のみ(X軸のみ、またはY軸のみ)のリサイズ能力を持つウィンドウ専用:
   ウィンドウ全体ではなく該当する縁（X軸のみなら右端、Y軸のみなら下端）を
   つかんだ場合だけリサイズを開始できるようにする当たり判定帯（実際に
   要望された挙動）。両軸とも持つウィンドウには使わない -- そちらは従来通り
   ウィンドウ全体のクリックでリサイズを開始する。`fullBounds`は
   GetWindowFullBoundsまたはGetWindowRectの結果を渡す（Strategy_HandleMouseDown/
   GameWindowProcのWM_SETCURSOR両方で使うため、スクリーン座標系であることが
   前提）。 */
RECT GetResizeEdgeZoneRight(RECT fullBounds);
RECT GetResizeEdgeZoneBottom(RECT fullBounds);

/* Deletable戦略: 子要素を切り離し（破棄はせず親なし状態にする）、ウィンドウ自身も
   その親から切り離した上で破棄する。DeletableWindowStrategy.RemoveAndCloseと一致させる。
   既にトゥームストーン化済み（hwnd==NULL）のインデックスに対して呼んでも安全 -- 何もしない。 */
void DeleteWindow(int index);

/* 実際のプラットフォーム/ゲームウィンドウ（Normal*、および任意の
   capabilities組み合わせを持つそれら）に対してtrueを返す -- ボタンと
   ゴールマーカーは除外する。 */
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
