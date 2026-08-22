/* Player/PlayerPhysics.cs, PlayerWindowInteraction.cs, PlayerForm.cs (HandleMovement) から移植。 */
#include "player.h"
#include "gamewindow.h"
#include "noentry.h"
#include "windowquery.h"
#include "zorder.h"
#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <float.h>

#define MOVE_SPEED 400.0f
#define GRAVITY 2600.0f
#define JUMP_FORCE 1100.0f
#define GROUND_CHECK_H 15

/* 前方宣言: 実体は移動可能領域ロジックの他の部分と一緒に後で定義されるが、
   Player_ApplyParentScaleが位置の再クランプのためにそれより前に必要とする。 */
static int IsValidMove(RECT bounds, int parentIdx);

static const char *kPlayerWindowClass = "WA_Player";
static Player *g_activePlayer = NULL;

static int RectsOverlap(RECT a, RECT b) { return WindowQuery_RectsOverlap(a, b); }

#define MAX_INTERSECTING 64

/* WindowManager.GetIntersectingWindows(sweepBounds)と一致させる: `sweep`と
   全体境界が重なる全てのqueryable（GameWindow相当）ウィンドウを、Z-orderで
   前面から背面の順に並べる。CheckGroundedの「外側」「内側」両方の分岐が
   このまさに同じ集合を探索する -- 現在の親とその直接の子だけではない -- ため、
   プレイヤーの親と階層的に関連していない床（兄弟ウィンドウや、sweepがたまたま
   届く他のウィンドウ）もすり抜けずに見つけられる。 */
static int GatherIntersectingWindows(RECT sweep, int *out, int maxOut)
{
    int n = 0;
    for (int i = 0; i < g_windowCount && n < maxOut; i++)
    {
        GameWindowData *d = &g_windows[i];
        if (!IsQueryableWindow(d->kind) || !d->hwnd || d->minimized)
            continue;
        RECT wb;
        GetWindowFullBounds(d->hwnd, &wb);
        if (!RectsOverlap(sweep, wb))
            continue;
        out[n++] = i;
    }
    for (int i = 1; i < n; i++)
    {
        int key = out[i];
        int keyZ = ZOrder_GetIndex(g_windows[key].hwnd);
        int j = i - 1;
        while (j >= 0 && ZOrder_GetIndex(g_windows[out[j]].hwnd) < keyZ)
        {
            out[j + 1] = out[j];
            j--;
        }
        out[j + 1] = key;
    }
    return n;
}

static void OffsetR(RECT *r, int dx, int dy)
{
    r->left += dx;
    r->right += dx;
    r->top += dy;
    r->bottom += dy;
}

/* PlayerFormの実際のFormは衝突ボックスのDISPLAY_RATIO=2.0倍で、それを中心に
   配置される（RENDER_RATIO=1.0は描画矩形が衝突ボックスと完全に一致することを
   意味するので、ここでは「描画」と「衝突」は一致し、表示用フォームだけが
   大きい）。この余分なマージンにより、PlayerAnimationのスクワッシュ/ストレッチが
   名目上60x60のヒットボックスの外側までクリップされずに描画できる余地が
   生まれる -- これがないと、アニメーションする本体とそのアウトラインが
   衝突ボックスちょうどに窮屈に収められ、意図した形状と目に見えて食い違って
   しまう。p->x/y/width/heightは常に論理的な衝突ボックスを保持し、実際の
   HWND矩形だけが2倍になる。 */
static void GetPlayerDisplayRect(const Player *p, int *dx, int *dy, int *dw, int *dh)
{
    *dw = p->width * 2;
    *dh = p->height * 2;
    *dx = (int)p->x - p->width / 2;
    *dy = (int)p->y - p->height / 2;
}

/* ---- 描画（変更なし） ---- */

static void PaintPlayer(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    HBRUSH keyBrush = CreateSolidBrush(RGB(255, 0, 255));
    FillRect(memDC, &rc, keyBrush);
    DeleteObject(keyBrush);

    /* PlayerForm.OnPaint: 本体は、クライアント矩形全体ではなくRENDER矩形
       （RENDER_RATIO=1.0 -> 衝突ボックスそのもの）を基準に、アニメーションの
       ScaleX/ScaleY内で足元を基準にして描画される -- クライアント矩形は
       DISPLAYフォームであり、衝突ボックスのDISPLAY_RATIO=2.0倍でそれを中心に
       配置される。これにより、アニメーションがクリップされずに伸縮できる
       マージンが確保される。 */
    float scaleX = 1.0f, scaleY = 1.0f;
    int logicalW = PLAYER_SIZE, logicalH = PLAYER_SIZE;
    if (g_activePlayer)
    {
        scaleX = g_activePlayer->anim.scaleX;
        scaleY = g_activePlayer->anim.scaleY;
        logicalW = g_activePlayer->width;
        logicalH = g_activePlayer->height;
    }
    float marginX = (float)(rc.right - rc.left - logicalW) / 2.0f;
    float marginY = (float)(rc.bottom - rc.top - logicalH) / 2.0f;
    float fullW = (float)logicalW;
    float fullH = (float)logicalH;
    float visualW = fullW * scaleX;
    float visualH = fullH * scaleY;
    float centerX = marginX + fullW / 2.0f;
    float bottomY = marginY + fullH;

    RECT body;
    body.left = (LONG)(centerX - visualW / 2.0f);
    body.top = (LONG)(bottomY - visualH);
    body.right = (LONG)(centerX + visualW / 2.0f);
    body.bottom = (LONG)bottomY;

    HBRUSH bodyBrush = CreateSolidBrush(RGB(174, 214, 241));
    HPEN outlinePen = CreatePen(PS_SOLID, 4, RGB(52, 73, 94));
    HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, bodyBrush);
    HPEN oldPen = (HPEN)SelectObject(memDC, outlinePen);
    RoundRect(memDC, body.left, body.top, body.right, body.bottom, 16, 16);
    SelectObject(memDC, oldBrush);
    SelectObject(memDC, oldPen);
    DeleteObject(bodyBrush);
    DeleteObject(outlinePen);

    HBRUSH eyeBrush = CreateSolidBrush(RGB(52, 73, 94));
    HBRUSH oldEyeBrush = (HBRUSH)SelectObject(memDC, eyeBrush);
    float eyeY = bottomY - visualH * 0.6f;
    float eyeOffset = visualW * 0.2f;
    float eyeX = (g_activePlayer && g_activePlayer->facingRight) ? (centerX + eyeOffset) : (centerX - eyeOffset);
    Ellipse(memDC, (int)eyeX - 4, (int)eyeY - 4, (int)eyeX + 4, (int)eyeY + 4);
    SelectObject(memDC, oldEyeBrush);
    DeleteObject(eyeBrush);

    /* PlayerForm.OnPaintは、本体自身の固定色ボーダーの上に重ねて、親の色に
       基づく追加のアウトラインを描画する。 */
    if (g_activePlayer && g_activePlayer->parentIdx >= 0)
    {
        GameWindowData *parent = GetWindowData(g_activePlayer->parentIdx);
        if (parent)
        {
            COLORREF outline = CalculateOutlineColor(parent->bg);
            HPEN parentPen = CreatePen(PS_SOLID, 5, outline);
            HPEN oldParentPen = (HPEN)SelectObject(memDC, parentPen);
            HBRUSH oldParentBrush = (HBRUSH)SelectObject(memDC, GetStockObject(NULL_BRUSH));
            RoundRect(memDC, body.left, body.top, body.right, body.bottom, 16, 16);
            SelectObject(memDC, oldParentBrush);
            SelectObject(memDC, oldParentPen);
            DeleteObject(parentPen);
        }
    }

    BitBlt(hdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);

    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK PlayerWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_PAINT:
        PaintPlayer(hwnd);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEACTIVATE:
        /* HandlePlayerFormMessages: プレイヤーはクリックしてもアクティブ化されない。 */
        return MA_NOACTIVATE;
    case WM_ACTIVATE:
        return 0;
    case WM_SYSCOMMAND:
    {
        /* HandlePlayerFormMessagesのWM_SYSCOMMAND処理と正確に一致させる:
           これにより、プレイヤー自身のタスクバー項目（WS_EX_APPWINDOWが
           それを与える）経由で最小化された後に復元して回復できる --
           再帰的な最小化カスケードは、それを「含む」ウィンドウが最小化された
           ときにPlayer_OnMinimizeを単なる関数呼び出しとして直接呼ぶだけで、
           そのカスケードの中で逆にPlayer_OnRestoreが呼ばれることは決してない。
           これはGameWindow.OnRestoreもカスケードしないのと一致する。この
           ハンドラが無いと、タスクバーのボタン（またはSC_RESTOREを送信する
           他のネイティブな経路）経由でプレイヤーのウィンドウを復元しても、
           Player_OnRestoreにisMinimizedをクリアするよう伝えることが決してなく、
           見た目上は最小化解除されてもプレイヤーは永久にフリーズしたままに
           なる。これらは全て（DefWindowProcAに渡されず）ここで飲み込まれる。
           Player_OnMinimize/Player_OnRestoreが既に実際のShowWindow呼び出しを
           自ら行っているためで、これはオリジナルがbase.WndProcに
           フォールスルーせずSuccessを返すのと全く同じ。 */
        int command = (int)(wParam & 0xFFF0);
        Player *p = Player_GetActive();
        switch (command)
        {
        case SC_MINIMIZE:
            if (p)
                Player_OnMinimize(p);
            return 0;
        case SC_RESTORE:
            if (p)
                Player_OnRestore(p);
            return 0;
        case SC_CLOSE:
            /* プレイヤーは閉じることができない。 */
            return 0;
        }
        break;
    }
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

void RegisterPlayerWindowClass(HINSTANCE hInstance)
{
    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = PlayerWindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = kPlayerWindowClass;
    wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.hbrBackground = NULL;
    RegisterClassA(&wc);
}

HWND CreatePlayerWindow(HINSTANCE hInstance, Player *p, int startX, int startY)
{
    p->x = (float)startX;
    p->y = (float)startY;
    p->width = PLAYER_SIZE;
    p->height = PLAYER_SIZE;
    p->origSize.cx = PLAYER_SIZE;
    p->origSize.cy = PLAYER_SIZE;
    p->origSizeGen = 0;
    p->vy = 0.0f;
    p->grounded = 0;
    p->facingRight = 1;
    p->parentIdx = -1;
    p->isMinimized = 0;
    p->lastValidParentIdx = -1;
    Anim_Init(&p->anim);

    int dx, dy, dw, dh;
    GetPlayerDisplayRect(p, &dx, &dy, &dw, &dh);
    /* WS_EX_APPWINDOWは、これがWS_POPUP（デフォルトではタスクバーボタンを
       得られない）であってもタスクバーボタンを強制的に持たせる -- PlayerForm
       は通常のWinFormsのFormであり、標準でShowInTaskbar=trueを持つ。
       OnMinimizeが実際にそれを最小化すると（下記）、オリジナルは他の
       最小化されたアプリウィンドウと同様にタスクバーに表示される。 */
    HWND hwnd = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_APPWINDOW, kPlayerWindowClass, "Player",
        WS_POPUP | WS_VISIBLE,
        dx, dy, dw, dh,
        NULL, NULL, hInstance, NULL);

    if (hwnd)
    {
        SetLayeredWindowAttributes(hwnd, RGB(255, 0, 255), 0, LWA_COLORKEY);
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    p->hwnd = hwnd;
    g_activePlayer = p;
    return hwnd;
}

Player *Player_GetActive(void) { return g_activePlayer; }

void Player_AssignInitialParent(Player *p)
{
    RECT pb;
    Player_GetBounds(p, &pb);

    int bestIdx = -1;
    int bestZ = -1;
    for (int i = 0; i < g_windowCount; i++)
    {
        GameWindowData *d = &g_windows[i];
        if (!IsQueryableWindow(d->kind) || !d->hwnd || d->minimized)
            continue;

        RECT client;
        WindowQuery_GetClientBounds(i, &client);
        if (pb.left >= client.left && pb.top >= client.top &&
            pb.right <= client.right && pb.bottom <= client.bottom)
        {
            int z = ZOrder_GetIndex(d->hwnd);
            if (z > bestZ)
            {
                bestZ = z;
                bestIdx = i;
            }
        }
    }
    p->parentIdx = bestIdx;
}

void Player_Reset(Player *p, int startX, int startY)
{
    p->x = (float)startX;
    p->y = (float)startY;
    p->width = PLAYER_SIZE;
    p->height = PLAYER_SIZE;
    p->origSize.cx = PLAYER_SIZE;
    p->origSize.cy = PLAYER_SIZE;
    p->origSizeGen = 0;
    p->vy = 0.0f;
    p->grounded = 0;
    p->parentIdx = -1;
    p->isMinimized = 0;
    p->lastValidParentIdx = -1;
    Anim_Init(&p->anim);
    if (p->hwnd)
    {
        int dx, dy, dw, dh;
        GetPlayerDisplayRect(p, &dx, &dy, &dw, &dh);
        SetWindowPos(p->hwnd, HWND_TOPMOST, dx, dy, dw, dh, SWP_NOACTIVATE);
        InvalidateRect(p->hwnd, NULL, FALSE);
    }
}

void Player_GetBounds(const Player *p, RECT *out)
{
    out->left = (LONG)p->x;
    out->top = (LONG)p->y;
    out->right = (LONG)p->x + p->width;
    out->bottom = (LONG)p->y + p->height;
}

void Player_FollowParentMove(Player *p, int parentIdx, int dx, int dy)
{
    if (p->parentIdx < 0 || (dx == 0 && dy == 0))
        return;
    int idx = p->parentIdx;
    int guard = 0;
    while (idx >= 0 && guard++ < MAX_WINDOWS)
    {
        if (idx == parentIdx)
        {
            p->x += (float)dx;
            p->y += (float)dy;
            return;
        }
        idx = g_windows[idx].parentIdx;
    }
}

void Player_ApplyParentScale(Player *p, int windowIndex, float scaleX, float scaleY)
{
    if (p->parentIdx != windowIndex || p->origSize.cx <= 0 || p->origSize.cy <= 0)
        return;

    int newW = (int)(p->origSize.cx * scaleX);
    int newH = (int)(p->origSize.cy * scaleY);
    if (newW < PLAYER_MIN_SIZE)
        newW = PLAYER_MIN_SIZE;
    if (newH < PLAYER_MIN_SIZE)
        newH = PLAYER_MIN_SIZE;

    p->width = newW;
    p->height = newH;

    /* AdjustPositionAfterResize: 新しい境界が親自身の境界に収まらなくなった
       場合、位置をその中にクランプし直す（C#側のフォールバックに合わせ、
       直接の親のCollisionBounds全体に対してのみチェックする）。 */
    RECT parentBounds;
    GetWindowFullBounds(g_windows[windowIndex].hwnd, &parentBounds);

    RECT proposed = {(int)p->x, (int)p->y, (int)p->x + newW, (int)p->y + newH};
    if (!IsValidMove(proposed, windowIndex))
    {
        int maxX = parentBounds.right - newW;
        int maxY = parentBounds.bottom - newH;
        int clampedX = (int)p->x;
        int clampedY = (int)p->y;
        if (clampedX < parentBounds.left)
            clampedX = parentBounds.left;
        if (clampedX > maxX)
            clampedX = maxX;
        if (clampedY < parentBounds.top)
            clampedY = parentBounds.top;
        if (clampedY > maxY)
            clampedY = maxY;
        p->x = (float)clampedX;
        p->y = (float)clampedY;
    }

    if (p->hwnd)
    {
        int dx, dy, dw, dh;
        GetPlayerDisplayRect(p, &dx, &dy, &dw, &dh);
        SetWindowPos(p->hwnd, NULL, dx, dy, dw, dh, SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(p->hwnd, NULL, FALSE);
        /* レイヤードカラーキーウィンドウを拡大すると、次のWM_PAINTが実際に
           マゼンタのカラーキー+本体でそこを塗りつぶすまで、新しく露出した
           端には直前にそこに合成されていたもの（黒一色のフラッシュとして
           見える）が表示され続ける。InvalidateRectだけでは、その再描画は
           次のメッセージループパスにキューされるだけ; ここで同期的に
           強制することで、そうしなければ黒い筋が表示されてしまう1フレーム分の
           隙間を埋める。 */
        UpdateWindow(p->hwnd);
    }
}

/* ---- CheckHorizontalCollision / CheckVerticalCollision: NoEntryのみ ---- */

static int SignOf(float v) { return v > 0 ? 1 : (v < 0 ? -1 : 0); }

static void CheckHorizontalCollision(RECT bounds, float *moveX)
{
    if (fabsf(*moveX) < 0.1f)
        return;

    int dir = SignOf(*moveX);
    int width = bounds.right - bounds.left;
    int height = bounds.bottom - bounds.top;

    int sweepLeft = dir > 0 ? bounds.left : bounds.left + (int)(*moveX);
    int sweepRight = dir > 0 ? bounds.right + (int)(*moveX) : bounds.right;
    RECT sweep = {sweepLeft, bounds.top, sweepRight, bounds.top + height};

    for (int i = 0; i < g_noEntryZoneCount; i++)
    {
        RECT z = g_noEntryZones[i];
        if (!RectsOverlap(sweep, z))
            continue;
        if (dir > 0)
        {
            int maxX = z.left - width;
            float cap = (float)(maxX - bounds.left);
            if (*moveX > cap)
                *moveX = cap;
        }
        else
        {
            int minX = z.right;
            float cap = (float)(minX - bounds.left);
            if (*moveX < cap)
                *moveX = cap;
        }
    }

    for (int i = 0; i < g_windowCount; i++)
    {
        if (!g_windows[i].isNoEntry || !g_windows[i].hwnd || g_windows[i].minimized)
            continue;
        RECT wb;
        GetWindowFullBounds(g_windows[i].hwnd, &wb);
        RECT bnd[4];
        int c = NoEntry_GetBoundaryRects(i, bnd);
        for (int b = 0; b < c; b++)
        {
            if (!RectsOverlap(sweep, bnd[b]))
                continue;
            RECT vis;
            IntersectRect(&vis, &sweep, &bnd[b]);
            if (!NoEntry_IsRectVisibleFromWindow(i, vis))
                continue; /* より前面のNoEntryウィンドウに隠れている */
            int isLeftEdge = abs(bnd[b].left - wb.left) < (5 + width / 2 + 2);
            int isRightEdge = abs(bnd[b].right - wb.right) < (5 + width / 2 + 2);
            int inside = bounds.left >= wb.left && bounds.right <= wb.right &&
                         bounds.top >= wb.top && bounds.bottom <= wb.bottom;
            if (inside)
            {
                if (dir > 0 && isRightEdge)
                {
                    float cap = (float)(bnd[b].left - width - bounds.left);
                    if (*moveX > cap)
                        *moveX = cap;
                }
                else if (dir < 0 && isLeftEdge)
                {
                    float cap = (float)(bnd[b].right - bounds.left);
                    if (*moveX < cap)
                        *moveX = cap;
                }
            }
            else
            {
                if (dir > 0 && isLeftEdge)
                {
                    float cap = (float)(bnd[b].left - width - bounds.left);
                    if (*moveX > cap)
                        *moveX = cap;
                }
                else if (dir < 0 && isRightEdge)
                {
                    float cap = (float)(bnd[b].right - bounds.left);
                    if (*moveX < cap)
                        *moveX = cap;
                }
            }
        }
    }
}

static void CheckVerticalCollision(RECT bounds, float *moveY, int *hitCeiling)
{
    if (fabsf(*moveY) < 0.1f)
        return;

    int dir = SignOf(*moveY);
    int width = bounds.right - bounds.left;
    int height = bounds.bottom - bounds.top;

    int sweepTop = dir > 0 ? bounds.top : bounds.top + (int)(*moveY);
    int sweepBottom = dir > 0 ? bounds.bottom + (int)(*moveY) : bounds.bottom;
    RECT sweep = {bounds.left, sweepTop, bounds.left + width, sweepBottom};

    float original = *moveY;

    for (int i = 0; i < g_noEntryZoneCount; i++)
    {
        RECT z = g_noEntryZones[i];
        if (!RectsOverlap(sweep, z))
            continue;
        if (dir > 0)
        {
            int maxY = z.top - height;
            float cap = (float)(maxY - bounds.top);
            if (*moveY > cap)
                *moveY = cap;
        }
        else
        {
            int minY = z.bottom;
            float cap = (float)(minY - bounds.top);
            if (*moveY < cap)
                *moveY = cap;
        }
    }

    for (int i = 0; i < g_windowCount; i++)
    {
        if (!g_windows[i].isNoEntry || !g_windows[i].hwnd || g_windows[i].minimized)
            continue;
        RECT wb;
        GetWindowFullBounds(g_windows[i].hwnd, &wb);
        RECT bnd[4];
        int c = NoEntry_GetBoundaryRects(i, bnd);
        for (int b = 0; b < c; b++)
        {
            if (!RectsOverlap(sweep, bnd[b]))
                continue;
            RECT vis;
            IntersectRect(&vis, &sweep, &bnd[b]);
            if (!NoEntry_IsRectVisibleFromWindow(i, vis))
                continue; /* より前面のNoEntryウィンドウに隠れている */
            int isTopEdge = abs(bnd[b].top - wb.top) < (5 + height / 2 + 2);
            int isBottomEdge = abs(bnd[b].bottom - wb.bottom) < (5 + height / 2 + 2);
            int inside = bounds.left >= wb.left && bounds.right <= wb.right &&
                         bounds.top >= wb.top && bounds.bottom <= wb.bottom;
            if (inside)
            {
                if (dir > 0 && isBottomEdge)
                {
                    float cap = (float)(bnd[b].top - height - bounds.top);
                    if (*moveY > cap)
                        *moveY = cap;
                }
                else if (dir < 0 && isTopEdge)
                {
                    float cap = (float)(bnd[b].bottom - bounds.top);
                    if (*moveY < cap)
                        *moveY = cap;
                }
            }
            else
            {
                if (dir > 0 && isTopEdge)
                {
                    float cap = (float)(bnd[b].top - height - bounds.top);
                    if (*moveY > cap)
                        *moveY = cap;
                }
                else if (dir < 0 && isBottomEdge)
                {
                    float cap = (float)(bnd[b].bottom - bounds.top);
                    if (*moveY < cap)
                        *moveY = cap;
                }
            }
        }
    }

    if (original < 0.0f && *moveY > original)
        *hitCeiling = 1;
}

/* ---- IsValidMove / AdjustMovement: 現在の親を中心とした移動可能領域
   （自身の領域と、隣接/重なるトップレベルウィンドウの領域および子孫との
   和集合）への包含判定、または親の外にいる場合はメイン画面への包含判定。
   PlayerWindowInteraction + WindowManager.CalculateMovableRegionと一致させる。 ---- */

static int IsWithinScreen(RECT b)
{
    return b.left >= 0 && b.top >= 0 &&
           b.right <= GetSystemMetrics(SM_CXSCREEN) && b.bottom <= GetSystemMetrics(SM_CYSCREEN);
}

#define MAX_REGION_RECTS 64

static int IsValidMove(RECT bounds, int parentIdx)
{
    if (NoEntry_IntersectsAny(bounds))
        return 0;

    if (parentIdx < 0)
        return IsWithinScreen(bounds);

    RECT region[MAX_REGION_RECTS];
    int regionCount = WindowQuery_BuildMovableRegion(parentIdx, region, MAX_REGION_RECTS);
    if (regionCount == 0)
        return IsWithinScreen(bounds);

    POINT corners[4] = {
        {bounds.left, bounds.top},
        {bounds.right - 1, bounds.top},
        {bounds.left, bounds.bottom - 1},
        {bounds.right - 1, bounds.bottom - 1},
    };

    for (int c = 0; c < 4; c++)
    {
        if (!WindowQuery_PointInAnyRect(corners[c], region, regionCount))
            return 0;
    }
    return 1;
}

static RECT AdjustMovement(RECT oldBounds, RECT target, int parentIdx, int *hitCeiling)
{
    RECT adj = oldBounds;

    if (oldBounds.left != target.left)
    {
        int step = (target.left > oldBounds.left) ? 1 : -1;
        while (adj.left != target.left)
        {
            RECT test = adj;
            OffsetR(&test, step, 0);
            if (IsValidMove(test, parentIdx))
                adj = test;
            else
                break;
        }
    }

    if (oldBounds.top != target.top)
    {
        int step = (target.top > oldBounds.top) ? 1 : -1;
        while (adj.top != target.top)
        {
            RECT test = adj;
            OffsetR(&test, 0, step);
            if (IsValidMove(test, parentIdx))
                adj = test;
            else
            {
                if (step < 0)
                    *hitCeiling = 1;
                break;
            }
        }
    }

    return adj;
}

/* ---- HandleWindowCollisions（外側のみ）/ HandleButtonCollisions（常に）:
   交差する各ソリッドウィンドウ/ボタンを完全なボックス（床/天井/壁）として
   扱う。 ---- */

static RECT BoxCollideAgainst(RECT proposed, RECT current, HWND hwnd, int *hitCeiling)
{
    RECT wb;
    GetWindowFullBounds(hwnd, &wb);
    /* このガードは、実際のゲームがまずGetIntersectingWindows(adjustedBounds)を
       反復処理することと一致させる -- 以下の各辺のテスト（天井を含む）は、
       プレイヤーの提案ボックスが実際に重なっているウィンドウにのみ適用され、
       画面上のどこか同じ高さにあるだけのウィンドウには決して適用されない。 */
    if (!RectsOverlap(proposed, wb))
        return proposed;

    int width = proposed.right - proposed.left;
    int height = proposed.bottom - proposed.top;
    RECT adj = proposed;

    if (current.bottom <= wb.top && adj.bottom > wb.top)
    {
        adj.top = wb.top - height;
        adj.bottom = wb.top;
    }
    else if (current.top >= wb.bottom && adj.top < wb.bottom)
    {
        adj.top = wb.bottom;
        adj.bottom = wb.bottom + height;
        if (hitCeiling)
            *hitCeiling = 1;
    }
    else if (current.right <= wb.left && adj.right > wb.left)
    {
        adj.left = wb.left - width;
        adj.right = wb.left;
    }
    else if (current.left >= wb.right && adj.left < wb.right)
    {
        adj.left = wb.right;
        adj.right = wb.right + width;
    }
    return adj;
}

static RECT HandleWindowCollisions(RECT proposed, RECT current, int *hitCeiling)
{
    RECT adjusted = proposed;
    for (int i = 0; i < g_windowCount; i++)
    {
        GameWindowData *d = &g_windows[i];
        if (d->isNoEntry || IsButtonWindowKind(d->kind) || d->kind == WT_GOAL)
            continue;
        if (!d->hwnd || d->minimized)
            continue;

        adjusted = BoxCollideAgainst(adjusted, current, d->hwnd, hitCeiling);
    }
    return adjusted;
}

static RECT HandleButtonCollisions(RECT proposed, RECT current, int *hitCeiling)
{
    RECT adjusted = proposed;
    for (int i = 0; i < g_windowCount; i++)
    {
        GameWindowData *d = &g_windows[i];
        if (!IsButtonWindowKind(d->kind) || !d->hwnd)
            continue;
        adjusted = BoxCollideAgainst(adjusted, current, d->hwnd, hitCeiling);
    }
    return adjusted;
}

/* ---- HandleWindowTransitions: プレイヤーが「内側にいる」ウィンドウを更新する。 ---- */

static void HandleWindowTransitions(Player *p, RECT newBounds)
{
    if (p->parentIdx < 0)
    {
        int covering = WindowQuery_GetFullyContaining(newBounds);
        if (covering >= 0)
            p->parentIdx = covering;
    }
    else
    {
        int newWin = WindowQuery_GetTopWindowAt(newBounds, p->parentIdx);
        if (newWin >= 0 && newWin != p->parentIdx)
            p->parentIdx = newWin;
        else if (newWin < 0)
            p->parentIdx = -1;
    }
}

/* ---- CheckGrounded: 外側 = ウィンドウの上面 + NoEntry + 画面の下端;
   内側 = 同じくウィンドウの上面（ネストしたプラットフォーム）+ 親自身の
   クライアント下端を部屋の床として扱う。 ---- */

static void CheckGrounded(Player *p, float dt)
{
    if (p->vy < 0.0f)
    {
        p->grounded = 0;
        return;
    }

    int feetX = (int)p->x;
    int feetY = (int)p->y + p->height - 10;
    int feetW = p->width;

    float maxStep = fabsf(p->vy * dt);
    if (maxStep < 20.0f)
        maxStep = 20.0f;

    int sweepTop = (int)fminf((float)feetY, feetY + p->vy * dt) - 5;
    int sweepBottom = feetY + GROUND_CHECK_H + 10 + (int)maxStep;
    RECT sweep = {feetX, sweepTop, feetX + feetW, sweepBottom};

    int playerLeft = (int)p->x;
    int playerRight = (int)p->x + p->width;
    int playerBottom = (int)p->y + p->height;

    for (int i = 0; i < g_noEntryZoneCount; i++)
    {
        RECT z = g_noEntryZones[i];
        if (!RectsOverlap(sweep, z))
            continue;
        if (playerBottom >= z.top && playerBottom <= z.top + 5 &&
            playerRight > z.left && playerLeft < z.right)
        {
            p->grounded = 1;
            p->y = (float)(z.top - p->height);
            p->vy = 0.0f;
            return;
        }
    }

    for (int i = 0; i < g_windowCount; i++)
    {
        if (!g_windows[i].isNoEntry || !g_windows[i].hwnd || g_windows[i].minimized)
            continue;
        RECT bnd[4]; /* NoEntry_GetBoundaryRectsによる順序: 上, 下, 左, 右 */
        int c = NoEntry_GetBoundaryRects(i, bnd);
        /* CheckAnyNoEntryBoundaryCollisionは全てのNoEntryウィンドウの4辺を
           チェックし（最初に見つかった可視のヒットが勝つ）、プレイヤーの足元が
           見つかった辺の「上端」付近にあれば、その辺を床として扱う --
           ウィンドウの「上」に立つ（他の全方向は境界が通行不可能なため唯一
           到達可能なケース）と、固定的な「下端バンド」ではなく「上端バンド」に
           着地する。下端バンドだけをチェックしていると、プレイヤーは
           NoEntryウィンドウ上で接地判定を得ることが一切できなかった。 */
        for (int b = 0; b < c; b++)
        {
            RECT band = bnd[b];
            if (!RectsOverlap(sweep, band))
                continue;
            RECT visBand;
            IntersectRect(&visBand, &sweep, &band);
            if (!NoEntry_IsRectVisibleFromWindow(i, visBand))
                continue; /* より前面のウィンドウに隠れている */
            if (playerBottom >= band.top && playerBottom <= band.top + 5 &&
                playerRight > band.left && playerLeft < band.right)
            {
                p->grounded = 1;
                p->y = (float)(band.top - p->height);
                p->vy = 0.0f;
                return;
            }
            break; /* CheckAnyCollisionはウィンドウごとに最初に見つかった可視の辺で停止する */
        }
    }

    for (int i = 0; i < g_windowCount; i++)
    {
        if (!IsButtonWindowKind(g_windows[i].kind))
            continue;
        RECT wb;
        GetWindowFullBounds(g_windows[i].hwnd, &wb);
        if (!RectsOverlap(sweep, wb))
            continue;
        if (playerBottom >= wb.top && playerBottom <= wb.top + 5 &&
            playerRight > wb.left && playerLeft < wb.right)
        {
            p->grounded = 1;
            p->y = (float)(wb.top - p->height);
            p->vy = 0.0f;
            return;
        }
    }

    RECT currentFeetBounds = {feetX, feetY, feetX + feetW, feetY + GROUND_CHECK_H};
    int idxs[MAX_INTERSECTING];
    int n = GatherIntersectingWindows(sweep, idxs, MAX_INTERSECTING);

    if (p->parentIdx < 0)
    {
        /* 外側: NoEntryでない全てのウィンドウの上面（タイトルバーを含む）は、
           上から着地できる床になる -- parentWindow==nullの分岐と一致させる。
           候補として有効なのは、より前面にある交差ウィンドウの全体境界が
           プレイヤーの足元バンドを覆っていない場合のみ（Z-orderによる遮蔽）。 */
        for (int k = 0; k < n; k++)
        {
            GameWindowData *d = &g_windows[idxs[k]];
            if (d->isNoEntry)
                continue;
            RECT wb;
            GetWindowFullBounds(d->hwnd, &wb);
            if (playerBottom < wb.top || playerBottom > wb.top + 5 ||
                playerRight <= wb.left || playerLeft >= wb.right)
                continue;

            int isGroundValid = 1;
            int myZ = ZOrder_GetIndex(d->hwnd);
            for (int m = 0; m < n; m++)
            {
                GameWindowData *other = &g_windows[idxs[m]];
                if (ZOrder_GetIndex(other->hwnd) <= myZ)
                    continue;
                RECT ob;
                GetWindowFullBounds(other->hwnd, &ob);
                if (RectsOverlap(ob, currentFeetBounds))
                {
                    isGroundValid = 0;
                    break;
                }
            }
            if (isGroundValid)
            {
                p->grounded = 1;
                p->y = (float)(wb.top - p->height);
                p->vy = 0.0f;
                return;
            }
        }
    }
    else
    {
        /* 内側: ウィンドウのタイトルバーは、そのウィンドウ（や他のどのウィンドウ）
           の内側にいる間は床にはならない -- それが部屋自身の床であれネストした
           プラットフォームであれ、「下端」のみが対象になる。部屋自身の床は
           この同じループから自然に導かれる: 親ウィンドウ自身がsweepと交差する
           候補の1つ（プレイヤーの足元がその境界内にある）なので、ここでは他の
           交差ウィンドウと切り離した特別扱いは不要 -- オリジナルがこの分岐で
           「現在の親」を他の交差ウィンドウと区別しないのとまさに同じ。 */
        int bestBottom = INT_MAX;
        for (int k = 0; k < n; k++)
        {
            GameWindowData *w = &g_windows[idxs[k]];
            if (w->isNoEntry)
                continue;
            RECT wb;
            GetWindowFullBounds(w->hwnd, &wb);
            /* オリジナルのRectangle(x,y,w,h)形式では
               windowGroundArea = (Left, Bottom-maxStep-5, Width, maxStep+10)
               -> bottom = Bottom+5であり、+10ではない。 */
            RECT groundArea = {wb.left, wb.bottom - (int)maxStep - 5, wb.right, wb.bottom + 5};
            if (!RectsOverlap(currentFeetBounds, groundArea))
                continue;

            int groundY = wb.bottom;
            int isFloorVisible = 1;
            int myZ = ZOrder_GetIndex(w->hwnd);
            for (int m = 0; m < n; m++)
            {
                GameWindowData *other = &g_windows[idxs[m]];
                if (ZOrder_GetIndex(other->hwnd) <= myZ)
                    continue;
                RECT ob;
                GetWindowFullBounds(other->hwnd, &ob);
                RECT floorArea = {feetX, groundY - 2, feetX + feetW, groundY + 2};
                int contains = ob.left <= floorArea.left && ob.top <= floorArea.top &&
                               ob.right >= floorArea.right && ob.bottom >= floorArea.bottom;
                int overlapsAndAbove = RectsOverlap(ob, floorArea) && ob.bottom < groundY;
                if (contains || overlapsAndAbove)
                {
                    isFloorVisible = 0;
                    break;
                }
            }
            if (isFloorVisible && groundY < bestBottom)
                bestBottom = groundY;
        }

        if (bestBottom != INT_MAX)
        {
            p->grounded = 1;
            p->y = (float)(bestBottom - p->height);
            p->vy = 0.0f;
            return;
        }
    }

    if (p->parentIdx < 0)
    {
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        if (playerBottom >= screenH)
        {
            p->grounded = 1;
            p->y = (float)(screenH - p->height);
            p->vy = 0.0f;
            return;
        }
    }

    p->grounded = 0;
}

/* PlayerForm.CalculateDistanceToMovableBoundsTopと一致させる: プレイヤーの
   現在の上端と、その真上にある最も近いqueryableウィンドウのクライアント下端
   との間の隙間。低い天井付近でのジャンプ伸縮を制限するために使う。 */
static float DistanceToMovableBoundsTop(const Player *p)
{
    int playerLeft = (int)p->x;
    int playerRight = (int)p->x + p->width;
    int playerTop = (int)p->y;

    int closestBottom = INT_MIN;
    for (int i = 0; i < g_windowCount; i++)
    {
        GameWindowData *d = &g_windows[i];
        if (!IsQueryableWindow(d->kind) || !d->hwnd || d->minimized)
            continue;

        RECT cb;
        WindowQuery_GetClientBounds(i, &cb);
        if (cb.bottom > playerTop)
            continue;
        if (playerRight <= cb.left || playerLeft >= cb.right)
            continue;
        if (cb.bottom > closestBottom)
            closestBottom = cb.bottom;
    }

    if (closestBottom == INT_MIN)
        return FLT_MAX;

    float distance = (float)(playerTop - closestBottom);
    return distance < 0.0f ? 0.0f : distance;
}

void Player_OnMinimize(Player *p)
{
    p->isMinimized = 1;
    p->lastValidParentIdx = p->parentIdx;
    p->parentIdx = -1;

    /* PlayerForm.OnMinimizeはWindowState = FormWindowState.Minimizedを
       設定する -- 内部フラグだけではなく、実際のWin32の最小化（非表示になり
       タスクバーボタンとして表示される）。これがないと、物理演算は既に
       停止しているのにプレイヤーは完全に表示されたままドラッグ可能な
       ように見えてしまう。 */
    if (p->hwnd)
        ShowWindow(p->hwnd, SW_SHOWMINIMIZED);
}

void Player_OnRestore(Player *p)
{
    p->isMinimized = 0;

    if (p->hwnd)
        ShowWindow(p->hwnd, SW_RESTORE);

    RECT bounds;
    Player_GetBounds(p, &bounds);

    if (p->lastValidParentIdx >= 0)
    {
        GameWindowData *last = GetWindowData(p->lastValidParentIdx);
        if (last && last->hwnd && !last->minimized)
        {
            RECT lastBounds;
            GetWindowFullBounds(last->hwnd, &lastBounds);
            if (RectsOverlap(bounds, lastBounds))
            {
                p->parentIdx = p->lastValidParentIdx;
                return;
            }
        }
    }

    p->parentIdx = WindowQuery_GetTopWindowAt(bounds, -1);
}

void Player_Update(Player *p, float dt)
{
    if (p->isMinimized)
        return;

    int wasGrounded = p->grounded;

    /* PlayerInputHandler.UpdateFacingは左を先にチェックして即座にreturnするため、
       両方向が同時に押されている場合は左が優先される。 */
    if (GetAsyncKeyState('A') & 0x8000 || GetAsyncKeyState(VK_LEFT) & 0x8000)
        p->facingRight = 0;
    else if (GetAsyncKeyState('D') & 0x8000 || GetAsyncKeyState(VK_RIGHT) & 0x8000)
        p->facingRight = 1;

    if (p->grounded &&
        (GetAsyncKeyState(VK_SPACE) & 0x8000 || GetAsyncKeyState(VK_UP) & 0x8000 || GetAsyncKeyState('W') & 0x8000))
    {
        p->vy = -JUMP_FORCE;
        p->grounded = 0;
        Anim_StartJump(&p->anim);
    }

    float moveX = 0.0f;
    if (GetAsyncKeyState('A') & 0x8000 || GetAsyncKeyState(VK_LEFT) & 0x8000)
        moveX -= MOVE_SPEED * dt;
    if (GetAsyncKeyState('D') & 0x8000 || GetAsyncKeyState(VK_RIGHT) & 0x8000)
        moveX += MOVE_SPEED * dt;
    /* PlayerForm.UpdateAsyncは、この衝突前の生の値と同じもの（「移動前の
       水平速度を計算（アニメーション用）」）を計算し、UpdateAnimationStateに
       渡す -- そのため、壁に向かって移動キーを押し続けている場合、下記の
       実際の衝突後の移動が最終的に0にクランプされても、Runningアニメーションは
       再生され続ける。 */
    float rawDx = moveX;
    float moveY = p->vy * dt;

    RECT current;
    Player_GetBounds(p, &current);

    CheckHorizontalCollision(current, &moveX);
    int hitCeiling = 0;
    CheckVerticalCollision(current, &moveY, &hitCeiling);
    if (hitCeiling)
    {
        p->vy = 0.0f;
        Anim_ResetScale(&p->anim);
    }

    RECT proposed = current;
    OffsetR(&proposed, (int)moveX, (int)moveY);

    if (!IsValidMove(proposed, p->parentIdx))
    {
        int ceil2 = 0;
        proposed = AdjustMovement(current, proposed, p->parentIdx, &ceil2);
        if (ceil2)
        {
            p->vy = 0.0f;
            Anim_ResetScale(&p->anim);
        }
    }

    if (p->parentIdx < 0)
    {
        int ceil3 = 0;
        proposed = HandleWindowCollisions(proposed, current, &ceil3);
        if (ceil3)
        {
            p->vy = 0.0f;
            Anim_ResetScale(&p->anim);
        }
    }

    int ceil4 = 0;
    proposed = HandleButtonCollisions(proposed, current, &ceil4);
    if (ceil4)
    {
        p->vy = 0.0f;
        Anim_ResetScale(&p->anim);
    }

    HandleWindowTransitions(p, proposed);

    p->x = (float)proposed.left;
    p->y = (float)proposed.top;

    CheckGrounded(p, dt);

    /* ここで意図的にHandleWindowTransitionsを再実行しない。オリジナルは
       HandleMovementの中で、PROPOSED（接地スナップ前）の境界に対してのみ
       一度だけそれを呼ぶ -- スナップ後の位置に対しては決して呼ばない。
       これは見落としではない: GetTopWindowAtの5点チェックは厳密な不等号を
       使うため（境界に接するだけの点は「内側」とはみなされない）、
       CheckGroundedのYスナップはプレイヤーの足をちょうど床の境界線上に
       置く。境界ちょうどの座標で再チェックすると、たまたま他に何もその
       正確な線上を占めていない限り5点全てが現在の親を見逃してしまい、
       誤ってparentIdxを-1にクリアし、以後の全ての移動をフリーズさせて
       しまう。 */
    if (!p->grounded)
        p->vy += GRAVITY * dt;
    else
        p->vy = 0.0f;

    Anim_UpdateState(&p->anim, p->grounded, wasGrounded, rawDx);
    Anim_Update(&p->anim, DistanceToMovableBoundsTop(p), (float)p->height);

    if (p->hwnd)
    {
        int dx, dy, dw, dh;
        GetPlayerDisplayRect(p, &dx, &dy, &dw, &dh);
        SetWindowPos(p->hwnd, NULL, dx, dy, dw, dh, SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(p->hwnd, NULL, FALSE);
    }
}
