#include "desktopicon.h"
#include "stage.h"
#ifdef ENABLE_STAGE_EDITOR
#include "editor.h"
#endif
#include <commctrl.h>

/* ---- 可視化オーバーレイ: アイコン1個につき1枚のマーカーウィンドウを
   作っていたが、タスクバーに表示する必要が無いにもかかわらずアイコンの
   数だけタスクバーボタンが並んでしまっていた（実際に報告された不具合）。
   個別ウィンドウにする理由も無いため、仮想画面全体を覆う透明・クリック
   スルー・常に最前面の1枚のレイヤードウィンドウに統合し、そのWM_PAINTで
   全アイコンの矩形枠をまとめて描画する。WS_EX_TOOLWINDOWでタスクバー/
   Alt+Tabからも確実に除外する。 ---- */

static const char *kOverlayWindowClass = "WA_DesktopIconOverlay";
static HWND g_overlayHwnd = NULL;
static RECT g_icons[MAX_DESKTOP_ICONS];
static int g_iconCount = 0;

static void PaintOverlay(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);

    /* マゼンタでまず塗りつぶし、LWA_COLORKEYで透過させる（枠線の内側は
       完全に透明になり、アイコンの見た目自体を隠さない）。 */
    HBRUSH bg = CreateSolidBrush(RGB(255, 0, 255));
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);

    if (g_iconCount > 0)
    {
        HPEN pen = CreatePen(PS_SOLID, 3, RGB(60, 220, 60));
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

        /* g_iconsはスクリーン座標だが、ウィンドウ自体が仮想画面の原点
           (負値になり得るマルチモニタ座標)へ配置されているため、その原点
           分を引いてクライアント座標に変換してから描画する。 */
        int ox = GetSystemMetrics(SM_XVIRTUALSCREEN);
        int oy = GetSystemMetrics(SM_YVIRTUALSCREEN);
        for (int i = 0; i < g_iconCount; i++)
        {
            RECT r = g_icons[i];
            Rectangle(hdc, r.left - ox, r.top - oy, r.right - ox, r.bottom - oy);
        }

        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);
    }

    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK OverlayWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_PAINT:
        PaintOverlay(hwnd);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

void DesktopIcon_RegisterWindowClass(HINSTANCE hInstance)
{
    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = OverlayWindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = kOverlayWindowClass;
    wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.hbrBackground = NULL;
    RegisterClassA(&wc);

    int ox = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int oy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int cx = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int cy = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    g_overlayHwnd = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kOverlayWindowClass, "DesktopIconOverlay",
        WS_POPUP | WS_VISIBLE,
        ox, oy, cx, cy,
        NULL, NULL, hInstance, NULL);
    if (g_overlayHwnd)
    {
        SetLayeredWindowAttributes(g_overlayHwnd, RGB(255, 0, 255), 0, LWA_COLORKEY);
        SetWindowPos(g_overlayHwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void DesktopIcon_Clear(void)
{
    g_iconCount = 0;
    if (g_overlayHwnd)
        InvalidateRect(g_overlayHwnd, NULL, TRUE);
}

int DesktopIcon_Count(void) { return g_iconCount; }

RECT DesktopIcon_GetBounds(int index)
{
    RECT empty = {0, 0, 0, 0};
    if (index < 0 || index >= g_iconCount)
        return empty;
    return g_icons[index];
}

/* ---- デスクトップのSysListView32探索。DesktopIconManager.
   GetDesktopListViewHandle/TryAlternativeDesktopSearchと一致。 ---- */

static BOOL CALLBACK FindWorkerWListViewProc(HWND hwnd, LPARAM lParam)
{
    char cls[64];
    GetClassNameA(hwnd, cls, sizeof(cls));
    if (lstrcmpA(cls, "WorkerW") != 0)
        return TRUE; /* 次のウィンドウへ */

    HWND defView = FindWindowExA(hwnd, NULL, "SHELLDLL_DefView", NULL);
    if (!defView)
        return TRUE;

    HWND listView = FindWindowExA(defView, NULL, "SysListView32", NULL);
    if (!listView)
        return TRUE;

    *(HWND *)lParam = listView;
    return FALSE; /* 見つかったので列挙を止める */
}

static HWND FindDesktopListView(void)
{
    HWND progman = FindWindowA("Progman", "Program Manager");
    HWND defView = progman ? FindWindowExA(progman, NULL, "SHELLDLL_DefView", NULL) : NULL;
    HWND listView = defView ? FindWindowExA(defView, NULL, "SysListView32", "FolderView") : NULL;
    if (!listView && defView)
        listView = FindWindowExA(defView, NULL, "SysListView32", NULL);
    if (listView)
        return listView;

    /* Windows 10/11では、SHELLDLL_DefViewがProgmanの直接の子ではなく、
       別途生成される「WorkerW」ウィンドウの子になっていることがある。 */
    listView = NULL;
    EnumWindows(FindWorkerWListViewProc, (LPARAM)&listView);
    return listView;
}

/* LVM_GETITEMRECTはクロスプロセス呼び出しのため、渡すRECTは呼び出し先
   （デスクトップのSysListView32を所有するexplorer.exeプロセス）の
   アドレス空間に存在する必要がある -- VirtualAllocEx/WriteProcessMemory/
   ReadProcessMemoryでリモートメモリを介して読み書きする
   （DesktopIconManager.GetItemClickableBoundsと同じ手法）。RECT.leftに
   問い合わせたい基準(LVIR_ICON等)を書き込んでから送ると、その基準での
   矩形がクライアント座標で返る。 */
static int GetItemIconRectScreen(HWND listView, HANDLE hProcess, LPVOID remoteRect, int index, RECT *outScreen)
{
    RECT req = {LVIR_ICON, 0, 0, 0};
    if (!WriteProcessMemory(hProcess, remoteRect, &req, sizeof(req), NULL))
        return 0;
    if (!SendMessageA(listView, LVM_GETITEMRECT, (WPARAM)index, (LPARAM)remoteRect))
        return 0;

    RECT client;
    if (!ReadProcessMemory(hProcess, remoteRect, &client, sizeof(client), NULL))
        return 0;
    if (client.right <= client.left || client.bottom <= client.top)
        return 0;

    POINT topLeft = {client.left, client.top};
    POINT bottomRight = {client.right, client.bottom};
    ClientToScreen(listView, &topLeft);
    ClientToScreen(listView, &bottomRight);

    outScreen->left = topLeft.x;
    outScreen->top = topLeft.y;
    outScreen->right = bottomRight.x;
    outScreen->bottom = bottomRight.y;
    return 1;
}

void DesktopIcon_Refresh(HINSTANCE hInstance)
{
    DesktopIcon_Clear();

    HWND listView = FindDesktopListView();
    if (!listView)
        return;

    DWORD pid = 0;
    GetWindowThreadProcessId(listView, &pid);
    if (pid == 0)
        return;

    HANDLE hProcess = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, pid);
    if (!hProcess)
        return;

    int itemCount = (int)SendMessageA(listView, LVM_GETITEMCOUNT, 0, 0);
    if (itemCount > MAX_DESKTOP_ICONS)
        itemCount = MAX_DESKTOP_ICONS;

    if (itemCount > 0)
    {
        LPVOID remoteRect = VirtualAllocEx(hProcess, NULL, sizeof(RECT), MEM_COMMIT, PAGE_READWRITE);
        if (remoteRect)
        {
            for (int i = 0; i < itemCount; i++)
            {
                RECT screen;
                if (GetItemIconRectScreen(listView, hProcess, remoteRect, i, &screen))
                    g_icons[g_iconCount++] = screen;
            }
            VirtualFreeEx(hProcess, remoteRect, 0, MEM_RELEASE);
        }
    }

    CloseHandle(hProcess);

    if (g_overlayHwnd)
    {
        SetWindowPos(g_overlayHwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        InvalidateRect(g_overlayHwnd, NULL, TRUE);
    }
}

int DesktopIcon_IsActiveForCurrentStage(void)
{
#ifdef ENABLE_STAGE_EDITOR
    return Stage_DesktopIconsEnabled() || Editor_IsTestStage();
#else
    return Stage_DesktopIconsEnabled();
#endif
}

/* ---- イベント駆動の変更検出。DesktopIconManager.cs / RegistryIconPositionWatcher.cs
   と同じ2系統をそのまま移植する:
   1) SHChangeNotifyRegister -- 作成/削除/名前変更/更新を、専用の非表示
      メッセージ専用ウィンドウ経由でWM_SHNOTIFYとして受け取る。
   2) レジストリ監視スレッド -- アイコンの「移動」はShell変更通知が飛んで
      来ないため、位置情報が書き込まれるHKCU\...\Shell\Bagsを
      RegNotifyChangeKeyValueで別スレッド監視し、500ms分デバウンスする。
   どちらもメインスレッドの共有フラグ(g_shellDirty/g_registryDirty)を
   InterlockedExchangeで立てるだけに留め、実際のDesktopIcon_Refresh
   （クロスプロセスAPI呼び出しを伴う）はDesktopIcon_UpdateEvents経由で
   必ずメインループのスレッドから行う。 ---- */

typedef struct
{
    const void *pidl;
    BOOL fRecursive;
} SHChangeNotifyEntry;

typedef ULONG(WINAPI *PFN_SHChangeNotifyRegister)(HWND, int, LONG, UINT, int, const SHChangeNotifyEntry *);

#define SHCNRF_InterruptLevel 0x0001
#define SHCNRF_ShellLevel 0x0002
#define SHCNE_RENAMEITEM 0x00000001L
#define SHCNE_CREATE 0x00000002L
#define SHCNE_DELETE 0x00000004L
#define SHCNE_UPDATEDIR 0x00001000L
#define SHCNE_UPDATEITEM 0x00002000L
#define WM_SHNOTIFY (WM_USER + 1)

static const char *kNotifyWindowClass = "WA_DesktopIconNotify";
static HWND g_notifyHwnd = NULL;
static ULONG g_shellNotifyId = 0;
static volatile LONG g_shellDirty = 0;
static volatile LONG g_registryDirty = 0;

static LRESULT CALLBACK NotifyWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_SHNOTIFY)
    {
        InterlockedExchange(&g_shellDirty, 1);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

/* SHChangeNotifyRegisterはヘッダ宣言の無い内部APIのため
   （DesktopIconManager.csがDllImportで直接宣言しているのと同じ理由）、
   shell32.dllから名前で解決し、失敗時のみオーディナル2へフォール
   バックする（一部の古いWindowsビルドでは名前エクスポートが無い）。
   対応するSHChangeNotifyDeregisterは、この登録を明示的に解除する
   タイミングが無い（プロセス終了までそのまま有効にしておく設計、
   InitShellChangeNotify関数コメント参照）ため解決していない。 */
static PFN_SHChangeNotifyRegister ResolveShChangeNotifyRegister(void)
{
    HMODULE h = GetModuleHandleA("shell32.dll");
    if (!h)
        h = LoadLibraryA("shell32.dll");
    if (!h)
        return NULL;

    PFN_SHChangeNotifyRegister p = (PFN_SHChangeNotifyRegister)GetProcAddress(h, "SHChangeNotifyRegister");
    if (!p)
        p = (PFN_SHChangeNotifyRegister)GetProcAddress(h, (LPCSTR)2);
    return p;
}

static void InitShellChangeNotify(HINSTANCE hInstance)
{
    PFN_SHChangeNotifyRegister pRegister = ResolveShChangeNotifyRegister();
    if (!pRegister)
        return;

    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = NotifyWindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = kNotifyWindowClass;
    RegisterClassA(&wc);

    /* HWND_MESSAGEのメッセージ専用ウィンドウ -- 画面には一切表示されず、
       PostMessageで届くWM_SHNOTIFYを受け取るためだけに存在する。
       DesktopIcon_Refreshのたびに内容を再描画するだけの可視化オーバーレイ
       (g_overlayHwnd)とは別物で、プロセス生存中ずっと同じハンドルのまま
       存在し続ける必要がある（SHChangeNotifyRegisterに渡した後にウィンドウ
       を破棄すると登録自体が無効になるため）。 */
    g_notifyHwnd = CreateWindowExA(0, kNotifyWindowClass, "", 0, 0, 0, 0, 0,
                                    HWND_MESSAGE, NULL, hInstance, NULL);
    if (!g_notifyHwnd)
        return;

    SHChangeNotifyEntry entry = {NULL, FALSE}; /* pidl=NULL: デスクトップ全体を監視 */
    LONG eventMask = SHCNE_CREATE | SHCNE_DELETE | SHCNE_RENAMEITEM | SHCNE_UPDATEITEM | SHCNE_UPDATEDIR;
    g_shellNotifyId = pRegister(g_notifyHwnd, SHCNRF_ShellLevel | SHCNRF_InterruptLevel,
                                 eventMask, WM_SHNOTIFY, 1, &entry);
}

#define REGISTRY_WATCH_PATH "Software\\Microsoft\\Windows\\Shell\\Bags"
#define REGISTRY_DEBOUNCE_MS 500

static DWORD WINAPI RegistryWatcherThreadProc(LPVOID param)
{
    (void)param;
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, REGISTRY_WATCH_PATH, 0, KEY_NOTIFY, &hKey) != ERROR_SUCCESS)
        return 0;

    HANDLE hEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (!hEvent)
    {
        RegCloseKey(hKey);
        return 0;
    }

    /* このスレッドはプロセス終了まで動き続ける常駐監視スレッド --
       RegistryIconPositionWatcher.csと異なり明示的なStopWatchingは
       設けていない。ゲーム終了時はプロセスごと終了するため、Win32が
       スレッド/レジストリハンドルを自動的に回収する（ゲーム単体exeの
       ためのシンプルな設計、ライブラリではないのでKISS優先）。 */
    for (;;)
    {
        if (RegNotifyChangeKeyValue(hKey, TRUE, REG_NOTIFY_CHANGE_LAST_SET, hEvent, TRUE) != ERROR_SUCCESS)
            break;
        if (WaitForSingleObject(hEvent, INFINITE) != WAIT_OBJECT_0)
            break;
        Sleep(REGISTRY_DEBOUNCE_MS);
        InterlockedExchange(&g_registryDirty, 1);
    }

    CloseHandle(hEvent);
    RegCloseKey(hKey);
    return 0;
}

void DesktopIcon_InitEventWatchers(HINSTANCE hInstance)
{
    InitShellChangeNotify(hInstance);
    CreateThread(NULL, 0, RegistryWatcherThreadProc, NULL, 0, NULL);
}

void DesktopIcon_UpdateEvents(HINSTANCE hInstance)
{
    if (!DesktopIcon_IsActiveForCurrentStage())
    {
        InterlockedExchange(&g_shellDirty, 0);
        InterlockedExchange(&g_registryDirty, 0);
        return;
    }

    LONG shellHit = InterlockedExchange(&g_shellDirty, 0);
    LONG registryHit = InterlockedExchange(&g_registryDirty, 0);
    if (shellHit || registryHit)
        DesktopIcon_Refresh(hInstance);
}
