#include "gamewindow.h"
#include "zorder.h"
#include "gamefont.h"
#include "hierarchy.h"
#include "player.h"
#include "windowquery.h"
#include "noentry.h"
#include "editor.h"
#include "gdiobj.h"
#include <stdio.h>
#include <math.h>
#include <dwmapi.h>

WindowRegistry g_windows;
int g_windowCount = 0;
int g_resizeGeneration = 0;

GameWindowData *WindowRegistry::Add()
{
    slots_.push_back(std::make_unique<GameWindowData>());
    g_windowCount = (int)slots_.size();
    return slots_.back().get();
}

void WindowRegistry::Clear()
{
    slots_.clear();
    g_windowCount = 0;
}

RECT g_noEntryZones[MAX_NOENTRY_ZONES];
int g_noEntryZoneCount = 0;

extern void Strategy_HandleMouseDown(int index);
extern void Strategy_HandleMouseUp(int index);
extern void Strategy_HandleButtonClick(WindowKind kind);

static const char *kGameWindowClass = "WA_GameWindow";
static const int NOENTRY_BORDER_WIDTH = 5;
static const int STRIPE_WIDTH = 20;

/* Draw*Markファミリー(DrawMovableMark/DrawResizableMark/DrawDeletableMark)
   共通の寸法: 中心からの半径と矢印/角の長さ。 */
static const int MARK_RADIUS = 18;
static const int MARK_ARROW_LEN = 6;
static const int MARK_PEN_WIDTH = 3;
/* DrawMinimizableMarkのバー半分の寸法。MinimizableWindowStrategy.
   DrawStrategyMark: markSize=60の半分(30)と、barHeight=markSize/6=10の半分(5)。 */
static const int MINIMIZE_BAR_HALF_W = 30;
static const int MINIMIZE_BAR_HALF_H = 5;

int FindWindowIndex(HWND hwnd)
{
    for (int i = 0; i < g_windowCount; i++)
        if (g_windows[i].hwnd == hwnd)
            return i;
    return -1;
}

GameWindowData *GetWindowData(int index)
{
    if (index < 0 || index >= g_windowCount)
        return NULL;
    return &g_windows[index];
}

int IsButtonWindowKind(WindowKind kind)
{
    if (kind == WT_BTN_START || kind == WT_BTN_RETRY || kind == WT_BTN_TOTITLE || kind == WT_BTN_EXIT)
        return 1;
#ifdef ENABLE_STAGE_EDITOR
    if (kind == WT_BTN_PALETTE || kind == WT_BTN_TEST || kind == WT_BTN_EXPORT || kind == WT_BTN_RESET)
        return 1;
#endif
    return 0;
}

int IsQueryableWindow(WindowKind kind)
{
    return !IsButtonWindowKind(kind) && kind != WT_GOAL;
}

/* Goal/ボタンはウィンドウクロームを持たない（元々枠なしのUI要素）。
   それ以外の全種別はゲーム描画のタイトルバーを持つ。 */
static int HasChrome(WindowKind kind)
{
    return kind != WT_GOAL && !IsButtonWindowKind(kind);
}

/* kind別のデフォルト配色/solid/isNoEntryを返す。CreateGameWindowIndexedの
   ウィンドウ生成時と、PaintGameWindowのパレットアイコン描画（実際には
   生成しないwindowKindの見た目だけを借りる）の両方から使う共通ロジック。 */
static void GetKindAppearance(WindowKind kind, COLORREF *bg, COLORREF *fg, int *solid, int *isNoEntry)
{
    *solid = 1;
    *isNoEntry = 0;
    switch (kind)
    {
    case WT_NORMAL_BLACK:
        *bg = RGB(0, 0, 0);
        *fg = RGB(255, 255, 255);
        break;
    case WT_NORMAL_WHITE:
        *bg = RGB(255, 255, 255);
        *fg = RGB(0, 0, 0);
        break;
    case WT_TEXT_DISPLAY:
        *bg = RGB(0, 0, 0);
        *fg = RGB(255, 255, 255);
        *solid = 0;
        break;
    case WT_MOVABLE:
        *bg = RGB(173, 216, 230);
        *fg = RGB(0, 0, 0);
        break;
    case WT_RESIZABLE:
        *bg = RGB(144, 238, 144);
        *fg = RGB(0, 0, 0);
        break;
    case WT_MINIMIZABLE:
    case WT_DELETABLE:
        *bg = RGB(255, 182, 193);
        *fg = RGB(0, 0, 0);
        break;
    case WT_NORMAL_BLACK_NOENTRY:
        *bg = RGB(0, 0, 0);
        *fg = RGB(255, 255, 255);
        *isNoEntry = 1;
        break;
    case WT_NORMAL_WHITE_NOENTRY:
        *bg = RGB(255, 255, 255);
        *fg = RGB(0, 0, 0);
        *isNoEntry = 1;
        break;
    case WT_RESIZABLE_NOENTRY:
        *bg = RGB(144, 238, 144);
        *fg = RGB(0, 0, 0);
        *isNoEntry = 1;
        break;
    case WT_MOVABLE_NOENTRY:
        *bg = RGB(173, 216, 230);
        *fg = RGB(0, 0, 0);
        *isNoEntry = 1;
        break;
    case WT_MINIMIZABLE_NOENTRY:
        *bg = RGB(255, 182, 193);
        *fg = RGB(0, 0, 0);
        *isNoEntry = 1;
        break;
    case WT_UNCONSTRAINED:
        *bg = RGB(221, 160, 221);
        *fg = RGB(0, 0, 0);
        break;
    case WT_UNCONSTRAINED_NOENTRY:
        *bg = RGB(221, 160, 221);
        *fg = RGB(0, 0, 0);
        *isNoEntry = 1;
        break;
    case WT_GOAL:
        *bg = RGB(255, 0, 255);
        *fg = RGB(255, 215, 0);
        *solid = 0;
        break;
    case WT_BTN_START:
    case WT_BTN_RETRY:
    case WT_BTN_TOTITLE:
    case WT_BTN_EXIT:
        *bg = RGB(200, 200, 200);
        *fg = RGB(0, 0, 0);
        *solid = 0;
        break;
#ifdef ENABLE_STAGE_EDITOR
    case WT_BTN_PALETTE:
    case WT_BTN_TEST:
    case WT_BTN_EXPORT:
    case WT_BTN_RESET:
        *bg = RGB(200, 200, 200);
        *fg = RGB(0, 0, 0);
        *solid = 0;
        break;
#endif
    default:
        *bg = RGB(128, 128, 128);
        *fg = RGB(255, 255, 255);
        break;
    }
}

int WindowKind_IsNoEntry(WindowKind kind)
{
    COLORREF bg, fg;
    int solid, isNoEntry;
    GetKindAppearance(kind, &bg, &fg, &solid, &isNoEntry);
    return isNoEntry;
}

void GameWindow_GetEffectiveFlip(const GameWindowData *data, int *outFlipX, int *outFlipY)
{
    int ownFlipX = (data->kind == WT_UNCONSTRAINED || data->kind == WT_UNCONSTRAINED_NOENTRY) && data->logicalW < 0;
    int ownFlipY = (data->kind == WT_UNCONSTRAINED || data->kind == WT_UNCONSTRAINED_NOENTRY) && data->logicalH < 0;
    *outFlipX = ownFlipX ^ data->inheritedFlipX;
    *outFlipY = ownFlipY ^ data->inheritedFlipY;
}

int FindGoalIndex(void)
{
    for (int i = 0; i < g_windowCount; i++)
        if (g_windows[i].kind == WT_GOAL)
            return i;
    return -1;
}

/* Goal_UpdateParent/Button_UpdateParent専用: 現在の親（あれば）が、辺が
   接している場合も内包とみなす緩い基準でまだboundsを内包しているかを返す。
   WindowQuery_FullyContainsは辺が接しているだけでは内包とみなさない厳密な
   不等号を使う -- これはユーザーが自由にドラッグしている最中の境界での
   ジッター防止には必要だが、Hierarchy_ApplyRelativeTransformによる
   スケール追従は、親自身が同じminSize/maxSizeで頭打ちになっている状況で
   Goal/ボタンを親の境界ぴったりまでクランプすることがある（親も子も
   同じ下限で縮み切った場合など）。この「ぴったり」をその厳密な基準だけで
   評価すると、追従で正しく親の内側に収めているにもかかわらず「もう内包
   されていない」と誤判定して親子関係を解除し続けてしまい、以降
   Hierarchy_ApplyRelativeTransform側の追従対象からも外れてサイズ・位置が
   その場で凍結し、親だけがさらに縮んで最終的にGoal/ボタンがはみ出て見える
   不具合があった（実際に報告された不具合: 制限なしリサイズウィンドウの子
   のとき、プレイヤーやウィンドウと違って途中で縮まなくなる）。
   呼び出し側は、通常の厳密な検索(WindowQuery_GetFullyContaining)が新しい
   親候補を一つも見つけられなかった場合のみ、この関数で現在の親をそのまま
   維持してよいか判定する -- 検索を丸ごとスキップしてはいけない。スキップ
   すると、既に親を持つGoal/ボタンに別のより前面のウィンドウを重ねても
   その新しいウィンドウへの親子付け替えが二度と起こらなくなってしまう
   （実際に報告された不具合）。 */
static int IsStillInclusivelyContainedByCurrentParent(int parentIdx, RECT bounds)
{
    if (parentIdx < 0)
        return 0;
    GameWindowData *parent = GetWindowData(parentIdx);
    if (!parent || !parent->hwnd || parent->minimized)
        return 0;
    RECT pb;
    GetWindowFullBounds(parent->hwnd, &pb);
    return bounds.left >= pb.left && bounds.top >= pb.top &&
           bounds.right <= pb.right && bounds.bottom <= pb.bottom;
}

/* Goal_UpdateParent/Button_UpdateParent共通の本体: インデックス1件分の
   親子再判定を行う。以前はGoal用とButton用で同じ処理が別々に書かれていた
   （Buttonはこれをg_windowCount全体のループでフィルタして回すだけ）。
   isEditorChromeの除外はButtonにしか実質関係しない(パレット/ツールバーの
   固定UIのみ該当)が、Goalに対して評価しても常に0なので安全に共有できる。 */
static void UpdateSpecialChildParent(int idx)
{
    GameWindowData *w = &g_windows[idx];
    if (!w->hwnd || w->minimized)
        return; /* OnRestoreが明示的に再アタッチする; 最小化中は自己追跡しない */
#ifdef ENABLE_STAGE_EDITOR
    /* パレットアイコン/ツールバーボタンはエディター画面上の固定UIであり、
       テストステージ側に配置した（親候補になり得る）ウィンドウの子には
       ならない -- そうしないと、配置したウィンドウがパレットの上に
       重なっただけでパレットがその子になってしまい、位置がその
       ウィンドウの移動に引きずられて動いてしまう（実際に報告された
       不具合）。 */
    if (w->isEditorChrome)
        return;
#endif

    RECT b;
    GetWindowFullBounds(w->hwnd, &b);
    int newParent = WindowQuery_GetFullyContaining(b);

    if (newParent == w->parentIdx)
        return;

    /* 厳密な検索では新しい親候補が見つからなかった(-1)が、現在の親がまだ
       （辺の接触を許容する緩い基準で）内包しているなら、誤って親子関係を
       解除しない。新しい親候補が見つかった場合はここを通らないので、
       より前面のウィンドウへの正しい付け替えは妨げられない。 */
    if (newParent < 0 && IsStillInclusivelyContainedByCurrentParent(w->parentIdx, b))
        return;

    if (w->parentIdx >= 0)
        Hierarchy_Detach(idx);
    if (newParent >= 0)
        Hierarchy_Attach(newParent, idx);
}

void Goal_UpdateParent(void)
{
    int goalIdx = FindGoalIndex();
    if (goalIdx >= 0)
        UpdateSpecialChildParent(goalIdx);
}

void Button_UpdateParent(void)
{
    for (int i = 0; i < g_windowCount; i++)
        if (IsButtonWindowKind(g_windows[i].kind))
            UpdateSpecialChildParent(i);
}

COLORREF CalculateOutlineColor(COLORREF bg)
{
    int r = GetRValue(bg), g = GetGValue(bg), b = GetBValue(bg);
    float brightness = (r * 0.299f + g * 0.587f + b * 0.114f) / 255.0f;

    if (brightness < 0.5f)
    {
        int nr = r + 100, ng = g + 100, nb = b + 100;
        if (nr > 255) nr = 255;
        if (ng > 255) ng = 255;
        if (nb > 255) nb = 255;
        return RGB(nr, ng, nb);
    }
    else
    {
        int nr = r - 50, ng = g - 50, nb = b - 50;
        if (nr < 0) nr = 0;
        if (ng < 0) ng = 0;
        if (nb < 0) nb = 0;
        return RGB(nr, ng, nb);
    }
}

/* 時計回りに流れる縞模様ボーダーの1辺分を描画する。OutlineRenderer.DrawClockwiseSide
   と完全に一致させる: セグメントは固定のSTRIPE_WIDTH単位ではなく、位相の境界に
   揃えられる。そのため、色が切り替わる箇所や辺の終端では、セグメントが
   STRIPE_WIDTHより短くなることがある。`forward`は`pos`が辺を座標増加方向に
   進むか逆方向に進むかを制御し、これによって4辺が独立して描画されているにも
   関わらず、1つの連続した時計回りループとして繋がる。 */
static void DrawClockwiseSide(HDC hdc, RECT strip, int horizontal, int forward,
                               int periStart, int sideLength, int startPhase)
{
    int pos = 0;
    while (pos < sideLength)
    {
        int phase = (startPhase + pos) % (STRIPE_WIDTH * 2);
        int isRed = phase < STRIPE_WIDTH;

        int remainInPhase = isRed ? (STRIPE_WIDTH - phase) : (STRIPE_WIDTH * 2 - phase);
        int segLength = remainInPhase < (sideLength - pos) ? remainInPhase : (sideLength - pos);

        RECT seg;
        if (horizontal)
        {
            int drawX = forward ? (strip.left + pos) : (strip.right - pos - segLength);
            seg.left = drawX;
            seg.right = drawX + segLength;
            seg.top = strip.top;
            seg.bottom = strip.bottom;
        }
        else
        {
            int drawY = forward ? (strip.top + pos) : (strip.bottom - pos - segLength);
            seg.top = drawY;
            seg.bottom = drawY + segLength;
            seg.left = strip.left;
            seg.right = strip.right;
        }

        GdiBrush br(isRed ? RGB(255, 0, 0) : RGB(30, 30, 30));
        FillRect(hdc, &seg, br);

        pos += segLength;
    }
    (void)periStart;
}

static void DrawClockwiseStripeBorder(HDC hdc, RECT rc, float offset)
{
    /* OutlineRenderer.DrawNoEntryAnimatedBorderを正確に移植する: 同じ4辺の
       周囲展開（上:左->右、右:上->下、下:右->左、左:下->上）で、各辺の累積
       周長開始位置に単一の連続オフセットを「加算」する（減算ではない --
       この符号こそがループを時計回りに見せている）。 */
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    int bw = NOENTRY_BORDER_WIDTH;
    int totalPattern = STRIPE_WIDTH * 2;

    RECT top = {rc.left, rc.top, rc.right, rc.top + bw};
    RECT right = {rc.right - bw, rc.top, rc.right, rc.bottom};
    RECT bottom = {rc.left, rc.bottom - bw, rc.right, rc.bottom};
    RECT left = {rc.left, rc.top, rc.left + bw, rc.bottom};

    int offI = (int)offset;
    int phaseTop = ((0 + offI) % totalPattern + totalPattern) % totalPattern;
    int phaseRight = ((w + offI) % totalPattern + totalPattern) % totalPattern;
    int phaseBottom = ((w + h + offI) % totalPattern + totalPattern) % totalPattern;
    int phaseLeft = ((2 * w + h + offI) % totalPattern + totalPattern) % totalPattern;

    DrawClockwiseSide(hdc, top, 1, 1, 0, w, phaseTop);
    DrawClockwiseSide(hdc, right, 0, 1, w, h, phaseRight);
    DrawClockwiseSide(hdc, bottom, 1, 0, w + h, w, phaseBottom);
    DrawClockwiseSide(hdc, left, 0, 0, 2 * w + h, h, phaseLeft);
}

static void DrawMovableMark(HDC hdc, RECT rc, COLORREF color)
{
    int cx = (rc.left + rc.right) / 2;
    int cy = (rc.top + rc.bottom) / 2;
    int r = MARK_RADIUS;
    GdiPen pen(PS_SOLID, MARK_PEN_WIDTH, color);
    ScopedSelectObject selectPen(hdc, pen);

    MoveToEx(hdc, cx - r, cy, NULL);
    LineTo(hdc, cx + r, cy);
    MoveToEx(hdc, cx, cy - r, NULL);
    LineTo(hdc, cx, cy + r);

    int a = MARK_ARROW_LEN;
    MoveToEx(hdc, cx - r, cy, NULL);
    LineTo(hdc, cx - r + a, cy - a);
    MoveToEx(hdc, cx - r, cy, NULL);
    LineTo(hdc, cx - r + a, cy + a);
    MoveToEx(hdc, cx + r, cy, NULL);
    LineTo(hdc, cx + r - a, cy - a);
    MoveToEx(hdc, cx + r, cy, NULL);
    LineTo(hdc, cx + r - a, cy + a);
    MoveToEx(hdc, cx, cy - r, NULL);
    LineTo(hdc, cx - a, cy - r + a);
    MoveToEx(hdc, cx, cy - r, NULL);
    LineTo(hdc, cx + a, cy - r + a);
    MoveToEx(hdc, cx, cy + r, NULL);
    LineTo(hdc, cx - a, cy + r - a);
    MoveToEx(hdc, cx, cy + r, NULL);
    LineTo(hdc, cx + a, cy + r - a);
}

static void DrawResizableMark(HDC hdc, RECT rc, COLORREF color)
{
    int cx = (rc.left + rc.right) / 2;
    int cy = (rc.top + rc.bottom) / 2;
    int r = MARK_RADIUS;
    GdiPen pen(PS_SOLID, MARK_PEN_WIDTH, color);
    ScopedSelectObject selectPen(hdc, pen);

    MoveToEx(hdc, cx - r, cy - r, NULL);
    LineTo(hdc, cx + r, cy + r);
    int a = MARK_ARROW_LEN;
    MoveToEx(hdc, cx - r, cy - r, NULL);
    LineTo(hdc, cx - r + a, cy - r);
    MoveToEx(hdc, cx - r, cy - r, NULL);
    LineTo(hdc, cx - r, cy - r + a);
    MoveToEx(hdc, cx + r, cy + r, NULL);
    LineTo(hdc, cx + r - a, cy + r);
    MoveToEx(hdc, cx + r, cy + r, NULL);
    LineTo(hdc, cx + r, cy + r - a);
}

static void DrawMinimizableMark(HDC hdc, RECT rc, COLORREF color)
{
    /* MinimizableWindowStrategy.DrawStrategyMark: bounds中央にmarkSize=60の
       ボックス、その中央にbarHeight=markSize/6=10のバーを配置する。 */
    int cx = (rc.left + rc.right) / 2;
    int cy = (rc.top + rc.bottom) / 2;
    GdiBrush br(color);
    RECT bar = {cx - MINIMIZE_BAR_HALF_W, cy - MINIMIZE_BAR_HALF_H, cx + MINIMIZE_BAR_HALF_W, cy + MINIMIZE_BAR_HALF_H};
    FillRect(hdc, &bar, br);
}

static void DrawDeletableMark(HDC hdc, RECT rc, COLORREF color)
{
    int cx = (rc.left + rc.right) / 2;
    int cy = (rc.top + rc.bottom) / 2;
    int r = MARK_RADIUS;
    GdiPen pen(PS_SOLID, MARK_PEN_WIDTH, color);
    ScopedSelectObject selectPen(hdc, pen);

    MoveToEx(hdc, cx - r, cy - r, NULL);
    LineTo(hdc, cx + r, cy + r);
    MoveToEx(hdc, cx + r, cy - r, NULL);
    LineTo(hdc, cx - r, cy + r);
}

static void DrawGoalMark(HDC hdc, RECT rc)
{
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0)
        return;

    /* 以前はGoal_Paintと同じくbaseFontSize = min(w,h)*1.5の等方フォントで
       描いていた（「G」の見た目は常に正方形に近いまま）。これは実際の当たり
       判定(CheckGoalが見るGetWindowFullBounds、つまりrc全体)とは無関係に
       常に正方形寄りの見た目になるため、Goalが親のリサイズで縦横比の異なる
       形（横に潰れた/縦に伸びた矩形）になると、見た目のGはその中央に小さく
       留まったままなのに、実際に触れて反応する範囲はrc全体（Gの見た目より
       ずっと外側まで）という食い違いが生じ、プレイヤーが「Gに触れないと
       届かない」と誤認してしまっていた（実際に報告された不具合）。
       正方形の参照バッファに等方フォントで描いてからStretchBltでrc全体へ
       引き伸ばすことで、見た目のGの外接矩形が常にrc（実際の判定範囲）と
       正確に同じ縦横比になるようにする。 */
    const int REF = 128;
    ScopedCompatibleDC refDC(hdc);
    GdiHandle<HBITMAP> refBmp(CreateCompatibleBitmap(hdc, REF, REF));
    ScopedSelectObject selectRefBmp(refDC, refBmp);

    RECT refRc = {0, 0, REF, REF};
    {
        GdiBrush magentaBrush(RGB(255, 0, 255));
        FillRect(refDC, &refRc, magentaBrush);
    }

    SetBkMode(refDC, TRANSPARENT);
    {
        GdiHandle<HFONT> font(CreateFontA((int)(REF * 1.5f), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                           DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial"));
        ScopedSelectObject selectFont(refDC, font);
        SetTextColor(refDC, RGB(40, 40, 40));
        RECT shadow = refRc;
        OffsetRect(&shadow, 2, 2);
        DrawTextA(refDC, "G", -1, &shadow, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SetTextColor(refDC, RGB(255, 215, 0));
        DrawTextA(refDC, "G", -1, &refRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    SetStretchBltMode(hdc, HALFTONE);
    SetBrushOrgEx(hdc, rc.left, rc.top, NULL);
    StretchBlt(hdc, rc.left, rc.top, w, h, refDC, 0, 0, REF, REF, SRCCOPY);
}

/* kindに対応するストラテジーマーク（あれば）を描画する。PaintGameWindowの
   通常描画と、ステージエディターのパレットアイコン描画（「配置される実際の
   種別」の見た目を借りるだけで実際にはそのkindのウィンドウではない）の
   両方から呼べる共通ロジック。マークを持たない種別（Normal/TextDisplay等）
   は何も描かない。 */
static void DrawKindMark(HDC hdc, RECT rc, WindowKind kind, COLORREF markColor)
{
    if (kind == WT_MOVABLE || kind == WT_MOVABLE_NOENTRY)
        DrawMovableMark(hdc, rc, markColor);
    else if (kind == WT_RESIZABLE || kind == WT_RESIZABLE_NOENTRY ||
             kind == WT_UNCONSTRAINED || kind == WT_UNCONSTRAINED_NOENTRY)
        DrawResizableMark(hdc, rc, markColor);
    else if (kind == WT_MINIMIZABLE || kind == WT_MINIMIZABLE_NOENTRY)
        DrawMinimizableMark(hdc, rc, markColor);
    else if (kind == WT_DELETABLE)
        DrawDeletableMark(hdc, rc, markColor);
}

/* ゲーム描画のタイトルバー帯（クライアント領域最上部TITLE_BAR_HEIGHT px）を
   描画する。WS_CAPTIONを使わなくなったため、OS標準のタイトルバーの代わりに
   ここで自前描画する。デフォルトの配色は固定のダークグレー+白文字で、
   外観カスタマイズ（SetWindowAppearance）が設定されていればそちらを使う。 */
static void DrawTitleBar(HDC hdc, RECT rc, GameWindowData *data)
{
    RECT bar = {rc.left, rc.top, rc.right, rc.top + TITLE_BAR_HEIGHT};

    COLORREF barBg = data->hasCustomAppearance ? data->titleBarBg : RGB(45, 45, 48);
    COLORREF barFg = data->hasCustomAppearance ? data->titleBarFg : RGB(255, 255, 255);

    {
        GdiBrush brush(barBg);
        FillRect(hdc, &bar, brush);
    }

    const char *title = data->hasCustomAppearance && data->titleText[0] != '\0'
                             ? data->titleText
                             : "WindowAction";
    /* GameFont_Getはキャッシュ済みのHFONTを返す(所有権はgamefont.cpp側)ため、
       ここではSelectObjectの退避/復元だけ行い、DeleteObjectはしない。 */
    HFONT font = GameFont_Get(11);
    ScopedSelectObject selectFont(hdc, font);
    SetTextColor(hdc, barFg);
    SetBkMode(hdc, TRANSPARENT);
    RECT textRc = bar;
    textRc.left += 8;
    DrawTextA(hdc, title, -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static int IsWindowHovered(int index)
{
    GameWindowData *data = &g_windows[index];
    POINT cur;
    GetCursorPos(&cur);

    RECT wb;
    GetWindowFullBounds(data->hwnd, &wb);
    if (cur.x < wb.left || cur.x >= wb.right || cur.y < wb.top || cur.y >= wb.bottom)
        return 0;

    /* WindowRenderingManager.DrawWindowMark: この位置でカーソルをより前面の
       ウィンドウが覆っている場合、このウィンドウのマークはホバー中とは
       みなされない。 */
    int myZ = ZOrder_GetIndex(data->hwnd);
    for (int i = 0; i < g_windowCount; i++)
    {
        if (i == index || !g_windows[i].hwnd || g_windows[i].minimized)
            continue;
        if (ZOrder_GetIndex(g_windows[i].hwnd) <= myZ)
            continue;
        RECT ob;
        GetWindowFullBounds(g_windows[i].hwnd, &ob);
        if (cur.x >= ob.left && cur.x < ob.right && cur.y >= ob.top && cur.y < ob.bottom)
            return 0;
    }
    return 1;
}

static void PaintGameWindow(HWND hwnd, int index)
{
    GameWindowData *data = &g_windows[index];
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rc;
    GetClientRect(hwnd, &rc);

    ScopedCompatibleDC memDC(hdc);
    GdiHandle<HBITMAP> memBmp(CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top));
    ScopedSelectObject selectMemBmp(memDC, memBmp);

    /* GameButton.Button_PaintはGameWindowとは別に自身の背景を塗りつぶし、
       ホバー時に明るくする: ホバー時はFromArgb(230,230,230)、待機時は
       (200,200,200) -- data->bg（生成時に固定）だけではこれを表現できないため、
       ボタン種別の場合はここで分岐してdata->bgをそのまま使わないようにしている。 */
    COLORREF fillColor = data->bg;
#ifdef ENABLE_STAGE_EDITOR
    if (data->kind == WT_BTN_PALETTE)
    {
        /* パレットアイコンは灰色ボタンではなく、配置される実際の種別の背景色を
           そのまま表示する（一目で何を置けるか分かるようにする）。 */
        COLORREF pbg, pfg;
        int psolid, pIsNoEntry;
        GetKindAppearance(data->paletteKind, &pbg, &pfg, &psolid, &pIsNoEntry);
        fillColor = pbg;
    }
    else
#endif
    if (IsButtonWindowKind(data->kind))
        fillColor = IsWindowHovered(index) ? RGB(230, 230, 230) : RGB(200, 200, 200);

    {
        GdiBrush brush(fillColor);
        FillRect(memDC, &rc, brush);
    }

    int hasChrome = HasChrome(data->kind);
    if (hasChrome)
        DrawTitleBar(memDC, rc, data);

    /* タイトルバー帯を持つ種別は、マーク/ラベルをその下のクライアント領域
       だけに収める（帯と重ならないようにする）。 */
    RECT contentRc = rc;
    if (hasChrome)
        contentRc.top += TITLE_BAR_HEIGHT;

    switch (data->kind)
    {
    case WT_MOVABLE:
    case WT_RESIZABLE:
    case WT_MINIMIZABLE:
    case WT_DELETABLE:
    case WT_MOVABLE_NOENTRY:
    case WT_RESIZABLE_NOENTRY:
    case WT_MINIMIZABLE_NOENTRY:
    case WT_UNCONSTRAINED:
    case WT_UNCONSTRAINED_NOENTRY:
    {
        /* StrategyMarkUtility.GetMarkColor: ホバー中は白、それ以外は中間グレー。 */
        COLORREF markColor = IsWindowHovered(index) ? RGB(255, 255, 255) : RGB(128, 128, 128);
        DrawKindMark(memDC, contentRc, data->kind, markColor);
        break;
    }
    case WT_GOAL:
        DrawGoalMark(memDC, contentRc);
        break;
#ifdef ENABLE_STAGE_EDITOR
    case WT_BTN_PALETTE:
        /* パレットアイコンは「配置される実際の種別」のマークをそのまま表示する
           （灰色ボタンに文字ラベルだけ、ではなく一目で分かるようにする）。
           プレイヤー開始位置はpaletteKindを見た目の背景色を借りるためだけに
           使っており実際にはその種別のウィンドウを配置しないため、紛らわしい
           種別マーク（Movableの矢印等）は描かず、テキストラベル"Player"のみに
           する。 */
        if (!data->paletteIsPlayerStart)
            DrawKindMark(memDC, contentRc, data->paletteKind, RGB(255, 255, 255));
        if (data->paletteIsNoEntry)
            DrawClockwiseStripeBorder(memDC, rc, data->stripeOffset);
        break;
#endif
    default:
        break;
    }

    if (data->text[0] != '\0' && data->kind != WT_GOAL)
    {
        /* WindowFactoryのTextDisplay描画ハンドラは12ptの通常ウェイトでラベルを
           描画する。GameButton.DrawButtonContentは太字で描画する: Retryは12pt、
           Start/ToTitle/Exitは14pt -- サイズが2種類あり、太字はボタンのみで
           TextDisplayでは使わない。 */
        HFONT font;
        if (data->kind == WT_BTN_RETRY)
            font = GameFont_GetBold(12);
        else if (IsButtonWindowKind(data->kind))
            font = GameFont_GetBold(14);
        else
            font = GameFont_Get(12);
        ScopedSelectObject selectFont(memDC, font);
        SetTextColor(memDC, data->fg);
        SetBkMode(memDC, TRANSPARENT);
        DrawTextA(memDC, data->text, -1, &contentRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_WORDBREAK);
    }

    if (data->isNoEntry)
    {
        DrawClockwiseStripeBorder(memDC, rc, data->stripeOffset);
    }
    else
    {
        /* 親を持たないウィンドウはOSの通常ウィンドウに近い控えめな単一色・
           細線(DEFAULT_OUTLINE_COLOR/DEFAULT_OUTLINE_WIDTH_NO_PARENT)。
           親を持つウィンドウは、どれがどの子なのか一目で分かるように、
           親の背景色から算出した色(CalculateOutlineColor)ではっきり太く
           (PARENT_OUTLINE_WIDTH)描く -- こちらは単一色に統一しない。
           外観カスタマイズでoutlineColorが明示的に設定されている場合は
           色のみそちらを優先する。 */
        COLORREF outline;
        int outlineWidth;
        if (data->parentIdx >= 0)
        {
            outline = data->hasCustomAppearance ? data->outlineColor
                                                 : CalculateOutlineColor(g_windows[data->parentIdx].bg);
            outlineWidth = PARENT_OUTLINE_WIDTH;
        }
        else
        {
            outline = data->hasCustomAppearance ? data->outlineColor : DEFAULT_OUTLINE_COLOR;
            outlineWidth = DEFAULT_OUTLINE_WIDTH_NO_PARENT;
        }
        GdiPen pen(PS_SOLID, outlineWidth, outline);
        ScopedSelectObject selectOutlinePen(memDC, pen);
        ScopedSelectObject selectNullBrush(memDC, GetStockObject(NULL_BRUSH));
        Rectangle(memDC, rc.left, rc.top, rc.right, rc.bottom);
    }

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    /* 制限なしリサイズ+反転ウィンドウが負の論理サイズ（反転状態）にある場合、
       実HWNDは常に正サイズのままだが、描画内容だけをStretchBltの負幅/負高さ
       指定でミラーする。プレイヤーの衝突判定は実HWNDの矩形しか見ないため、
       これは純粋に見た目だけの効果。
       子/孫ウィンドウは自分自身が反転しているわけではないが、反転した
       祖先の内部に描かれている以上、見た目上はその祖先と一緒に鏡映される
       べき。ただしこれは「今その祖先の内部にいるか」をその場で判定する
       のではなく、inheritedFlipX/Yという積算済みの状態を使う -- 一度
       反転した見た目は、親から切り離されても、再度どこかの祖先が反転する
       まで元に戻らない（Hierarchy_ToggleInheritedFlip/UpdateUnconstrained
       参照）。 */
    int flipX, flipY;
    GameWindow_GetEffectiveFlip(data, &flipX, &flipY);
    if (flipX || flipY)
    {
        /* 負の幅/高さを指定するミラー手法のGDI特有の癖: 原点をwidth/height
           そのものにすると、境界の1列/1行がステップ丸めの都合で描画されずに
           残ることがある（PaintPlayerで黒い線として顕在化した不具合と同じ
           原因）。原点をwidth-1/height-1にすることでその境界も確実に
           上書きされる。 */
        StretchBlt(hdc, flipX ? w - 1 : 0, flipY ? h - 1 : 0, flipX ? -w : w, flipY ? -h : h,
                   memDC, 0, 0, w, h, SRCCOPY);
    }
    else
    {
        BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
    }

    EndPaint(hwnd, &ps);
}

/* CaptureIconicBitmap/CopyBitmapScaled共通: DWMのDwmSetIconicThumbnail/
   DwmSetIconicLivePreviewBitmapに渡すビットマップは、32bpp・トップダウンの
   DIBセクションである必要がある（MSDNのサンプルもこの形式を使う）。
   CreateCompatibleBitmapで作った通常のDDBを渡すと、DWM側で受理されず
   タスクバーのサムネイルが「更新中」のまま止まってしまうことがある
   （実際に報告された不具合）。 */
/* outBitsが非NULLなら、生成したDIBセクションのピクセルバッファ先頭を
   書き出す -- CaptureIconicBitmapがGoalのマゼンタ画素にアルファ0を
   焼き込むために直接触る必要がある。 */
static HBITMAP CreateArgbDibSection(HDC referenceDC, int w, int h, void **outBits)
{
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; /* トップダウン(上から下)で格納する */
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void *bits = NULL;
    HBITMAP bmp = CreateDIBSection(referenceDC, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (outBits)
        *outBits = bits;
    return bmp;
}

/* 縮小アニメーションを開始する直前に一度だけ呼ぶ。ウィンドウが「今まさに
   描画している」フルサイズの見た目をそのままビットマップへコピーしておく
   -- これが無いと、アニメーションで数px四方まで縮んだ後の内容がDWMの
   タスクバーサムネイル/ライブプレビューになってしまう（実際に報告された
   不具合）。GetDC+BitBlt方式を使う理由: PrintWindow/WM_PRINTはWM_PAINTのみで
   自前描画するウィンドウでは何も描画されないことがある既知の癖があるが、
   GetDCで取得した実際の画面上のDCから直接コピーすれば、今まさに表示されて
   いる内容をそのまま確実に取得できる。 */
static void CaptureIconicBitmap(GameWindowData *data)
{
    if (data->iconicBitmap)
    {
        DeleteObject(data->iconicBitmap);
        data->iconicBitmap = NULL;
    }

    RECT rc;
    GetClientRect(data->hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0)
        return;

    ScopedWindowDC hdcWin(data->hwnd);
    ScopedCompatibleDC hdcMem(hdcWin);
    void *bits = NULL;
    HBITMAP bmp = CreateArgbDibSection(hdcWin, w, h, &bits); /* 所有権はdata->iconicBitmapへ移るためRAII化しない */
    {
        ScopedSelectObject selectBmp(hdcMem, bmp);
        BitBlt(hdcMem, 0, 0, w, h, hdcWin, 0, 0, SRCCOPY);
    }

    /* GoalはSetLayeredWindowAttributes(..., RGB(255,0,255), LWA_COLORKEY)で
       マゼンタ画素を透過させているが、これは実際の画面合成時にDWMが行う
       効果であり、GetDC+BitBltで取得した生のピクセルデータ自体には反映
       されない -- そのままDwmSetIconicThumbnail/LivePreviewへ渡すと、
       タスクバーのサムネイル上ではマゼンタが不透明な色としてそのまま
       写り込んでしまう（実際に報告された不具合）。GDIのBitBltはアルファ
       チャンネルを一切書き込まないため、この時点では全画素のアルファが
       0のまま -- DWMは「全画素アルファ0」を特殊ケースとして不透明画像
       扱いするため、通常のウィンドウ（マゼンタを使わない）はこれで
       たまたま正しく表示されている。Goalだけはマゼンタ画素にアルファ0
       （透明）、それ以外の画素に明示的にアルファ255（不透明）を焼き込み、
       DWMに実際のライブ表示と同じ透過を伝える。 */
    if (data->kind == WT_GOAL && bits)
    {
        UINT32 *px = (UINT32 *)bits;
        int count = w * h;
        for (int i = 0; i < count; i++)
        {
            UINT32 c = px[i];
            int isMagenta = ((c & 0x00FFFFFFu) == 0x00FF00FFu);
            px[i] = isMagenta ? 0x00000000u : (0xFF000000u | (c & 0x00FFFFFFu));
        }
    }

    data->iconicBitmap = bmp;
}

/* CaptureIconicBitmap/RespondIconicThumbnail/RespondIconicLivePreview共通:
   キャプチャ済みビットマップを指定サイズへコピーした複製を作る。
   DwmSetIconicThumbnail/DwmSetIconicLivePreviewBitmapへ渡すビットマップの
   所有権はDWM側に移り、DWMが破棄する（MSDN仕様）ため、キャッシュ済みの
   dataIconicBitmap自身を直接渡してはならず、呼び出しのたびに複製する。 */
static HBITMAP CopyBitmapScaled(HBITMAP src, int srcW, int srcH, int dstW, int dstH)
{
    HBITMAP dstBmp;
    {
        ScopedWindowDC screenDC(NULL);
        ScopedCompatibleDC srcDC(screenDC);
        ScopedCompatibleDC dstDC(screenDC);
        dstBmp = CreateArgbDibSection(screenDC, dstW, dstH, NULL); /* 所有権は呼び出し元(DWM)へ移るためRAII化しない */

        ScopedSelectObject selectSrc(srcDC, src);
        ScopedSelectObject selectDst(dstDC, dstBmp);
        if (dstW == srcW && dstH == srcH)
        {
            BitBlt(dstDC, 0, 0, dstW, dstH, srcDC, 0, 0, SRCCOPY);
        }
        else
        {
            SetStretchBltMode(dstDC, HALFTONE);
            StretchBlt(dstDC, 0, 0, dstW, dstH, srcDC, 0, 0, srcW, srcH, SRCCOPY);
        }
    }
    return dstBmp;
}

/* WM_DWMSENDICONICTHUMBNAIL応答: タスクバーボタンにマウスを乗せた時の
   小さなサムネイル。lParamで要求された箱に収まるよう、アスペクト比を
   保ったまま縮小する。 */
static void RespondIconicThumbnail(GameWindowData *data, int reqW, int reqH)
{
    if (!data->iconicBitmap || reqW <= 0 || reqH <= 0)
        return;

    BITMAP bmInfo;
    if (!GetObject(data->iconicBitmap, sizeof(bmInfo), &bmInfo) ||
        bmInfo.bmWidth <= 0 || bmInfo.bmHeight <= 0)
        return;

    float scaleW = (float)reqW / (float)bmInfo.bmWidth;
    float scaleH = (float)reqH / (float)bmInfo.bmHeight;
    float scale = (scaleW < scaleH) ? scaleW : scaleH;
    int dstW = (int)(bmInfo.bmWidth * scale);
    int dstH = (int)(bmInfo.bmHeight * scale);
    if (dstW < 1)
        dstW = 1;
    if (dstH < 1)
        dstH = 1;

    HBITMAP scaled = CopyBitmapScaled(data->iconicBitmap, bmInfo.bmWidth, bmInfo.bmHeight, dstW, dstH);
    DwmSetIconicThumbnail(data->hwnd, scaled, 0);
}

/* WM_DWMSENDICONICLIVEPREVIEWBITMAP応答: タスクバーボタンをクリックした
   時のAero Peekの大きなプレビュー。原寸大のキャプチャをそのまま渡す。 */
static void RespondIconicLivePreview(GameWindowData *data)
{
    if (!data->iconicBitmap)
        return;

    BITMAP bmInfo;
    if (!GetObject(data->iconicBitmap, sizeof(bmInfo), &bmInfo) ||
        bmInfo.bmWidth <= 0 || bmInfo.bmHeight <= 0)
        return;

    HBITMAP copy = CopyBitmapScaled(data->iconicBitmap, bmInfo.bmWidth, bmInfo.bmHeight, bmInfo.bmWidth, bmInfo.bmHeight);
    DwmSetIconicLivePreviewBitmap(data->hwnd, copy, NULL, 0);
}

static LRESULT CALLBACK GameWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int index = FindWindowIndex(hwnd);

    switch (msg)
    {
    /* WM_NCHITTESTの特別扱いは不要になった: 全ゲームウィンドウがWS_POPUP
       （非クライアント領域を持たない）になったため、DefWindowProcAは常に
       HTCLIENTを返す。移動/リサイズは元々OSの非クライアントドラッグではなく
       独自のマウスポーリング（Strategy_UpdateAll）で行っている。 */
    case WM_LBUTTONDOWN:
        if (index >= 0)
        {
#ifdef ENABLE_STAGE_EDITOR
            /* パレットアイコンはクリック即選択ではなく、ドラッグ&ドロップで
               配置する（Editor_UpdatePaletteDragsが毎フレーム追従させ、
               WM_LBUTTONUPで実際の配置とホームポジションへの復帰を行う）。
               固定UIなのでZ-order操作自体も不要。 */
            if (g_windows[index].kind == WT_BTN_PALETTE)
            {
                Editor_StartPaletteDrag(index);
                return 0;
            }
#endif
            ZOrder_BringToFront(hwnd);
            if (IsButtonWindowKind(g_windows[index].kind))
                Strategy_HandleButtonClick(g_windows[index].kind);
            else
                Strategy_HandleMouseDown(index);
        }
        return 0;
    case WM_LBUTTONUP:
        if (index >= 0)
        {
#ifdef ENABLE_STAGE_EDITOR
            if (g_windows[index].kind == WT_BTN_PALETTE)
            {
                Editor_EndPaletteDrag(index);
                return 0;
            }
#endif
            Strategy_HandleMouseUp(index);
        }
        return 0;
    case WM_PAINT:
        if (index >= 0)
            PaintGameWindow(hwnd, index);
        else
        {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
        }
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_DWMSENDICONICTHUMBNAIL:
        if (index >= 0)
            RespondIconicThumbnail(&g_windows[index], LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_DWMSENDICONICLIVEPREVIEWBITMAP:
        if (index >= 0)
            RespondIconicLivePreview(&g_windows[index]);
        return 0;
    case WM_CLOSE:
        return 0;
    case WM_DESTROY:
        return 0;
    case WM_ACTIVATE:
    {
        /* WindowMessageHandler.HandleWindowMessageはGameWindow専用の分岐として
           WM_ACTIVATE（wParam != 0、つまりアクティブ化される側）を特別扱いする:
           HandleLeftButtonDown(window)でBringWindowToFrontを呼びZ-orderを
           最前面にした上で、CheckPotentialParentWindowをBeginInvokeで遅延実行
           して親子判定もやり直す。マウスクリックでのアクティブ化はWM_LBUTTONDOWN/
           WM_LBUTTONUPで既にBringToFront+CheckAndUpdateされるが、タスクバーの
           アイコンから選択した場合はマウスメッセージが一切飛んでこず
           WM_ACTIVATEだけが届くため、ここで処理しないとZ-order（当たり判定に
           使うリスト）も親子関係も更新されないまま、視覚上だけ最前面になる
           （ユーザー報告のバグそのもの）。この分岐はSuccessを返しbase.WndProc
           を呼ばない（=DefWindowProcAへは渡さない）ため、非アクティブ化側
           （wParam==0）だけ下のbreakでDefWindowProcAに流す。
           Goal/ボタンはGameWindow相当ではなくこの分岐の対象外
           （IsQueryableWindowで除外）。BeginInvokeに相当する遅延実行の仕組みは
           ここには無いため、SC_RESTORE時のHierarchy_CheckAndUpdate呼び出しと
           同様に同期的に呼び出す。 */
        if (index >= 0 && (int)wParam != 0 && IsQueryableWindow(g_windows[index].kind))
        {
            ZOrder_BringToFront(hwnd);
            Hierarchy_CheckAndUpdate(index);
            return 0;
        }
        break;
    }
    case WM_SYSCOMMAND:
    {
        /* WS_SYSMENUを外したのでシステムメニュー/タイトルバーのダブルクリック
           経由でこのメッセージが飛んでくることはなくなったが、タスクバーの
           アイコンをクリックしての最小化/復元はWS_SYSMENUの有無に関わらず
           シェルがWM_SYSCOMMAND(SC_MINIMIZE/SC_RESTORE)を送ってくる --
           これを処理しないと、最小化したウィンドウをタスクバー経由で
           復元する手段が失われる（一度削除して発生した回帰）。
           SetWindowMinimizedを経由させることで、トリガー元（クリック/
           タスクバー）に関わらずdata->minimized状態と縮小/拡大アニメーション
           が一貫する。DefWindowProcAには渡さない（渡すとOS自身の
           無アニメーションな即時最小化/復元が並行して起きてしまう）。 */
        int command = (int)(wParam & 0xFFF0);
        if (index >= 0)
        {
            int isMinimizableKind = (g_windows[index].kind == WT_MINIMIZABLE ||
                                      g_windows[index].kind == WT_MINIMIZABLE_NOENTRY);
            if (command == SC_MINIMIZE && isMinimizableKind && !g_windows[index].minimized)
                SetWindowMinimized(index, 1);
            else if (command == SC_RESTORE && g_windows[index].minimized)
                SetWindowMinimized(index, 0);
            return 0;
        }
        break;
    }
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

void RegisterGameWindowClass(HINSTANCE hInstance)
{
    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = GameWindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = kGameWindowClass;
    wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.hbrBackground = NULL;
    RegisterClassA(&wc);
}

void ResetWindowRegistry(void)
{
    for (int i = 0; i < g_windowCount; i++)
    {
        if (g_windows[i].hwnd)
            DestroyWindow(g_windows[i].hwnd);
    }
    g_windows.Clear();
    NoEntry_ResetZones();
    ZOrder_Reset();
}

int CreateGameWindowIndexed(HINSTANCE hInstance, WindowKind kind, int x, int y, int w, int h, const char *text)
{
    if (g_windowCount >= MAX_WINDOWS)
        return -1;

    COLORREF bg, fg;
    int solid, isNoEntry;
    GetKindAppearance(kind, &bg, &fg, &solid, &isNoEntry);

    DWORD style, exStyle;
    if (kind == WT_GOAL)
    {
        style = WS_POPUP | WS_VISIBLE;
        exStyle = WS_EX_LAYERED;
    }
    else if (IsButtonWindowKind(kind))
    {
        /* GameButton.InitializeButtonはFormBorderStyle.Noneを設定する -- ゴールと
           同様に枠なし。ここでWS_BORDERを付けると、オリジナルには枠が無いのに
           GetWindowFullBoundsのクライアント/外枠の分離がボタンにも適用されて
           しまう。
           WS_EX_TOOLWINDOW: 所有者を持たないトップレベルウィンドウはデフォルトで
           タスクバーボタンを持ってしまう。ステージエディターのパレットアイコン
           (WT_BTN_PALETTE、種別数分)やツールバーボタンは数が多く、これらすべてが
           タスクバーに並ぶと非常に見づらい（実際に報告された不具合）。ボタンは
           元々OS標準の見た目・タスクバー統合を必要としないゲーム内UI要素なので、
           ここで一律にタスクバーから除外する。 */
        style = WS_POPUP | WS_VISIBLE;
        exStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
#ifdef ENABLE_STAGE_EDITOR
        /* パレットアイコンだけはWS_EX_LAYEREDも付けておく -- ドラッグ中に
           SetLayeredWindowAttributes(..., LWA_ALPHA)で半透明化するため。
           待機中はアルファ255（不透明）で通常のウィンドウと見た目は変わらない。 */
        if (kind == WT_BTN_PALETTE)
            exStyle |= WS_EX_LAYERED;
#endif
    }
    else
    {
        /* ゲーム描画のカスタムタイトルバー(DrawTitleBar)を使うため、OS標準の
           WS_CAPTION|WS_SYSMENU|WS_BORDERは付けない -- クライアント領域が
           そのままウィンドウ全体になる。移動/リサイズは元々WM_NCHITTESTで
           OS標準ドラッグを無効化し独自ポーリングで行っていたため、この
           変更によるマウス操作ロジックへの影響はない。 */
        style = WS_POPUP | WS_VISIBLE;
        exStyle = 0;
    }

    HWND hwnd = CreateWindowExA(exStyle, kGameWindowClass, "WindowAction",
                                 style, x, y, w, h, NULL, NULL, hInstance, NULL);
    if (!hwnd)
        return -1;

    if (kind == WT_GOAL)
    {
        SetLayeredWindowAttributes(hwnd, RGB(255, 0, 255), 0, LWA_COLORKEY);
    }
#ifdef ENABLE_STAGE_EDITOR
    else if (kind == WT_BTN_PALETTE)
    {
        /* WS_EX_LAYEREDウィンドウは一度もSetLayeredWindowAttributesを呼ばないと
           正しく描画されないことがあるため、待機時の不透明状態を明示しておく。 */
        SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
    }
#endif
    /* WS_SYSMENUを付けなくなったため、システムメニュー経由のSC_CLOSE無効化は
       不要（閉じるボタン自体がOS側に存在しない）。 */

    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    /* Windowsのフォアグラウンドロック制限により、他のフォアグラウンドプロセス
       （エディタやターミナル等）から起動された直後のこのプロセスは、最初に
       表示する1枚目のウィンドウについてはSetWindowPos(HWND_TOPMOST)（および
       SetForegroundWindow）を実際には拒否されることがある -- 何度単純に
       再試行しても直らない（実測確認済み）。2枚目以降のウィンドウは同じ呼び出しで
       即座に成功するため、影響を受けるのはステージ最初の1枚だけだが、それが
       常に「一番奥に固定される」ように見える実際のバグの原因になっていた。
       現在のフォアグラウンドウィンドウのスレッドと自スレッドの入力キューを
       一時的に結合(AttachThreadInput)すると、この制限を正規に回避できる
       （多くのWindowsアプリが採用する標準的な手法）。 */
    if (!(GetWindowLongPtrA(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST))
    {
        HWND fg = GetForegroundWindow();
        DWORD curThread = GetCurrentThreadId();
        DWORD fgThread = fg ? GetWindowThreadProcessId(fg, NULL) : 0;
        int attached = (fgThread != 0 && fgThread != curThread) ? AttachThreadInput(curThread, fgThread, TRUE) : 0;

        SetForegroundWindow(hwnd);
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

        if (attached)
            AttachThreadInput(curThread, fgThread, FALSE);

        /* それでも駄目な場合の最終手段: 一旦NOTOPMOSTにしてからTOPMOSTへ
           付け直すと、ごく稀にHWND_TOPMOST単発が反映されないケースで通ることがある。 */
        if (!(GetWindowLongPtrA(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST))
        {
            SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }

    int index = g_windowCount; /* Add()がg_windowCountをindex+1へ更新する前の値 */
    GameWindowData *data = g_windows.Add(); /* std::make_uniqueの値初期化により既にゼロ初期化済み */
    data->hwnd = hwnd;
    data->kind = kind;
    data->bg = bg;
    data->fg = fg;
    data->solid = solid;
    data->isNoEntry = isNoEntry;
    data->parentIdx = -1;
    data->childCount = 0;
    data->logicalW = w;
    data->logicalH = h;
    if (text)
    {
        int i = 0;
        while (text[i] != '\0' && i < 63)
        {
            data->text[i] = text[i];
            i++;
        }
        data->text[i] = '\0';
    }

    ZOrder_Register(hwnd);
    return index;
}

HWND CreateGameWindow(HINSTANCE hInstance, WindowKind kind, int x, int y, int w, int h, const char *text)
{
    int idx = CreateGameWindowIndexed(hInstance, kind, x, y, w, h, text);
    return idx >= 0 ? g_windows[idx].hwnd : NULL;
}

void GetWindowFullBounds(HWND hwnd, RECT *out)
{
    /* GameWindow.CollisionBoundsと正確に一致させる -- このプロパティは単なる
       GetWindowRectではない: Left/Right/Bottomはスクリーン座標系のCLIENT矩形
       から来る（ウィンドウ枠のピクセルは含まない）が、Topだけはウィンドウの
       外枠位置（タイトルバーを含む）から来る:
         CollisionBounds => new(AdjustedBounds.X, Location.Y,
             AdjustedBounds.Width, AdjustedBounds.Height + titleBarHeight)
       これは{Left=ClientLeft, Top=OuterTop, Right=ClientRight,
       Bottom=ClientBottom}に相当する。素のGetWindowRectは四辺全てに枠を
       含むため、それを直接使う床/天井/壁/移動可能領域のチェックは全て、
       左/右/下端で枠幅の分だけズレていた -- 枠の無いWS_POPUPウィンドウ
       （ゴール、ボタン）ではこれは無意味（クライアント==外枠が既に成立）だが、
       枠やタイトルバーを持つGameWindow種別では、プレイヤーの衝突ボックスが
       乗っている対象の本来のクライアントベースの端より数ピクセル先に
       はみ出してしまっていた。 */
    RECT outer;
    GetWindowRect(hwnd, &outer);

    RECT client;
    GetClientRect(hwnd, &client);
    POINT tl = {0, 0};
    POINT br = {client.right, client.bottom};
    ClientToScreen(hwnd, &tl);
    ClientToScreen(hwnd, &br);

    out->left = tl.x;
    out->top = outer.top;
    out->right = br.x;
    out->bottom = br.y;
}

void SetWindowAppearance(int index, COLORREF titleBarBg, COLORREF titleBarFg,
                          COLORREF outlineColor, const char *titleText)
{
    GameWindowData *data = GetWindowData(index);
    if (!data || !data->hwnd)
        return;

    data->hasCustomAppearance = 1;
    data->titleBarBg = titleBarBg;
    data->titleBarFg = titleBarFg;
    data->outlineColor = outlineColor;
    data->titleText[0] = '\0';
    if (titleText)
    {
        int i = 0;
        while (titleText[i] != '\0' && i < 63)
        {
            data->titleText[i] = titleText[i];
            i++;
        }
        data->titleText[i] = '\0';
    }
    InvalidateRect(data->hwnd, NULL, FALSE);
}

/* [cx,cy]を中心とするMINIMIZE_ANIM_POINT_SIZE四方の小さな矩形を返す
   （0x0だとSetWindowPos/GDIが扱いにくいため）。 */
static RECT MinimizeAnimPointRect(int cx, int cy)
{
    int h = MINIMIZE_ANIM_POINT_SIZE / 2;
    RECT r = {cx - h, cy - h, cx + h, cy + h};
    return r;
}

/* 最小化の縮小アニメーションを1つのウィンドウに対して開始する（見た目・
   DWMサムネイル準備の共通処理）。SetWindowMinimized自身と、その子孫を
   まとめて動かすStartMinimizeAnimSubtreeの両方から呼ぶ。 */
static void StartMinimizeAnim(GameWindowData *data)
{
    if (!data->hwnd || data->minimized || data->minimizeAnimState != 0)
        return;

    /* 縮小アニメーションで実際にウィンドウを小さくする前に、フルサイズの
       見た目を1回だけキャプチャしてDWMへ渡す準備をする（RespondIconic*
       参照）。DWMWA_FORCE_ICONIC_REPRESENTATIONを立てることで、以後
       ウィンドウが最小化されている間はDWMが自動キャプチャを使わず
       必ずWM_DWMSENDICONICTHUMBNAIL/WM_DWMSENDICONICLIVEPREVIEWBITMAPで
       問い合わせてくるようになり、数px四方まで縮んだ後の内容が
       タスクバーサムネイルに映ってしまう不具合を防げる。 */
    CaptureIconicBitmap(data);
    BOOL trueVal = TRUE;
    DwmSetWindowAttribute(data->hwnd, DWMWA_HAS_ICONIC_BITMAP, &trueVal, sizeof(trueVal));
    DwmSetWindowAttribute(data->hwnd, DWMWA_FORCE_ICONIC_REPRESENTATION, &trueVal, sizeof(trueVal));
    /* 属性を立てただけではDWMが即座に問い合わせてくるとは限らない
       （「更新中」のプレースホルダのまま止まって見えることがあった --
       実際に報告された不具合）。明示的にDwmInvalidateIconicBitmapsを
       呼び、WM_DWMSENDICONICTHUMBNAIL/WM_DWMSENDICONICLIVEPREVIEWBITMAPを
       今すぐ問い合わせさせる。 */
    DwmInvalidateIconicBitmaps(data->hwnd);

    GetWindowRect(data->hwnd, &data->minimizeAnimFrom);
    int cx = (data->minimizeAnimFrom.left + data->minimizeAnimFrom.right) / 2;
    /* 収縮先をウィンドウ自身の下端ではなく画面（タスクバー）の下端にする
       -- 自身の下端だと、画面の上の方にあるウィンドウほど本来のタスク
       バーの位置とかけ離れた高さで収縮してしまい、見た目上左上寄りへ
       消えていくように見えてしまう（実際に報告された不具合）。実際の
       Windowsの最小化ジーニーと同じく、常に画面下端(タスクバー方向)へ
       向かって縮んでいくようにする。 */
    int screenBottom = GetSystemMetrics(SM_CYSCREEN);
    data->minimizeAnimTo = MinimizeAnimPointRect(cx, screenBottom);
    data->minimizeAnimT = 0.0f;
    data->minimizeAnimState = 1;
}

/* rootIndexの全子孫（ボタンを除く。ボタンは元々サイズ変更が無く、最小化時も
   Hierarchy_MinimizeSubtreeが即座に非表示にするだけで違和感が無い）にも、
   同じ縮小アニメーションを開始する。ルート自身がめり込んで消えるのに、
   その中にいる子孫だけ何の演出も無く一瞬で消えてしまう（実際に報告された
   不具合）のを防ぐ。SetWindowMinimizedがrootIndex自身の分を開始した直後に
   呼ぶこと。 */
static void StartMinimizeAnimSubtree(int rootIndex)
{
    GameWindowData *root = GetWindowData(rootIndex);
    if (!root)
        return;
    for (int i = 0; i < root->childCount; i++)
    {
        GameWindowData *child = GetWindowData(root->childIdx[i]);
        if (!child || IsButtonWindowKind(child->kind))
            continue;
        StartMinimizeAnim(child);
        StartMinimizeAnimSubtree(root->childIdx[i]);
    }
}

void SetWindowMinimized(int index, int minimized)
{
    /* GameWindow.OnMinimize/OnRestoreは非対称: 最小化はサブツリー全体
       （その中のどこに親子付けされていようとプレイヤーやゴールも含む）を
       再帰的に解体し個別に最小化するが、復元は復元対象の1つのウィンドウ
       にしか作用しない。Hierarchy_MinimizeSubtreeとHierarchy_RestoreWindow
       を参照。

       WS_CAPTIONを外したことでOS標準の最小化ジーニーアニメーションが
       使えなくなったため、ここで自前の縮小/拡大アニメーションを開始する。
       実際の階層解体/OS最小化（Hierarchy_MinimizeSubtree）はアニメーション
       完了時にMinimizeAnim_UpdateAllから呼ばれる -- こうすることで、
       Windowsが「復元先」として記憶するWINDOWPLACEMENTには、縮小後ではなく
       常に正しいフルサイズが渡る。復元側はHierarchy_RestoreWindowを先に
       呼んでOSに正しいフルサイズへ戻させてから、そこから一旦縮めて
       アニメーションで元のサイズへ戻す（見た目の拡大効果のみ）。 */
    GameWindowData *data = GetWindowData(index);
    if (!data || !data->hwnd)
        return;

    if (minimized)
    {
        if (data->minimized || data->minimizeAnimState != 0)
            return;

        StartMinimizeAnim(data);
        /* ルート自身だけでなく、その子孫（ウィンドウ内に乗っている他の
           ウィンドウ）も同じ縮小アニメーションで一緒に消えていくように
           する（実際に報告された不具合: 子孫が縮小せず一瞬で消えていた）。 */
        StartMinimizeAnimSubtree(index);

        /* プレイヤーがこのサブツリー内にいる場合、Player_StartMinimizeAnimで
           プレイヤー自身も同じ縮小アニメーションを開始する。物理演算は
           アニメーション完了(Hierarchy_MinimizeSubtree経由でPlayer_OnMinimize
           が呼ばれるタイミング)を待たず、この開始と同時に凍結される
           （そうしないと、実際に消えるまでの数百msの間も重力/接地判定が
           働き続け、縮小中で不安定な床の上から落下してしまっていた --
           実際に報告された不具合）。parentIdx/hwndの後始末（Player_OnMinimize
           が行う）はアニメーション完了時のまま据え置かれる。 */
        Player *p = Player_GetActive();
        if (p && p->parentIdx >= 0 &&
            (p->parentIdx == index || Hierarchy_IsDescendantOf(p->parentIdx, index)))
        {
            Player_StartMinimizeAnim(p);
        }
    }
    else
    {
        if (!data->minimized || data->minimizeAnimState != 0)
            return;
        Hierarchy_RestoreWindow(index);

        /* 通常の描画に戻すため、強制アイコン表現を解除しキャプチャした
           ビットマップを解放する（次に最小化される時にCaptureIconicBitmapで
           改めて撮り直す）。 */
        BOOL falseVal = FALSE;
        DwmSetWindowAttribute(data->hwnd, DWMWA_FORCE_ICONIC_REPRESENTATION, &falseVal, sizeof(falseVal));
        if (data->iconicBitmap)
        {
            DeleteObject(data->iconicBitmap);
            data->iconicBitmap = NULL;
        }

        RECT full;
        GetWindowRect(data->hwnd, &full);
        int cx = (full.left + full.right) / 2;
        /* 収縮先を画面下端にした変更と対にする: 復元アニメーションも同じ
           画面下端（タスクバー方向）から元のサイズへ向かって広がってくる
           ように見せる。 */
        int screenBottom = GetSystemMetrics(SM_CYSCREEN);
        data->minimizeAnimFrom = MinimizeAnimPointRect(cx, screenBottom);
        data->minimizeAnimTo = full;
        SetWindowPos(data->hwnd, NULL, data->minimizeAnimFrom.left, data->minimizeAnimFrom.top,
                     data->minimizeAnimFrom.right - data->minimizeAnimFrom.left,
                     data->minimizeAnimFrom.bottom - data->minimizeAnimFrom.top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        data->minimizeAnimT = 0.0f;
        data->minimizeAnimState = 2;
    }
}

static int LerpInt(int a, int b, float t)
{
    return a + (int)((b - a) * t);
}

void MinimizeAnim_UpdateAll(float dt)
{
    for (int i = 0; i < g_windowCount; i++)
    {
        GameWindowData *data = &g_windows[i];
        if (data->minimizeAnimState == 0 || !data->hwnd)
            continue;

        data->minimizeAnimT += dt / MINIMIZE_ANIM_DURATION;
        float t = data->minimizeAnimT;
        if (t > 1.0f)
            t = 1.0f;

        RECT from = data->minimizeAnimFrom;
        RECT to = data->minimizeAnimTo;
        int x = LerpInt(from.left, to.left, t);
        int y = LerpInt(from.top, to.top, t);
        int w = LerpInt(from.right - from.left, to.right - to.left, t);
        int h = LerpInt(from.bottom - from.top, to.bottom - to.top, t);
        SetWindowPos(data->hwnd, NULL, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(data->hwnd, NULL, FALSE);
        /* InvalidateRectだけでは再描画が次のメッセージループパスまで遅延され、
           特に復元(拡大)アニメーション中は、その1フレーム分の隙間で新しく
           露出した領域が未初期化の黒いままになって見えてしまう（実際に
           報告された不具合: 復元時に黒い部分が映る）。他の拡大処理
           （Hierarchy_ApplyRelativeTransform等）と同じくUpdateWindowで
           同期的に再描画を強制する。 */
        UpdateWindow(data->hwnd);

        if (data->minimizeAnimT >= 1.0f)
        {
            int wasMinimizing = (data->minimizeAnimState == 1);
            data->minimizeAnimState = 0;
            if (wasMinimizing)
            {
                /* 縮小アニメーションで動かした分を元のフルサイズへ戻してから
                   実際にOS最小化する（上の関数コメント参照）。SWP_NOREDRAWは
                   GDIレベルの再描画要求を抑えるだけで、DWM側がウィンドウの
                   実サーフェス自体を新しい位置・サイズへ即座に移動/伸縮する
                   ことまでは止められず、それだけでは「タスクバー方向へほぼ
                   縮みきった状態から、元のフルサイズ・元の位置へ一瞬だけ戻る」
                   瞬間が依然として見えてしまっていた（実際にWin32 API計測で
                   確認・報告された不具合）。確実に見せないようにするため、
                   フルサイズへ戻す前に一旦SW_HIDEで非表示にしてから位置・
                   サイズを更新する -- 非表示の状態で動かせば画面には一切
                   反映されず、直後のShowWindow(SW_MINIMIZE)（非表示→最小化は
                   どちらも「見えない」状態同士の遷移）でそのままタスクバーへ
                   収まる。 */
                ShowWindow(data->hwnd, SW_HIDE);
                RECT full = data->minimizeAnimFrom;
                SetWindowPos(data->hwnd, NULL, full.left, full.top,
                             full.right - full.left, full.bottom - full.top,
                             SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
                Hierarchy_MinimizeSubtree(i);
            }
        }
    }
}

void DeleteWindow(int index)
{
    GameWindowData *data = GetWindowData(index);
    if (!data || !data->hwnd)
        return;

    /* RemoveAndClose: 子要素は切り離される（親を失った状態で生き残る）だけで、
       親と一緒に破棄されることはない。 */
    for (int i = 0; i < data->childCount; i++)
    {
        GameWindowData *child = GetWindowData(data->childIdx[i]);
        if (child)
            child->parentIdx = -1;
    }
    data->childCount = 0;

    Hierarchy_Detach(index);

    Player *p = Player_GetActive();
    if (p && p->parentIdx == index)
        p->parentIdx = -1;

    HWND hwnd = data->hwnd;
    data->hwnd = NULL;
    ZOrder_Unregister(hwnd);
    DestroyWindow(hwnd);

    /* 最小化中に削除された場合、CaptureIconicBitmapで確保したGDIビットマップが
       残ったままになるのを防ぐ。 */
    if (data->iconicBitmap)
    {
        DeleteObject(data->iconicBitmap);
        data->iconicBitmap = NULL;
    }
}
