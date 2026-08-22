#include "gamewindow.h"
#include "zorder.h"
#include "gamefont.h"
#include "hierarchy.h"
#include "player.h"
#include "windowquery.h"
#include "noentry.h"
#include <stdio.h>
#include <math.h>

GameWindowData g_windows[MAX_WINDOWS];
int g_windowCount = 0;
int g_resizeGeneration = 0;

RECT g_noEntryZones[MAX_NOENTRY_ZONES];
int g_noEntryZoneCount = 0;

extern void Strategy_HandleMouseDown(int index);
extern void Strategy_HandleMouseUp(int index);
extern void Strategy_HandleButtonClick(WindowKind kind);

static const char *kGameWindowClass = "WA_GameWindow";
static const int NOENTRY_BORDER_WIDTH = 5;
static const int STRIPE_WIDTH = 20;

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

static int IsInteractiveKind(WindowKind kind)
{
    return kind == WT_MOVABLE || kind == WT_RESIZABLE ||
           kind == WT_MOVABLE_NOENTRY || kind == WT_RESIZABLE_NOENTRY;
}

static int IsButtonKind(WindowKind kind)
{
    return kind == WT_BTN_START || kind == WT_BTN_RETRY || kind == WT_BTN_TOTITLE || kind == WT_BTN_EXIT;
}

int IsButtonWindowKind(WindowKind kind) { return IsButtonKind(kind); }

int IsQueryableWindow(WindowKind kind)
{
    return !IsButtonKind(kind) && kind != WT_GOAL;
}

int FindGoalIndex(void)
{
    for (int i = 0; i < g_windowCount; i++)
        if (g_windows[i].kind == WT_GOAL)
            return i;
    return -1;
}

void Goal_UpdateParent(void)
{
    int goalIdx = FindGoalIndex();
    if (goalIdx < 0)
        return;
    GameWindowData *goal = &g_windows[goalIdx];
    if (!goal->hwnd || goal->minimized)
        return; /* OnRestoreが明示的に再アタッチする; 最小化中のゴールは自己追跡しない */

    RECT gb;
    GetWindowFullBounds(goal->hwnd, &gb);
    int newParent = WindowQuery_GetFullyContaining(gb);
    if (newParent == goal->parentIdx)
        return;

    if (goal->parentIdx >= 0)
        Hierarchy_Detach(goalIdx);
    if (newParent >= 0)
        Hierarchy_Attach(newParent, goalIdx);
}

void Button_UpdateParent(void)
{
    for (int i = 0; i < g_windowCount; i++)
    {
        GameWindowData *btn = &g_windows[i];
        if (!IsButtonKind(btn->kind) || !btn->hwnd || btn->minimized)
            continue;

        RECT bb;
        GetWindowFullBounds(btn->hwnd, &bb);
        int newParent = WindowQuery_GetFullyContaining(bb);
        if (newParent == btn->parentIdx)
            continue;

        if (btn->parentIdx >= 0)
            Hierarchy_Detach(i);
        if (newParent >= 0)
            Hierarchy_Attach(newParent, i);
    }
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

        HBRUSH br = CreateSolidBrush(isRed ? RGB(255, 0, 0) : RGB(30, 30, 30));
        FillRect(hdc, &seg, br);
        DeleteObject(br);

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
    int r = 18;
    HPEN pen = CreatePen(PS_SOLID, 3, color);
    HPEN old = (HPEN)SelectObject(hdc, pen);

    MoveToEx(hdc, cx - r, cy, NULL);
    LineTo(hdc, cx + r, cy);
    MoveToEx(hdc, cx, cy - r, NULL);
    LineTo(hdc, cx, cy + r);

    int a = 6;
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

    SelectObject(hdc, old);
    DeleteObject(pen);
}

static void DrawResizableMark(HDC hdc, RECT rc, COLORREF color)
{
    int cx = (rc.left + rc.right) / 2;
    int cy = (rc.top + rc.bottom) / 2;
    int r = 18;
    HPEN pen = CreatePen(PS_SOLID, 3, color);
    HPEN old = (HPEN)SelectObject(hdc, pen);

    MoveToEx(hdc, cx - r, cy - r, NULL);
    LineTo(hdc, cx + r, cy + r);
    int a = 6;
    MoveToEx(hdc, cx - r, cy - r, NULL);
    LineTo(hdc, cx - r + a, cy - r);
    MoveToEx(hdc, cx - r, cy - r, NULL);
    LineTo(hdc, cx - r, cy - r + a);
    MoveToEx(hdc, cx + r, cy + r, NULL);
    LineTo(hdc, cx + r - a, cy + r);
    MoveToEx(hdc, cx + r, cy + r, NULL);
    LineTo(hdc, cx + r, cy + r - a);

    SelectObject(hdc, old);
    DeleteObject(pen);
}

static void DrawMinimizableMark(HDC hdc, RECT rc, COLORREF color)
{
    /* MinimizableWindowStrategy.DrawStrategyMark: bounds中央にmarkSize=60の
       ボックス、その中央にbarHeight=markSize/6=10のバーを配置する。 */
    int cx = (rc.left + rc.right) / 2;
    int cy = (rc.top + rc.bottom) / 2;
    HBRUSH br = CreateSolidBrush(color);
    RECT bar = {cx - 30, cy - 5, cx + 30, cy + 5};
    FillRect(hdc, &bar, br);
    DeleteObject(br);
}

static void DrawDeletableMark(HDC hdc, RECT rc, COLORREF color)
{
    int cx = (rc.left + rc.right) / 2;
    int cy = (rc.top + rc.bottom) / 2;
    int r = 18;
    HPEN pen = CreatePen(PS_SOLID, 3, color);
    HPEN old = (HPEN)SelectObject(hdc, pen);

    MoveToEx(hdc, cx - r, cy - r, NULL);
    LineTo(hdc, cx + r, cy + r);
    MoveToEx(hdc, cx + r, cy - r, NULL);
    LineTo(hdc, cx - r, cy + r);

    SelectObject(hdc, old);
    DeleteObject(pen);
}

static void DrawGoalMark(HDC hdc, RECT rc)
{
    /* Goal_Paintは baseFontSize = Math.Min(localRenderRect.Width,
       localRenderRect.Height) を計算する。ここでlocalRenderRectは衝突ボックスの
       RENDER_RATIO=1.5倍 -- 「G」のグリフはゴールの現在サイズに応じてスケールし、
       固定ポイントサイズではない。ここで固定値40を使うとデフォルトの64x64では
       問題なく見えるが、ゴールがリサイズされると明らかに一致しなくなる。 */
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    int fontHeight = (int)(((w < h) ? w : h) * 1.5f);
    if (fontHeight < 8)
        fontHeight = 8;

    SetBkMode(hdc, TRANSPARENT);
    HFONT font = CreateFontA(fontHeight, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
    HFONT old = (HFONT)SelectObject(hdc, font);
    SetTextColor(hdc, RGB(40, 40, 40));
    RECT shadow = rc;
    OffsetRect(&shadow, 1, 1);
    DrawTextA(hdc, "G", -1, &shadow, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SetTextColor(hdc, RGB(255, 215, 0));
    DrawTextA(hdc, "G", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, old);
    DeleteObject(font);
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

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    /* GameButton.Button_PaintはGameWindowとは別に自身の背景を塗りつぶし、
       ホバー時に明るくする: ホバー時はFromArgb(230,230,230)、待機時は
       (200,200,200) -- data->bg（生成時に固定）だけではこれを表現できないため、
       ボタン種別の場合はここで分岐してdata->bgをそのまま使わないようにしている。 */
    COLORREF fillColor = data->bg;
    if (IsButtonWindowKind(data->kind))
        fillColor = IsWindowHovered(index) ? RGB(230, 230, 230) : RGB(200, 200, 200);

    HBRUSH brush = CreateSolidBrush(fillColor);
    FillRect(memDC, &rc, brush);
    DeleteObject(brush);

    switch (data->kind)
    {
    case WT_MOVABLE:
    case WT_RESIZABLE:
    case WT_MINIMIZABLE:
    case WT_DELETABLE:
    case WT_MOVABLE_NOENTRY:
    case WT_RESIZABLE_NOENTRY:
    case WT_MINIMIZABLE_NOENTRY:
    {
        /* StrategyMarkUtility.GetMarkColor: ホバー中は白、それ以外は中間グレー。 */
        COLORREF markColor = IsWindowHovered(index) ? RGB(255, 255, 255) : RGB(128, 128, 128);
        if (data->kind == WT_MOVABLE || data->kind == WT_MOVABLE_NOENTRY)
            DrawMovableMark(memDC, rc, markColor);
        else if (data->kind == WT_RESIZABLE || data->kind == WT_RESIZABLE_NOENTRY)
            DrawResizableMark(memDC, rc, markColor);
        else if (data->kind == WT_MINIMIZABLE || data->kind == WT_MINIMIZABLE_NOENTRY)
            DrawMinimizableMark(memDC, rc, markColor);
        else
            DrawDeletableMark(memDC, rc, markColor);
        break;
    }
    case WT_GOAL:
        DrawGoalMark(memDC, rc);
        break;
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
        HFONT oldFont = (HFONT)SelectObject(memDC, font);
        SetTextColor(memDC, data->fg);
        SetBkMode(memDC, TRANSPARENT);
        DrawTextA(memDC, data->text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_WORDBREAK);
        SelectObject(memDC, oldFont);
    }

    if (data->isNoEntry)
    {
        DrawClockwiseStripeBorder(memDC, rc, data->stripeOffset);
    }
    else if (data->parentIdx >= 0)
    {
        /* WindowRenderingManagerは、親を持つ全てのウィンドウ（独自の縞模様を
           持つNoEntryウィンドウだけでなく）に対して、この親色に基づく
           アウトラインを描画する。 */
        COLORREF outline = CalculateOutlineColor(g_windows[data->parentIdx].bg);
        HPEN pen = CreatePen(PS_SOLID, 5, outline);
        HPEN oldOutlinePen = (HPEN)SelectObject(memDC, pen);
        HBRUSH oldOutlineBrush = (HBRUSH)SelectObject(memDC, GetStockObject(NULL_BRUSH));
        Rectangle(memDC, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(memDC, oldOutlineBrush);
        SelectObject(memDC, oldOutlinePen);
        DeleteObject(pen);
    }

    BitBlt(hdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);

    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK GameWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int index = FindWindowIndex(hwnd);

    switch (msg)
    {
    case WM_NCHITTEST:
    {
        if (index >= 0 && IsInteractiveKind(g_windows[index].kind))
        {
            LRESULT def = DefWindowProcA(hwnd, msg, wParam, lParam);
            if (def == HTCAPTION || def == HTCLIENT)
                return HTCLIENT;
            return def; /* リサイズ境界のヒットテストは無効化しておく; 移動処理は独自のドラッグで行う */
        }
        break;
    }
    case WM_LBUTTONDOWN:
        if (index >= 0)
        {
            ZOrder_BringToFront(hwnd);
            if (IsButtonKind(g_windows[index].kind))
                Strategy_HandleButtonClick(g_windows[index].kind);
            else
                Strategy_HandleMouseDown(index);
        }
        return 0;
    case WM_LBUTTONUP:
        if (index >= 0)
            Strategy_HandleMouseUp(index);
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
        /* HandleSysCommandは常にWM_SYSCOMMANDをウィンドウのStrategyに転送し
           NotHandledを返す。そのため、この後もDefWindowProcAが実際のOSレベルの
           最小化/復元を実行する（このcaseは下の共有"break"にフォールスルーし、
           "return"はしない）。オリジナルではStrategyディスパッチ自体が非対称:
             - SC_RESTORE -> BaseWindowStrategy.HandleWindowMessageが全ての
               ウィンドウ種別に対してこれを処理する（window.OnRestore()）。
               復元はストラテジーに関わらず同じ動作のため。
             - SC_MINIMIZE -> MinimizableWindowStrategyのみがこれをオーバーライドし、
               最小化カスケード（WindowEffectManager.ApplyEffects）を実行する。
               他の全てのストラテジーの基底ハンドラはこれに対して何もしないため、
               Minimizableでないウィンドウが何らかの形でSC_MINIMIZEを受け取っても
               （通常はこれをトリガーするタイトルバーのボックス自体が存在しない）、
               ゲームロジックの効果なしに素のOSレベルの最小化が行われるだけ --
               これは「改善」すべき点ではなく、単に一致させるべき挙動。
           最小化されたWT_MINIMIZABLEウィンドウがここでSC_RESTOREの処理を
           一度も受けなければ、OS標準の復元でのみ最小化解除が可能になるが、
           それはHWNDの非表示を解除するだけで、ゲーム側のminimized=trueフラグを
           クリアすることも、Hierarchy_CheckAndUpdateを再実行することもない。
           その結果、再び表示された後も永続的に親子ツリーから外れたままになる。 */
        int command = (int)(wParam & 0xFFF0);
        if (index >= 0)
        {
            int isMinimizableKind = (g_windows[index].kind == WT_MINIMIZABLE ||
                                      g_windows[index].kind == WT_MINIMIZABLE_NOENTRY);
            if (command == SC_MINIMIZE && isMinimizableKind && !g_windows[index].minimized)
                SetWindowMinimized(index, 1);
            else if (command == SC_RESTORE && g_windows[index].minimized)
                SetWindowMinimized(index, 0);
        }
        break; /* 下のDefWindowProcAにフォールスルーする */
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
    g_windowCount = 0;
    NoEntry_ResetZones();
    ZOrder_Reset();
}

int CreateGameWindowIndexed(HINSTANCE hInstance, WindowKind kind, int x, int y, int w, int h, const char *text)
{
    if (g_windowCount >= MAX_WINDOWS)
        return -1;

    COLORREF bg, fg;
    int solid = 1;
    int isNoEntry = 0;
    switch (kind)
    {
    case WT_NORMAL_BLACK:
        bg = RGB(0, 0, 0);
        fg = RGB(255, 255, 255);
        break;
    case WT_NORMAL_WHITE:
        bg = RGB(255, 255, 255);
        fg = RGB(0, 0, 0);
        break;
    case WT_TEXT_DISPLAY:
        bg = RGB(0, 0, 0);
        fg = RGB(255, 255, 255);
        solid = 0;
        break;
    case WT_MOVABLE:
        bg = RGB(173, 216, 230);
        fg = RGB(0, 0, 0);
        break;
    case WT_RESIZABLE:
        bg = RGB(144, 238, 144);
        fg = RGB(0, 0, 0);
        break;
    case WT_MINIMIZABLE:
    case WT_DELETABLE:
        bg = RGB(255, 182, 193);
        fg = RGB(0, 0, 0);
        break;
    case WT_NORMAL_BLACK_NOENTRY:
        bg = RGB(0, 0, 0);
        fg = RGB(255, 255, 255);
        isNoEntry = 1;
        break;
    case WT_NORMAL_WHITE_NOENTRY:
        bg = RGB(255, 255, 255);
        fg = RGB(0, 0, 0);
        isNoEntry = 1;
        break;
    case WT_RESIZABLE_NOENTRY:
        bg = RGB(144, 238, 144);
        fg = RGB(0, 0, 0);
        isNoEntry = 1;
        break;
    case WT_MOVABLE_NOENTRY:
        bg = RGB(173, 216, 230);
        fg = RGB(0, 0, 0);
        isNoEntry = 1;
        break;
    case WT_MINIMIZABLE_NOENTRY:
        bg = RGB(255, 182, 193);
        fg = RGB(0, 0, 0);
        isNoEntry = 1;
        break;
    case WT_GOAL:
        bg = RGB(255, 0, 255);
        fg = RGB(255, 215, 0);
        solid = 0;
        break;
    case WT_BTN_START:
    case WT_BTN_RETRY:
    case WT_BTN_TOTITLE:
    case WT_BTN_EXIT:
        bg = RGB(200, 200, 200);
        fg = RGB(0, 0, 0);
        solid = 0;
        break;
    default:
        bg = RGB(128, 128, 128);
        fg = RGB(255, 255, 255);
        break;
    }

    DWORD style, exStyle;
    if (kind == WT_GOAL)
    {
        style = WS_POPUP | WS_VISIBLE;
        exStyle = WS_EX_LAYERED;
    }
    else if (IsButtonKind(kind))
    {
        /* GameButton.InitializeButtonはFormBorderStyle.Noneを設定する -- ゴールと
           同様に枠なし。ここでWS_BORDERを付けると、オリジナルには枠が無いのに
           GetWindowFullBoundsのクライアント/外枠の分離がボタンにも適用されて
           しまう。 */
        style = WS_POPUP | WS_VISIBLE;
        exStyle = WS_EX_TOPMOST;
    }
    else
    {
        style = WS_CAPTION | WS_SYSMENU | WS_BORDER | WS_VISIBLE;
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
    else if (!IsButtonKind(kind))
    {
        HMENU sysMenu = GetSystemMenu(hwnd, FALSE);
        if (sysMenu)
            EnableMenuItem(sysMenu, SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);
    }

    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    int index = g_windowCount++;
    GameWindowData *data = &g_windows[index];
    ZeroMemory(data, sizeof(*data));
    data->hwnd = hwnd;
    data->kind = kind;
    data->bg = bg;
    data->fg = fg;
    data->solid = solid;
    data->isNoEntry = isNoEntry;
    data->parentIdx = -1;
    data->childCount = 0;
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

void SetWindowMinimized(int index, int minimized)
{
    /* GameWindow.OnMinimize/OnRestoreは非対称: 最小化はサブツリー全体
       （その中のどこに親子付けされていようとプレイヤーやゴールも含む）を
       再帰的に解体し個別に最小化するが、復元は復元対象の1つのウィンドウ
       にしか作用しない。Hierarchy_MinimizeSubtreeとHierarchy_RestoreWindow
       を参照。 */
    if (minimized)
        Hierarchy_MinimizeSubtree(index);
    else
        Hierarchy_RestoreWindow(index);
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
}
