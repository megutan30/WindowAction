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
#include <dwmapi.h>

#define MOVE_SPEED 400.0f
#define GRAVITY 2600.0f
#define JUMP_FORCE 1100.0f
#define GROUND_CHECK_H 15

/* 前方宣言: 実体は移動可能領域ロジックの他の部分と一緒に後で定義されるが、
   Player_ApplyParentRelativeTransformが位置の再クランプのためにそれより
   前に必要とする。 */
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

    /* 制限なしリサイズウィンドウの子(GameWindow.PaintGameWindow)と同じ仕組み:
       乗っている(乗っていた)祖先が反転した回数のパリティをinheritedFlipX/Yに
       永続的に積算しており、それに応じて描画内容だけをStretchBltの負幅/
       負高さでミラーする。実HWNDの矩形自体は変えない(見た目だけの効果)。 */
    int flipX = g_activePlayer && g_activePlayer->inheritedFlipX;
    int flipY = g_activePlayer && g_activePlayer->inheritedFlipY;
    int fullW2 = rc.right - rc.left;
    int fullH2 = rc.bottom - rc.top;
    if (flipX || flipY)
    {
        /* 負の幅/高さを指定するミラー手法のGDI特有の癖: 原点をwidth/height
           そのものにすると、境界の1列/1行がステップ丸めの都合で描画されずに
           残り、コピー先の初期内容（レイヤードウィンドウの黒い初期背景）が
           カラーキーで抜けずそのまま黒い線として見えてしまう。原点を
           width-1/height-1にすることでその境界列/行も確実に上書きされるが、
           念のためhdc自体も先にカラーキーで塗っておき、それでも残る
           取りこぼし画素があれば黒ではなく透明として抜けるようにする。 */
        HBRUSH hdcKeyBrush = CreateSolidBrush(RGB(255, 0, 255));
        FillRect(hdc, &rc, hdcKeyBrush);
        DeleteObject(hdcKeyBrush);
        StretchBlt(hdc, flipX ? fullW2 - 1 : 0, flipY ? fullH2 - 1 : 0, flipX ? -fullW2 : fullW2, flipY ? -fullH2 : fullH2,
                   memDC, 0, 0, fullW2, fullH2, SRCCOPY);
    }
    else
    {
        BitBlt(hdc, 0, 0, fullW2, fullH2, memDC, 0, 0, SRCCOPY);
    }
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);

    EndPaint(hwnd, &ps);
}

/* GameWindow.cのCreateArgbDibSectionと同じ: DwmSetIconicThumbnail/
   DwmSetIconicLivePreviewBitmapに渡すビットマップは32bpp・トップダウンの
   DIBセクションである必要がある。 */
static HBITMAP CreatePlayerArgbDibSection(HDC referenceDC, int w, int h, void **outBits)
{
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; /* トップダウン(上から下)で格納する */
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    return CreateDIBSection(referenceDC, &bmi, DIB_RGB_COLORS, outBits, NULL, 0);
}

/* 縮小アニメーションを開始する直前に一度だけ呼ぶ（GameWindow.cの
   CaptureIconicBitmapと同じ理由: これが無いと、アニメーションで数px四方まで
   縮んだ後の内容がDWMのタスクバーサムネイル/ライブプレビューになって
   しまう）。PaintPlayerは背景をマゼンタのカラーキーで塗ってから本体を
   描画しているため、GetDC+BitBltでそのまま取り込むと背景がマゼンタの
   不透明なブロックとしてサムネイルに映ってしまう -- BitBltはアルファ
   チャンネルを一切書き換えないため、取り込んだ後にマゼンタの画素だけ
   アルファ0(透明)、それ以外をアルファ255(不透明)に手動で置き換えることで、
   プレイヤー本体だけが正しく切り抜かれたサムネイルになる。 */
static void CapturePlayerIconicBitmap(Player *p)
{
    if (p->iconicBitmap)
    {
        DeleteObject(p->iconicBitmap);
        p->iconicBitmap = NULL;
    }

    RECT rc;
    GetClientRect(p->hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0)
        return;

    HDC hdcWin = GetDC(p->hwnd);
    HDC hdcMem = CreateCompatibleDC(hdcWin);
    void *bits = NULL;
    HBITMAP bmp = CreatePlayerArgbDibSection(hdcWin, w, h, &bits);
    if (!bmp || !bits)
    {
        if (bmp)
            DeleteObject(bmp);
        DeleteDC(hdcMem);
        ReleaseDC(p->hwnd, hdcWin);
        return;
    }
    HBITMAP oldBmp = (HBITMAP)SelectObject(hdcMem, bmp);
    BitBlt(hdcMem, 0, 0, w, h, hdcWin, 0, 0, SRCCOPY);
    SelectObject(hdcMem, oldBmp);
    DeleteDC(hdcMem);
    ReleaseDC(p->hwnd, hdcWin);

    unsigned char *px = (unsigned char *)bits;
    for (int i = 0; i < w * h; i++)
    {
        unsigned char b = px[i * 4 + 0];
        unsigned char g = px[i * 4 + 1];
        unsigned char r = px[i * 4 + 2];
        int isMagenta = (r == 255 && g == 0 && b == 255);
        px[i * 4 + 3] = (unsigned char)(isMagenta ? 0 : 255);
    }

    p->iconicBitmap = bmp;
}

/* CapturePlayerIconicBitmap/RespondPlayerIconicThumbnail/
   RespondPlayerIconicLivePreview共通: キャプチャ済みビットマップを指定
   サイズへコピーした複製を作る。DwmSetIconicThumbnail/
   DwmSetIconicLivePreviewBitmapへ渡すビットマップの所有権はDWM側に移り、
   DWMが破棄する（MSDN仕様）ため、キャッシュ済みのp->iconicBitmap自身を
   直接渡してはならず、呼び出しのたびに複製する。 */
static HBITMAP CopyPlayerBitmapScaled(HBITMAP src, int srcW, int srcH, int dstW, int dstH)
{
    HDC screenDC = GetDC(NULL);
    HDC srcDC = CreateCompatibleDC(screenDC);
    HDC dstDC = CreateCompatibleDC(screenDC);
    void *bits = NULL;
    HBITMAP dstBmp = CreatePlayerArgbDibSection(screenDC, dstW, dstH, &bits);
    ReleaseDC(NULL, screenDC);
    if (!dstBmp)
    {
        DeleteDC(srcDC);
        DeleteDC(dstDC);
        return NULL;
    }

    HBITMAP oldSrc = (HBITMAP)SelectObject(srcDC, src);
    HBITMAP oldDst = (HBITMAP)SelectObject(dstDC, dstBmp);
    if (dstW == srcW && dstH == srcH)
    {
        BitBlt(dstDC, 0, 0, dstW, dstH, srcDC, 0, 0, SRCCOPY);
    }
    else
    {
        SetStretchBltMode(dstDC, HALFTONE);
        StretchBlt(dstDC, 0, 0, dstW, dstH, srcDC, 0, 0, srcW, srcH, SRCCOPY);
    }
    SelectObject(srcDC, oldSrc);
    SelectObject(dstDC, oldDst);
    DeleteDC(srcDC);
    DeleteDC(dstDC);

    /* 通常のBitBlt/StretchBltはアルファチャンネルの意味を理解せず、拡縮の
       過程でアルファ値を保持しない（0にリセットされたり不定値になったり
       する）ことがある。CapturePlayerIconicBitmapで設定したマゼンタ=透明の
       情報がここで失われると、DWM側ではアルファ255(不透明)として扱われ、
       サムネイルの背景がマゼンタ(見た目は紫寄り)に映ってしまう（実際に
       報告された不具合）。コピー後にもう一度同じ基準でアルファを設定し
       直すことで、拡縮後も透明部分が正しく抜けるようにする。 */
    unsigned char *px = (unsigned char *)bits;
    for (int i = 0; i < dstW * dstH; i++)
    {
        unsigned char b = px[i * 4 + 0];
        unsigned char g = px[i * 4 + 1];
        unsigned char r = px[i * 4 + 2];
        int isMagenta = (r == 255 && g == 0 && b == 255);
        px[i * 4 + 3] = (unsigned char)(isMagenta ? 0 : 255);
    }

    return dstBmp;
}

/* WM_DWMSENDICONICTHUMBNAIL応答: タスクバーボタンにマウスを乗せた時の
   小さなサムネイル。lParamで要求された箱に収まるよう、アスペクト比を
   保ったまま縮小する。 */
static void RespondPlayerIconicThumbnail(Player *p, int reqW, int reqH)
{
    if (!p->iconicBitmap || reqW <= 0 || reqH <= 0)
        return;

    BITMAP bmInfo;
    if (!GetObject(p->iconicBitmap, sizeof(bmInfo), &bmInfo) ||
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

    HBITMAP scaled = CopyPlayerBitmapScaled(p->iconicBitmap, bmInfo.bmWidth, bmInfo.bmHeight, dstW, dstH);
    if (scaled)
        DwmSetIconicThumbnail(p->hwnd, scaled, 0);
}

/* WM_DWMSENDICONICLIVEPREVIEWBITMAP応答: タスクバーボタンをクリックした
   時のAero Peekの大きなプレビュー。原寸大のキャプチャをそのまま渡す。 */
static void RespondPlayerIconicLivePreview(Player *p)
{
    if (!p->iconicBitmap)
        return;

    BITMAP bmInfo;
    if (!GetObject(p->iconicBitmap, sizeof(bmInfo), &bmInfo) ||
        bmInfo.bmWidth <= 0 || bmInfo.bmHeight <= 0)
        return;

    HBITMAP copy = CopyPlayerBitmapScaled(p->iconicBitmap, bmInfo.bmWidth, bmInfo.bmHeight, bmInfo.bmWidth, bmInfo.bmHeight);
    if (copy)
        DwmSetIconicLivePreviewBitmap(p->hwnd, copy, NULL, 0);
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
    case WM_DWMSENDICONICTHUMBNAIL:
    {
        Player *p = Player_GetActive();
        if (p)
            RespondPlayerIconicThumbnail(p, LOWORD(lParam), HIWORD(lParam));
        return 0;
    }
    case WM_DWMSENDICONICLIVEPREVIEWBITMAP:
    {
        Player *p = Player_GetActive();
        if (p)
            RespondPlayerIconicLivePreview(p);
        return 0;
    }
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
    p->lastAppliedParentIdx = -1;
    p->vy = 0.0f;
    p->grounded = 0;
    p->facingRight = 1;
    p->parentIdx = -1;
    p->isMinimized = 0;
    p->lastValidParentIdx = -1;
    p->inheritedFlipX = 0;
    p->inheritedFlipY = 0;
    p->minimizeAnimState = 0;
    p->iconicBitmap = NULL;
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
    p->lastAppliedParentIdx = -1;
    p->vy = 0.0f;
    p->grounded = 0;
    p->parentIdx = -1;
    p->isMinimized = 0;
    p->lastValidParentIdx = -1;
    p->inheritedFlipX = 0;
    p->inheritedFlipY = 0;
    p->minimizeAnimState = 0;
    /* 最小化アニメーションの途中でステージがリセットされた場合に備え、
       上書きする前に解放する（GDIリソースリークの防止）。 */
    if (p->iconicBitmap)
    {
        DeleteObject(p->iconicBitmap);
        p->iconicBitmap = NULL;
    }
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

static int PlayerRoundToNearest(float v)
{
    return (v >= 0.0f) ? (int)(v + 0.5f) : (int)(v - 0.5f);
}

void Player_ApplyParentRelativeTransform(Player *p, int windowIndex, RECT newRect)
{
    if (p->parentIdx != windowIndex)
        return;

    /* lastAppliedParentRectがまだこのwindowIndexについて確立されていない
       場合（プレイヤーが初めてこの親に入った、またはリサイズ中の親と
       その子ウィンドウの間を行き来した直後）は、現在のサイズ・位置・
       親矩形をこの場でベースラインとして確立するだけにとどめ、今フレーム
       でのスケール適用は行わない。もしここでジェスチャー開始時点の矩形
       (oldRect)を基準にスケールを適用してしまうと、プレイヤーが既にある
       程度リサイズが進行した状態のウィンドウへ途中から入ってきた場合、
       「ジェスチャー開始時点からずっとそこにいたかのような」倍率が入った
       瞬間に一気に掛かってしまい、サイズ・位置が瞬間的に大きくジャンプ
       してしまう（実際に報告された不具合: あらかじめリサイズしておいた
       ウィンドウにプレイヤーが入ると急激にサイズが変わる）。常に「前回
       この関数を適用した時点」からの差分だけを積み重ねる方式に統一する
       ことで、いつ入ってきても連続的な変化になる。

       以前はこの基準を、まだ一度もこの部屋の床に接地していない（空中で
       入ってきた直後の）状態では確立しなかった（着地するまで保留）。しかし
       これは「空中にいる間ずっとこの部屋のリサイズを一切追従しない」ことを
       意味し、リサイズ中の部屋へジャンプ/落下で入った場合、着地するまでの
       間ずっと部屋の変化から取り残され続け、実際に着地しようとする頃には
       床が既に大きく動いてしまっていて着地に失敗し、そのまますり抜ける
       不具合があった（実際に報告された不具合: 空中でリサイズウィンドウが
       親になってから着地するまでの間、追従が起きていないように感じる）。
       今は空中かどうかに関わらずこの場で基準を確立する -- スケール適用
       自体は次のフレーム以降だけなので「今フレームの一気なジャンプ」は
       起こらず、直後の位置クランプ（下記）で空中の任意位置がnewRectの
       外に出ることも防げるため、着地前に確立しても安全になった。

       windowIndexが既に確立済み（プレイヤーがこの部屋にずっと居続けている）
       であれば、新しいリサイズジェスチャーが始まってもここで基準を作り
       直さない -- 以前はg_resizeGenerationが変わるたびに強制的に再確立
       していたため、ジェスチャー最初の1フレームだけスケールが一切適用
       されず、そのフレームで一気に大きく縮んだ（マウスを素早くドラッグ
       した）場合にプレイヤーだけ取り残されて床をすり抜けてしまう不具合が
       あった（実際に報告された不具合: リサイズウィンドウに入っていると
       プレイヤーがすり抜ける）。lastAppliedParentRectは同じ部屋にいる限り
       常に最新の状態に更新され続けるため、世代をまたいでもそのまま基準
       として使い続けて問題ない。 */
    if (p->lastAppliedParentIdx != windowIndex)
    {
        p->lastAppliedParentIdx = windowIndex;
        p->lastAppliedParentRect = newRect;

        /* HandleWindowTransitionsが親をwindowIndexへ切り替えるのはこの関数の
           呼び出しより後（Strategy_UpdateAllの中でこの関数が呼ばれた時点では
           まだ旧親のまま）なので、基準を確立できる最初の機会は実際に親が
           切り替わってからさらに1フレーム後になる。その間もこの部屋の
           リサイズは進み続けているため、ここで基準を確立する時点で既に
           プレイヤーの現在位置がnewRectの外へ出てしまっていることがある
           （特に素早くドラッグして1フレームの縮小量が大きい場合、または
           空中にいた間ずっと追従されていなかった場合）。はみ出したままだと、
           この直後に走るCheckGroundedが新しい床をtolerance内に見つけられず
           接地判定を得られなくなり、「一度も接地しない → 基準を確立できない
           （旧設計）→ 追従されない → さらにはみ出す」という連鎖で床を
           すり抜けてしまう（実際に報告された不具合）。スケール追従はまだ
           適用しない（それは次のフレーム以降）が、位置だけは親の現在の
           矩形の内側へクランプしておくことで、この連鎖を断ち切る。 */
        int clampW = p->width;
        int clampH = p->height;
        int maxX = newRect.right - clampW;
        int maxY = newRect.bottom - clampH;
        int clampedX = (int)p->x;
        int clampedY = (int)p->y;
        if (clampedX < newRect.left)
            clampedX = newRect.left;
        if (clampedX > maxX)
            clampedX = maxX;
        if (clampedY < newRect.top)
            clampedY = newRect.top;
        if (clampedY > maxY)
            clampedY = maxY;
        p->x = (float)clampedX;
        p->y = (float)clampedY;
        return;
    }

    int lastW = p->lastAppliedParentRect.right - p->lastAppliedParentRect.left;
    int lastH = p->lastAppliedParentRect.bottom - p->lastAppliedParentRect.top;
    if (lastW <= 0 || lastH <= 0)
    {
        p->lastAppliedParentRect = newRect;
        return;
    }

    /* サイズ・位置ともに、前回この関数を適用した時点の親矩形(lastAppliedParentRect)
       からの差分だけを、プレイヤーの「現在の」サイズ・位置（Player_Updateに
       よる歩行移動を既に反映済みかもしれない）に対して適用する。ジェスチャー
       開始時点からの累積再計算にすると、リサイズ中にプレイヤーが歩いた分が
       フレームごとに上書きされて元の相対位置へ戻されてしまう（別途報告
       された不具合）。差分適用にすることで、歩行による移動とリサイズに
       よる相対位置・相対サイズ追従を両立させる。 */
    float stepScaleX = (float)(newRect.right - newRect.left) / (float)lastW;
    float stepScaleY = (float)(newRect.bottom - newRect.top) / (float)lastH;

    /* (int)キャストによる単純な切り捨てだと常に「0方向」へ丸められ、縮小/
       拡大を1フレームごとに積み重ねる差分方式ではこの偏りが毎フレーム
       蓄積する -- 同じウィンドウサイズまで縮小してから元に戻しても、
       プレイヤーが元の大きさに戻らずわずかに小さいまま、という形で顕在化
       する（実際に報告された不具合）。位置の計算と同じPlayerRoundToNearest
       （四捨五入）を使うことで、丸めの偏りを無くし蓄積誤差を防ぐ。 */
    int newW = PlayerRoundToNearest((float)p->width * stepScaleX);
    int newH = PlayerRoundToNearest((float)p->height * stepScaleY);
    if (newW < PLAYER_MIN_SIZE)
        newW = PLAYER_MIN_SIZE;
    if (newH < PLAYER_MIN_SIZE)
        newH = PLAYER_MIN_SIZE;

    int newX = newRect.left + PlayerRoundToNearest((float)((int)p->x - p->lastAppliedParentRect.left) * stepScaleX);
    int newY = newRect.top + PlayerRoundToNearest((float)((int)p->y - p->lastAppliedParentRect.top) * stepScaleY);

    p->lastAppliedParentRect = newRect;
    p->width = newW;
    p->height = newH;
    p->x = (float)newX;
    p->y = (float)newY;

    /* AdjustPositionAfterResize: 新しい境界が親自身の境界に収まらなくなった
       場合、位置をその中にクランプし直す（C#側のフォールバックに合わせ、
       直接の親のCollisionBounds全体に対してのみチェックする）。 */
    RECT parentBounds;
    GetWindowFullBounds(g_windows[windowIndex].hwnd, &parentBounds);

    RECT proposed = {newX, newY, newX + newW, newY + newH};
    if (!IsValidMove(proposed, windowIndex))
    {
        int maxX = parentBounds.right - newW;
        int maxY = parentBounds.bottom - newH;
        int clampedX = newX;
        int clampedY = newY;
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
        /* SWP_NOREDRAW: 位置とサイズが同時に変わるため、これを付けないとOS側が
           SetWindowPosの中で古い内容を新しい位置/サイズへ引き伸ばして即座に
           描画してしまうことがあり、すぐ下の同期的なInvalidateRect+
           UpdateWindowによる正しい描画で1フレームごとに上書きされる形になって
           がくがくして見える（UpdateUnconstrainedの反転時と同じ原因）。 */
        SetWindowPos(p->hwnd, NULL, dx, dy, dw, dh, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
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

/* 制限なしリサイズウィンドウが反転した瞬間に一度だけ呼ぶ。Player_
   ApplyParentRelativeTransformは常に正のスケール比（大きさの比率）だけで
   位置を追従させるため、反転（親の可視矩形の左上そのものが動く/入れ替わる）
   は正しく表現できない -- 反転前に床の上（親矩形の下寄り）にいたプレイヤーは、
   その「開始位置からの下寄り具合」をそのまま新しい矩形にも適用され、結果的に
   新しい矩形でも下寄りの位置、つまり反転で見た目上下端に移動したタイトルバー
   の位置に来てしまう（実際に報告された不具合: タイトルバーへのめり込み、
   かつそこが天井扱いになり常に「落下中」から抜け出せなくなる）。
   ここでは`parentBounds`（反転を反映済みの現在の親矩形）を軸に、プレイヤーの
   位置を該当する軸について正しく鏡映する。 */
void Player_MirrorWithinParent(Player *p, int windowIndex, RECT parentBounds, int mirrorX, int mirrorY)
{
    if (p->parentIdx != windowIndex || (!mirrorX && !mirrorY))
        return;

    RECT pb;
    Player_GetBounds(p, &pb);
    if (mirrorX)
        p->x = (float)(parentBounds.left + parentBounds.right - pb.right);
    if (mirrorY)
        p->y = (float)(parentBounds.top + parentBounds.bottom - pb.bottom);

    /* 次フレームのPlayer_ApplyParentRelativeTransformが、この鏡映による
       ジャンプを「歩行による移動」と誤認して差分適用してしまわないよう、
       追従の基準もこの時点の親矩形に更新しておく。 */
    p->lastAppliedParentRect = parentBounds;
    p->lastAppliedParentIdx = windowIndex;

    if (p->hwnd)
    {
        int dx, dy, dw, dh;
        GetPlayerDisplayRect(p, &dx, &dy, &dw, &dh);
        SetWindowPos(p->hwnd, NULL, dx, dy, dw, dh, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
        InvalidateRect(p->hwnd, NULL, FALSE);
        UpdateWindow(p->hwnd);
    }
}

/* ---- CheckHorizontalCollision / CheckVerticalCollision: NoEntryのみ ---- */

static int SignOf(float v) { return v > 0 ? 1 : (v < 0 ? -1 : 0); }

/* +1 = 通常の重力(下方向)、-1 = 反転(上方向、乗っている制限なしリサイズ
   ウィンドウが上下反転した状態)。inheritedFlipYはHierarchy_ToggleInheritedFlip
   により、実際に祖先が反転した瞬間だけXORで積算される永続フラグなので、
   親から離れても向きはそのまま保たれる。 */
static int GravDir(const Player *p) { return p->inheritedFlipY ? -1 : 1; }

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

static void CheckVerticalCollision(RECT bounds, float *moveY, int *hitCeiling, int gravDir)
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

    /* hitCeilingは「接地面から離れる向き(=ジャンプ方向、重力と逆)の移動が
       ブロックされた」ことを意味する。通常重力(gravDir>0)ではジャンプは
       -Y方向なので元のoriginal<0のケース、反転重力(gravDir<0)ではジャンプは
       +Y方向になるためoriginal>0のケースを見る。 */
    if (gravDir > 0)
    {
        if (original < 0.0f && *moveY > original)
            *hitCeiling = 1;
    }
    else
    {
        if (original > 0.0f && *moveY < original)
            *hitCeiling = 1;
    }
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

static RECT AdjustMovement(RECT oldBounds, RECT target, int parentIdx, int *hitCeiling, int gravDir)
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
                /* ジャンプ方向(接地面から離れる向き)でブロックされた時だけ
                   hitCeilingを立てる。通常重力ではstep<0(上方向)、反転重力では
                   step>0(下方向)がそれにあたる。 */
                if (step == -gravDir)
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

static RECT BoxCollideAgainst(RECT proposed, RECT current, HWND hwnd, int *hitCeiling, int gravDir)
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
    /* ジャンプ方向(接地面から離れる向き)は常に重力と逆。通常重力では上方向
       (このブロックの下側の分岐)、反転重力では下方向(上側の分岐)がそれに
       あたるため、hitCeilingを立てる分岐をgravDirで切り替える。 */
    int jumpDir = -gravDir;

    if (current.bottom <= wb.top && adj.bottom > wb.top)
    {
        adj.top = wb.top - height;
        adj.bottom = wb.top;
        if (hitCeiling && jumpDir > 0)
            *hitCeiling = 1;
    }
    else if (current.top >= wb.bottom && adj.top < wb.bottom)
    {
        adj.top = wb.bottom;
        adj.bottom = wb.bottom + height;
        if (hitCeiling && jumpDir < 0)
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

static RECT HandleWindowCollisions(RECT proposed, RECT current, int *hitCeiling, int gravDir)
{
    RECT adjusted = proposed;
    for (int i = 0; i < g_windowCount; i++)
    {
        GameWindowData *d = &g_windows[i];
        if (d->isNoEntry || IsButtonWindowKind(d->kind) || d->kind == WT_GOAL)
            continue;
        if (!d->hwnd || d->minimized)
            continue;

        adjusted = BoxCollideAgainst(adjusted, current, d->hwnd, hitCeiling, gravDir);
    }
    return adjusted;
}

static RECT HandleButtonCollisions(RECT proposed, RECT current, int *hitCeiling, int gravDir)
{
    RECT adjusted = proposed;
    for (int i = 0; i < g_windowCount; i++)
    {
        GameWindowData *d = &g_windows[i];
        if (!IsButtonWindowKind(d->kind) || !d->hwnd)
            continue;
        adjusted = BoxCollideAgainst(adjusted, current, d->hwnd, hitCeiling, gravDir);
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
        {
            p->parentIdx = covering;
            /* Player_ApplyParentRelativeTransformが使うlastAppliedParentIdx/
               lastAppliedParentRectは「今の親に連続して居続けている間だけ」
               有効な基準 -- 親が変わった（今回のように新しく入った）瞬間に
               無効化しておかないと、以前に別の場所へ移動する前に一度その
               ウィンドウの中で基準を確立したことがある場合、今度そこへ
               戻ってきた時に「既に確立済み」と誤認してしまう。その間その
               ウィンドウはプレイヤー不在のままリサイズされ続けていることが
               あるため、古い（ズレた）基準にいきなり現在のスケールを
               適用してしまい、サイズが一気にジャンプする不具合があった
               （実際に報告された不具合: 他のウィンドウからリサイズ中の
               ウィンドウに入ると急激に大きさが変わる）。 */
            p->lastAppliedParentIdx = -1;
        }
    }
    else
    {
        /* 現在の親が最小化アニメーション中(SetWindowMinimized参照)なら、
           その実ウィンドウ矩形は一時的にMINIMIZE_ANIM_POINT_SIZEの点へ向けて
           縮んでいる最中で、プレイヤーの現在位置を全く含まなくなる。ここで
           毎フレームの再判定をそのまま適用すると、アニメーション完了時に
           Hierarchy_MinimizeSubtreeが実行される前にparentIdxが他へ移って
           しまい、本来一緒に凍結されるべきプレイヤーが最小化を免れてしまう
           （実際に報告された不具合）。アニメーション中は再判定自体をスキップし、
           Hierarchy_MinimizeSubtreeが正しくこのウィンドウを親として見つけて
           Player_OnMinimizeを呼べるようにする。 */
        GameWindowData *curParent = GetWindowData(p->parentIdx);
        if (curParent && curParent->minimizeAnimState != 0)
            return;

        int newWin = WindowQuery_GetTopWindowAt(newBounds, p->parentIdx);
        if (newWin >= 0 && newWin != p->parentIdx)
        {
            p->parentIdx = newWin;
            p->lastAppliedParentIdx = -1; /* 上と同じ理由: 親が切り替わったので基準を無効化 */
        }
        else if (newWin < 0)
        {
            p->parentIdx = -1;
            p->lastAppliedParentIdx = -1;
        }
    }
}

/* GetWindowFullBoundsのTopは実際にはウィンドウ外枠の最上端であり、`w`自身が
   反転していない限りゲーム描画のタイトルバー帯を含む（CLAUDE.md
   CollisionBounds参照）。この辺を「上から乗る天井/床」ではなく「下から
   頭をぶつける天井」として使う場合、外枠の一番上まで潜り込ませると
   タイトルバーに埋まって見える -- タイトルバー自体を天井として、その下端
   で止める必要がある。`w`自身が反転していれば、タイトルバーは既に見た目上
   下端に移動しているため無調整でよい（CheckGroundedInvertedの内側判定用）。 */
static int CeilingContactY(const GameWindowData *w, RECT wb)
{
    int flipX, flipY;
    GameWindow_GetEffectiveFlip(w, &flipX, &flipY);
    int hasChrome = w->kind != WT_GOAL && !IsButtonWindowKind(w->kind);
    return (hasChrome && !flipY) ? (wb.top + TITLE_BAR_HEIGHT) : wb.top;
}

/* CeilingContactYの下端版: `w`の本当の床のY座標。`w`自身が反転している
   場合、タイトルバーは見た目上その底辺(wb.bottom側)に移動しているため、
   そこに足/頭が触れる着地面としてはタイトルバー帯の分だけ手前
   (wb.bottom - TITLE_BAR_HEIGHT)で止める必要がある -- そうしないと
   反転していないプレイヤーが反転した部屋の床に着地した時にタイトルバーへ
   めり込む（CheckGroundedNormalの内側判定）のと、反転したプレイヤーが
   反転していない部屋の天井の下から接触する時（CheckGroundedInvertedの
   外側判定）の両方で使う。`w`自身が反転していなければタイトルバーは
   通常通り上端にあるため、床(wb.bottom)は無調整でよい。 */
static int FloorContactY(const GameWindowData *w, RECT wb)
{
    int flipX, flipY;
    GameWindow_GetEffectiveFlip(w, &flipX, &flipY);
    int hasChrome = w->kind != WT_GOAL && !IsButtonWindowKind(w->kind);
    return (hasChrome && flipY) ? (wb.bottom - TITLE_BAR_HEIGHT) : wb.bottom;
}

/* ---- CheckGroundedNormal: 外側 = ウィンドウの上面 + NoEntry + 画面の下端;
   内側 = 同じくウィンドウの上面（ネストしたプラットフォーム）+ 親自身の
   クライアント下端を部屋の床として扱う。通常重力(GravDir>0)用。 ---- */

static void CheckGroundedNormal(Player *p, float dt)
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
            /* wが上下反転している場合、そのタイトルバーは見た目上wb.bottom側に
               移動しているため、本当の床はwb.bottomそのものではなく
               FloorContactYがタイトルバー帯の分だけ手前に補正した位置になる
               -- そうしないと反転していないプレイヤーが反転した部屋の床に
               着地した時にタイトルバーへめり込んでしまう。 */
            int floorY = FloorContactY(w, wb);
            /* オリジナルのRectangle(x,y,w,h)形式では
               windowGroundArea = (Left, Bottom-maxStep-5, Width, maxStep+10)
               -> bottom = Bottom+5であり、+10ではない。 */
            RECT groundArea = {wb.left, floorY - (int)maxStep - 5, wb.right, floorY + 5};
            if (!RectsOverlap(currentFeetBounds, groundArea))
                continue;

            int groundY = floorY;
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

/* ---- CheckGroundedInverted: CheckGroundedNormalの上下ミラー版。乗っている
   制限なしリサイズウィンドウが上下反転した状態(GravDir<0)のとき、プレイヤーは
   天井に張り付く。全ての「床」判定を「天井」判定に置き換える:
   接地面=障害物の上面(obstacle.top)ではなく下面(obstacle.bottom)、
   接触するのはプレイヤーの足元(bottom)ではなく頭(top)、画面端は下端では
   なく上端(y=0)。CheckGroundedNormalと1対1で対応するよう意図的に並行した
   構造を保っている(ロジックの共有ではなく可読性・保守性を優先)。 ---- */

static void CheckGroundedInverted(Player *p, float dt)
{
    if (p->vy > 0.0f)
    {
        p->grounded = 0;
        return;
    }

    int headX = (int)p->x;
    int headY = (int)p->y + 10;
    int headW = p->width;

    float maxStep = fabsf(p->vy * dt);
    if (maxStep < 20.0f)
        maxStep = 20.0f;

    int sweepBottom = (int)fmaxf((float)headY, headY + p->vy * dt) + 5;
    int sweepTop = headY - GROUND_CHECK_H - 10 - (int)maxStep;
    RECT sweep = {headX, sweepTop, headX + headW, sweepBottom};

    int playerLeft = (int)p->x;
    int playerRight = (int)p->x + p->width;
    int playerTop = (int)p->y;

    for (int i = 0; i < g_noEntryZoneCount; i++)
    {
        RECT z = g_noEntryZones[i];
        if (!RectsOverlap(sweep, z))
            continue;
        if (playerTop <= z.bottom && playerTop >= z.bottom - 5 &&
            playerRight > z.left && playerLeft < z.right)
        {
            p->grounded = 1;
            p->y = (float)z.bottom;
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
        for (int b = 0; b < c; b++)
        {
            RECT band = bnd[b];
            if (!RectsOverlap(sweep, band))
                continue;
            RECT visBand;
            IntersectRect(&visBand, &sweep, &band);
            if (!NoEntry_IsRectVisibleFromWindow(i, visBand))
                continue; /* より前面のウィンドウに隠れている */
            if (playerTop <= band.bottom && playerTop >= band.bottom - 5 &&
                playerRight > band.left && playerLeft < band.right)
            {
                p->grounded = 1;
                p->y = (float)band.bottom;
                p->vy = 0.0f;
                return;
            }
            break;
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
        if (playerTop <= wb.bottom && playerTop >= wb.bottom - 5 &&
            playerRight > wb.left && playerLeft < wb.right)
        {
            p->grounded = 1;
            p->y = (float)wb.bottom;
            p->vy = 0.0f;
            return;
        }
    }

    RECT currentHeadBounds = {headX, headY - GROUND_CHECK_H, headX + headW, headY};
    int idxs[MAX_INTERSECTING];
    int n = GatherIntersectingWindows(sweep, idxs, MAX_INTERSECTING);

    if (p->parentIdx < 0)
    {
        for (int k = 0; k < n; k++)
        {
            GameWindowData *d = &g_windows[idxs[k]];
            if (d->isNoEntry)
                continue;
            RECT wb;
            GetWindowFullBounds(d->hwnd, &wb);
            int contactY = FloorContactY(d, wb);
            if (playerTop > contactY || playerTop < contactY - 5 ||
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
                if (RectsOverlap(ob, currentHeadBounds))
                {
                    isGroundValid = 0;
                    break;
                }
            }
            if (isGroundValid)
            {
                p->grounded = 1;
                p->y = (float)contactY;
                p->vy = 0.0f;
                return;
            }
        }
    }
    else
    {
        int bestTop = INT_MIN;
        for (int k = 0; k < n; k++)
        {
            GameWindowData *w = &g_windows[idxs[k]];
            if (w->isNoEntry)
                continue;
            RECT wb;
            GetWindowFullBounds(w->hwnd, &wb);
            int contactY = CeilingContactY(w, wb);
            RECT ceilingArea = {wb.left, contactY - 5, wb.right, contactY + (int)maxStep + 5};
            if (!RectsOverlap(currentHeadBounds, ceilingArea))
                continue;

            int ceilingY = contactY;
            int isCeilingVisible = 1;
            int myZ = ZOrder_GetIndex(w->hwnd);
            for (int m = 0; m < n; m++)
            {
                GameWindowData *other = &g_windows[idxs[m]];
                if (ZOrder_GetIndex(other->hwnd) <= myZ)
                    continue;
                RECT ob;
                GetWindowFullBounds(other->hwnd, &ob);
                RECT ceilBandArea = {headX, ceilingY - 2, headX + headW, ceilingY + 2};
                int contains = ob.left <= ceilBandArea.left && ob.top <= ceilBandArea.top &&
                               ob.right >= ceilBandArea.right && ob.bottom >= ceilBandArea.bottom;
                int overlapsAndBelow = RectsOverlap(ob, ceilBandArea) && ob.top > ceilingY;
                if (contains || overlapsAndBelow)
                {
                    isCeilingVisible = 0;
                    break;
                }
            }
            if (isCeilingVisible && ceilingY > bestTop)
                bestTop = ceilingY;
        }

        if (bestTop != INT_MIN)
        {
            p->grounded = 1;
            p->y = (float)bestTop;
            p->vy = 0.0f;
            return;
        }
    }

    if (p->parentIdx < 0)
    {
        if (playerTop <= 0)
        {
            p->grounded = 1;
            p->y = 0.0f;
            p->vy = 0.0f;
            return;
        }
    }

    p->grounded = 0;
}

static void CheckGrounded(Player *p, float dt)
{
    if (GravDir(p) > 0)
        CheckGroundedNormal(p, dt);
    else
        CheckGroundedInverted(p, dt);
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

/* GameWindow.cのMinimizeAnimPointRect/LerpIntと同じ考え方をプレイヤーにも
   適用する（GameWindowData.minimizeAnimState等と同じ仕組み）。定数は
   gamewindow.hのMINIMIZE_ANIM_DURATION/MINIMIZE_ANIM_POINT_SIZEをそのまま
   使い、ウィンドウの縮小アニメーションと同じ速さ・同じ最終サイズで
   タイミングが揃うようにする。 */
static RECT PlayerMinimizeAnimPointRect(int cx, int cy)
{
    int h = MINIMIZE_ANIM_POINT_SIZE / 2;
    RECT r = {cx - h, cy - h, cx + h, cy + h};
    return r;
}

static int PlayerLerpInt(int a, int b, float t)
{
    return a + (int)((b - a) * t);
}

void Player_StartMinimizeAnim(Player *p)
{
    if (p->isMinimized || p->minimizeAnimState != 0 || !p->hwnd)
        return;

    /* 通常の物理演算/移動更新をここで即座に停止する（Player_Update先頭の
       ガード）。乗っているウィンドウ側の縮小アニメーションが完了する
       （実際に非表示になる）まで数百msあり、その間も重力・接地判定が
       働き続けると、縮小中で不安定な床の上から落下してしまっていた
       （実際に報告された不具合）。parentIdxの記録や実際のウィンドウの
       非表示化はまだ行わない -- それらはアニメーション完了時に呼ばれる
       Player_OnMinimizeが引き続き正しい親を記録できるよう、そのタイミング
       まで据え置く。 */
    p->isMinimized = 1;

    /* 縮小アニメーションで実際に小さくする前に、フルサイズの見た目を1回
       だけキャプチャしてDWMへ渡す準備をする（GameWindow.cの
       CaptureIconicBitmapと同じ理由: 数px四方まで縮んだ後の内容が
       タスクバーサムネイルに映ってしまう不具合を防ぐ）。 */
    CapturePlayerIconicBitmap(p);
    BOOL trueVal = TRUE;
    DwmSetWindowAttribute(p->hwnd, DWMWA_HAS_ICONIC_BITMAP, &trueVal, sizeof(trueVal));
    DwmSetWindowAttribute(p->hwnd, DWMWA_FORCE_ICONIC_REPRESENTATION, &trueVal, sizeof(trueVal));
    DwmInvalidateIconicBitmaps(p->hwnd);

    GetWindowRect(p->hwnd, &p->minimizeAnimFrom);
    int cx = (p->minimizeAnimFrom.left + p->minimizeAnimFrom.right) / 2;
    int screenBottom = GetSystemMetrics(SM_CYSCREEN);
    p->minimizeAnimTo = PlayerMinimizeAnimPointRect(cx, screenBottom);
    p->minimizeAnimT = 0.0f;
    p->minimizeAnimState = 1;
}

void Player_UpdateMinimizeAnim(Player *p, float dt)
{
    if (p->minimizeAnimState == 0 || !p->hwnd)
        return;

    p->minimizeAnimT += dt / MINIMIZE_ANIM_DURATION;
    float t = p->minimizeAnimT;
    if (t > 1.0f)
        t = 1.0f;

    RECT from = p->minimizeAnimFrom;
    RECT to = p->minimizeAnimTo;
    int x = PlayerLerpInt(from.left, to.left, t);
    int y = PlayerLerpInt(from.top, to.top, t);
    int w = PlayerLerpInt(from.right - from.left, to.right - to.left, t);
    int h = PlayerLerpInt(from.bottom - from.top, to.bottom - to.top, t);
    SetWindowPos(p->hwnd, NULL, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    InvalidateRect(p->hwnd, NULL, FALSE);
    /* InvalidateRectだけでは再描画が次のメッセージループパスまで遅延され、
       新しく露出した領域が未初期化の黒いままになって見えてしまうことが
       ある（GameWindow.cのMinimizeAnim_UpdateAllと同じ理由）。 */
    UpdateWindow(p->hwnd);

    if (p->minimizeAnimT >= 1.0f)
        p->minimizeAnimState = 0;
}

void Player_OnMinimize(Player *p)
{
    p->isMinimized = 1;
    p->lastValidParentIdx = p->parentIdx;
    p->parentIdx = -1;
    p->lastAppliedParentIdx = -1; /* HandleWindowTransitionsと同じ理由: 親が変わるので基準を無効化 */

    /* PlayerForm.OnMinimizeはWindowState = FormWindowState.Minimizedを
       設定する -- 内部フラグだけではなく、実際のWin32の最小化（非表示になり
       タスクバーボタンとして表示される）。これがないと、物理演算は既に
       停止しているのにプレイヤーは完全に表示されたままドラッグ可能な
       ように見えてしまう。
       SW_SHOWMINIMIZEDではなくSW_MINIMIZEを使う: 前者はウィンドウを
       アクティブ化した上で最小化するため、最小化後もプレイヤー自身の
       ウィンドウがキーボードフォーカスを持ったままになり、その状態で
       移動キー（方向キー相当）を押すとWindows標準のアイコンナビゲーション
       処理が働いて警告音が鳴ってしまう（実際に報告された不具合）。
       SW_MINIMIZEはアクティブ化せずZオーダー上の次のウィンドウへ
       フォーカスを譲るため、この副作用が起きない。

       この時点でウィンドウの実際のサイズは、直前のPlayer_UpdateMinimizeAnim
       で縮小しきった小さいサイズになっている。そのままSW_MINIMIZEすると、
       WindowsのWINDOWPLACEMENTにその小さいサイズが「復元先」として記憶
       されてしまい、後でPlayer_OnRestoreでSW_RESTOREした際に一旦その
       小さいサイズで復元されてから、次のPlayer_Updateで正しいフルサイズへ
       一気にジャンプすることになり、その際に新しく露出した領域が未初期化の
       黒いままになって見えてしまう（実際に報告された不具合、GameWindow.cの
       SetWindowMinimizedで既に修正済みのものと同じ原因）。
       SW_HIDEで一旦非表示にしてから正しいフルサイズ（GetPlayerDisplayRect）
       へ位置・サイズを更新し、それからSW_MINIMIZEすることで、
       WINDOWPLACEMENTには常に正しいフルサイズが記録されるようにする。
       SWP_NOREDRAWだけでは不十分（DWMがGDIの再描画抑制とは無関係に実際の
       合成サーフェスを移動/リサイズしてしまう）なため、SW_HIDEによる
       非表示化が必須。 */
    if (p->hwnd)
    {
        ShowWindow(p->hwnd, SW_HIDE);
        int dx, dy, dw, dh;
        GetPlayerDisplayRect(p, &dx, &dy, &dw, &dh);
        SetWindowPos(p->hwnd, NULL, dx, dy, dw, dh, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
        ShowWindow(p->hwnd, SW_MINIMIZE);
    }
}

void Player_OnRestore(Player *p)
{
    p->isMinimized = 0;
    p->minimizeAnimState = 0;
    p->lastAppliedParentIdx = -1; /* HandleWindowTransitionsと同じ理由: 親が変わるので基準を無効化 */

    if (p->hwnd)
    {
        ShowWindow(p->hwnd, SW_RESTORE);
        /* 通常の描画に戻すため、強制アイコン表現を解除しキャプチャした
           ビットマップを解放する（次に最小化される時にCapturePlayerIconicBitmap
           で改めて撮り直す。GameWindow.cのSetWindowMinimized復元処理と同じ）。 */
        BOOL falseVal = FALSE;
        DwmSetWindowAttribute(p->hwnd, DWMWA_FORCE_ICONIC_REPRESENTATION, &falseVal, sizeof(falseVal));
    }
    if (p->iconicBitmap)
    {
        DeleteObject(p->iconicBitmap);
        p->iconicBitmap = NULL;
    }

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
    int gravDir = GravDir(p);

    /* PlayerInputHandler.UpdateFacingは左を先にチェックして即座にreturnするため、
       両方向が同時に押されている場合は左が優先される。 */
    if (GetAsyncKeyState('A') & 0x8000 || GetAsyncKeyState(VK_LEFT) & 0x8000)
        p->facingRight = 0;
    else if (GetAsyncKeyState('D') & 0x8000 || GetAsyncKeyState(VK_RIGHT) & 0x8000)
        p->facingRight = 1;

    if (p->grounded &&
        (GetAsyncKeyState(VK_SPACE) & 0x8000 || GetAsyncKeyState(VK_UP) & 0x8000 || GetAsyncKeyState('W') & 0x8000))
    {
        /* ジャンプは常に接地面から離れる向き = 重力と逆方向。 */
        p->vy = -JUMP_FORCE * gravDir;
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
    CheckVerticalCollision(current, &moveY, &hitCeiling, gravDir);
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
        proposed = AdjustMovement(current, proposed, p->parentIdx, &ceil2, gravDir);
        if (ceil2)
        {
            p->vy = 0.0f;
            Anim_ResetScale(&p->anim);
        }
    }

    if (p->parentIdx < 0)
    {
        int ceil3 = 0;
        proposed = HandleWindowCollisions(proposed, current, &ceil3, gravDir);
        if (ceil3)
        {
            p->vy = 0.0f;
            Anim_ResetScale(&p->anim);
        }
    }

    int ceil4 = 0;
    proposed = HandleButtonCollisions(proposed, current, &ceil4, gravDir);
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
        p->vy += GRAVITY * dt * gravDir;
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
