using System;
using System.Collections.Generic;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using MultiWindowActionGame.Core;
using MultiWindowActionGame.Utilities;

namespace MultiWindowActionGame.Managers
{
    public struct DesktopIcon
    {
        public Rectangle Bounds { get; set; }
        public Rectangle CollisionBounds { get; set; }
        public string Name { get; set; }
        public IntPtr IconHandle { get; set; }
        public bool IsValid { get; set; }

        public DesktopIcon(Rectangle bounds, string name, IntPtr iconHandle, Rectangle? collisionBounds = null)
        {
            Bounds = bounds;
            CollisionBounds = collisionBounds ?? bounds;
            Name = name ?? string.Empty;
            IconHandle = iconHandle;
            IsValid = true;
        }
    }

    public interface IDesktopIconManager
    {
        List<DesktopIcon> GetDesktopIcons();
        void RefreshIcons();
        Task RefreshIconsAsync();
        bool IsInitialized { get; }
        void InitializeChangeNotification(Form targetForm);
    }

    public class DesktopIconManager : IDesktopIconManager
    {
        private readonly ILogger logger;
        private readonly IErrorHandler errorHandler;
        private List<DesktopIcon> cachedIcons = new List<DesktopIcon>();
        private bool isInitialized = false;

        // 変更監視システム
        private uint changeNotifyId = 0;
        private readonly object cacheLock = new object();

        // 差分検出システム
        private Dictionary<string, int> iconHashCache = new Dictionary<string, int>();

        // 非同期処理用
        private Task? refreshTask = null;
        private readonly CancellationTokenSource cancellationTokenSource = new CancellationTokenSource();

        // レジストリ監視システム
        private RegistryIconPositionWatcher? registryWatcher;

        // Win32 API定数
        private const int LVM_FIRST = 0x1000;
        private const int LVM_GETITEMCOUNT = LVM_FIRST + 4;
        private const int LVM_GETITEMPOSITION = LVM_FIRST + 16;
        private const int LVM_GETITEMTEXT = LVM_FIRST + 45;
        private const int LVIF_TEXT = 0x0001;
        private const int PROCESS_VM_OPERATION = 0x0008;
        private const int PROCESS_VM_READ = 0x0010;
        private const int PROCESS_VM_WRITE = 0x0020;
        private const int MEM_COMMIT = 0x1000;
        private const int MEM_RELEASE = 0x8000;
        private const int PAGE_READWRITE = 0x04;

        [StructLayout(LayoutKind.Sequential)]
        private struct POINT
        {
            public int X;
            public int Y;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct LVITEM
        {
            public uint mask;
            public int iItem;
            public int iSubItem;
            public uint state;
            public uint stateMask;
            public IntPtr pszText;
            public int cchTextMax;
            public int iImage;
            public IntPtr lParam;
        }

        // Win32 API宣言
        [DllImport("user32.dll", SetLastError = true)]
        private static extern IntPtr FindWindow(string lpClassName, string lpWindowName);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern IntPtr FindWindowEx(IntPtr parentHandle, IntPtr childAfter, string className, string windowTitle);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern IntPtr OpenProcess(uint dwDesiredAccess, bool bInheritHandle, uint dwProcessId);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern IntPtr VirtualAllocEx(IntPtr hProcess, IntPtr lpAddress, uint dwSize, uint flAllocationType, uint flProtect);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool VirtualFreeEx(IntPtr hProcess, IntPtr lpAddress, uint dwSize, uint dwFreeType);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool ReadProcessMemory(IntPtr hProcess, IntPtr lpBaseAddress, [Out] byte[] lpBuffer, int dwSize, out IntPtr lpNumberOfBytesRead);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool WriteProcessMemory(IntPtr hProcess, IntPtr lpBaseAddress, byte[] lpBuffer, uint nSize, out IntPtr lpNumberOfBytesWritten);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool CloseHandle(IntPtr hObject);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern bool ClientToScreen(IntPtr hWnd, ref POINT lpPoint);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

        [StructLayout(LayoutKind.Sequential)]
        private struct RECT
        {
            public int Left;
            public int Top;
            public int Right;
            public int Bottom;
        }

        // 代替検索用のAPI
        [DllImport("user32.dll")]
        private static extern bool EnumWindows(EnumWindowsProc enumProc, IntPtr lParam);

        [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Auto)]
        private static extern int GetClassName(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);

        [DllImport("user32.dll")]
        private static extern int GetSystemMetrics(int nIndex);

        [DllImport("user32.dll")]
        private static extern IntPtr GetDC(IntPtr hWnd);

        [DllImport("gdi32.dll")]
        private static extern int GetDeviceCaps(IntPtr hdc, int nIndex);

        [DllImport("user32.dll")]
        private static extern int ReleaseDC(IntPtr hWnd, IntPtr hDC);

        // システムメトリクス定数
        private const int SM_CXICON = 11; // 大きいアイコンの幅
        private const int SM_CYICON = 12; // 大きいアイコンの高さ
        private const int SM_CXSMICON = 49; // 小さいアイコンの幅
        private const int SM_CYSMICON = 50; // 小さいアイコンの高さ

        // DPI関連定数
        private const int LOGPIXELSX = 88; // X方向の1インチあたりの論理ピクセル数
        private const int LOGPIXELSY = 90; // Y方向の1インチあたりの論理ピクセル数

        // 作業領域取得用API
        [DllImport("user32.dll")]
        private static extern bool SystemParametersInfo(int uAction, int uParam, ref RECT lpvParam, int fuWinIni);
        private const int SPI_GETWORKAREA = 0x0030;

        // アイコンサイズ取得用Win32 API
        [DllImport("shell32.dll")]
        private static extern int SHGetFileInfo(string pszPath, uint dwFileAttributes, ref SHFILEINFO psfi, uint cbFileInfo, uint uFlags);

        [DllImport("user32.dll")]
        private static extern IntPtr ImageList_GetIcon(IntPtr himl, int i, uint flags);

        [DllImport("user32.dll")]
        private static extern bool GetIconInfo(IntPtr hIcon, out ICONINFO piconinfo);

        [DllImport("gdi32.dll")]
        private static extern bool GetObject(IntPtr hgdiobj, int cbBuffer, out BITMAP lpvObject);

        [DllImport("user32.dll")]
        private static extern bool DestroyIcon(IntPtr hIcon);

        // アイコン情報構造体
        [StructLayout(LayoutKind.Sequential)]
        private struct SHFILEINFO
        {
            public IntPtr hIcon;
            public int iIcon;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
            public string szDisplayName;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 80)]
            public string szTypeName;
            public uint dwAttributes;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct ICONINFO
        {
            public bool fIcon;
            public uint xHotspot;
            public uint yHotspot;
            public IntPtr hbmMask;
            public IntPtr hbmColor;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct BITMAP
        {
            public int bmType;
            public int bmWidth;
            public int bmHeight;
            public int bmWidthBytes;
            public short bmPlanes;
            public short bmBitsPixel;
            public IntPtr bmBits;
        }

        // SHGetFileInfo フラグ
        private const uint SHGFI_ICON = 0x100;
        private const uint SHGFI_LARGEICON = 0x0;
        private const uint SHGFI_SMALLICON = 0x1;
        private const uint SHGFI_SYSICONINDEX = 0x4000;

        // ListView関連定数
        private const int LVM_GETITEMRECT = LVM_FIRST + 14;
        private const int LVIR_BOUNDS = 0;
        private const int LVIR_ICON = 1;
        private const int LVIR_LABEL = 2;
        private const int LVIR_SELECTBOUNDS = 3;

        // SHChangeNotify関連定数
        [DllImport("shell32.dll")]
        private static extern uint SHChangeNotifyRegister(IntPtr hwnd, int fSources, uint wEventMask, uint uMsg, int cEntries, ref SHChangeNotifyEntry pshcne);

        [DllImport("shell32.dll")]
        private static extern bool SHChangeNotifyDeregister(uint ulID);

        [StructLayout(LayoutKind.Sequential)]
        private struct SHChangeNotifyEntry
        {
            public IntPtr pidl;
            public bool fRecursive;
        }

        private const int SHCNRF_InterruptLevel = 0x0001;
        private const int SHCNRF_ShellLevel = 0x0002;
        private const int SHCNRF_RecursiveInterrupt = 0x1000;
        private const uint SHCNE_CREATE = 0x00000002;
        private const uint SHCNE_DELETE = 0x00000004;
        private const uint SHCNE_MKDIR = 0x00000008;
        private const uint SHCNE_RMDIR = 0x00000010;
        private const uint SHCNE_RENAMEITEM = 0x00000001;
        private const uint SHCNE_RENAMEFOLDER = 0x00020000;
        private const uint SHCNE_UPDATEITEM = 0x00002000;
        private const uint SHCNE_UPDATEDIR = 0x00001000;
        private const uint WM_SHNOTIFY = 0x0401;

        private delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

        public DesktopIconManager(ILogger logger, IErrorHandler errorHandler)
        {
            this.logger = logger ?? throw new ArgumentNullException(nameof(logger));
            this.errorHandler = errorHandler ?? throw new ArgumentNullException(nameof(errorHandler));

            // レジストリ監視を初期化
            InitializeRegistryWatcher();

            // InitializeChangeNotificationはProgram.cs側から呼び出される
        }

        /// <summary>
        /// デスクトップアイコン変更監視を初期化
        /// </summary>
        /// <param name="targetForm">Shell通知を受信するフォーム</param>
        public void InitializeChangeNotification(Form targetForm)
        {
            try
            {
                if (targetForm == null)
                {
                    logger.LogError("Target form is null, cannot register shell notification");
                    return;
                }

                if (!targetForm.IsHandleCreated)
                {
                    logger.LogWarning("Target form handle not created yet");
                    return;
                }

                var desktopPath = Environment.GetFolderPath(Environment.SpecialFolder.Desktop);
                logger.LogInfo($"Initializing desktop change notification for: {desktopPath}");

                var handle = targetForm.Handle;
                logger.LogInfo($"Using form handle: 0x{handle.ToInt64():X8}");

                // 監視するイベントマスク
                uint eventMask = SHCNE_CREATE | SHCNE_DELETE | SHCNE_RENAMEITEM |
                                SHCNE_UPDATEITEM | SHCNE_UPDATEDIR;

                // 監視対象を設定（デスクトップ全体）
                var entry = new SHChangeNotifyEntry
                {
                    pidl = IntPtr.Zero,  // デスクトップ全体を監視
                    fRecursive = false
                };

                // Shell通知を登録
                changeNotifyId = SHChangeNotifyRegister(
                    handle,
                    SHCNRF_ShellLevel | SHCNRF_InterruptLevel,
                    eventMask,
                    WM_SHNOTIFY,
                    1,
                    ref entry
                );

                if (changeNotifyId == 0)
                {
                    var errorCode = Marshal.GetLastWin32Error();
                    logger.LogError($"Failed to register shell notification (Error code: {errorCode})");
                }
                else
                {
                    logger.LogInfo($"Desktop change notification registered successfully (ID: {changeNotifyId})");
                }
            }
            catch (Exception ex)
            {
                logger.LogError($"Failed to initialize change notification: {ex.Message}");
                logger.LogError($"Stack trace: {ex.StackTrace}");
            }
        }

        /// <summary>
        /// 変更監視を停止
        /// </summary>
        private void StopChangeNotification()
        {
            if (changeNotifyId != 0)
            {
                SHChangeNotifyDeregister(changeNotifyId);
                changeNotifyId = 0;
                logger.LogInfo("Desktop change notification stopped");
            }
        }

        /// <summary>
        /// レジストリ監視を初期化
        /// </summary>
        private void InitializeRegistryWatcher()
        {
            try
            {
                registryWatcher = new RegistryIconPositionWatcher(
                    logger,
                    OnRegistryPositionChanged
                );

                registryWatcher.StartWatching();
                logger.LogInfo("Registry position watcher initialized");
            }
            catch (Exception ex)
            {
                logger.LogError($"Failed to initialize registry watcher: {ex.Message}");
                registryWatcher = null; // フォールバック: Shell通知のみ
            }
        }

        /// <summary>
        /// レジストリ変更検知時のコールバック
        /// </summary>
        private void OnRegistryPositionChanged()
        {
            try
            {
                logger.LogInfo("Registry position change detected, refreshing icons...");
                Task.Run(() => RefreshIconsAsync());
            }
            catch (Exception ex)
            {
                logger.LogError($"Error handling registry position change: {ex.Message}");
            }
        }

        /// <summary>
        /// アイコンのハッシュ値を計算（位置とファイル情報から）
        /// </summary>
        private int CalculateIconHash(DesktopIcon icon)
        {
            unchecked
            {
                int hash = 17;
                hash = hash * 31 + icon.Name?.GetHashCode() ?? 0;
                hash = hash * 31 + icon.Bounds.X;
                hash = hash * 31 + icon.Bounds.Y;
                hash = hash * 31 + icon.Bounds.Width;
                hash = hash * 31 + icon.Bounds.Height;
                return hash;
            }
        }

        /// <summary>
        /// アイコンリストの変更を検出
        /// </summary>
        private bool HasIconsChanged(List<DesktopIcon> newIcons)
        {
            try
            {
                var newHashMap = new Dictionary<string, int>();

                // 新しいアイコンのハッシュを計算
                foreach (var icon in newIcons)
                {
                    var key = icon.Name ?? $"Icon_{icon.Bounds.X}_{icon.Bounds.Y}";
                    newHashMap[key] = CalculateIconHash(icon);
                }

                // 数が違う場合は変更あり
                if (newHashMap.Count != iconHashCache.Count)
                {
                    logger.LogInfo($"Icon count changed: {iconHashCache.Count} -> {newHashMap.Count}");
                    return true;
                }

                // ハッシュ値を比較
                foreach (var kvp in newHashMap)
                {
                    if (!iconHashCache.TryGetValue(kvp.Key, out int oldHash) || oldHash != kvp.Value)
                    {
                        logger.LogInfo($"Icon changed: {kvp.Key}");
                        return true;
                    }
                }

                return false;
            }
            catch (Exception ex)
            {
                logger.LogError($"Error checking icon changes: {ex.Message}");
                return true; // エラー時は変更ありとみなして更新
            }
        }

        /// <summary>
        /// アイコンハッシュキャッシュを更新
        /// </summary>
        private void UpdateIconHashCache(List<DesktopIcon> icons)
        {
            try
            {
                iconHashCache.Clear();
                foreach (var icon in icons)
                {
                    var key = icon.Name ?? $"Icon_{icon.Bounds.X}_{icon.Bounds.Y}";
                    iconHashCache[key] = CalculateIconHash(icon);
                }
            }
            catch (Exception ex)
            {
                logger.LogError($"Error updating icon hash cache: {ex.Message}");
            }
        }

        /// <summary>
        /// 実際のシステムアイコンサイズ設定を取得
        /// </summary>
        /// <returns>アイコンのサイズ（幅、高さ）</returns>
        private (int width, int height) GetActualDesktopIconSize()
        {
            try
            {
                // 標準的なファイルアイコンのサイズを取得してシステム設定を推定
                var shfi = new SHFILEINFO();
                var result = SHGetFileInfo("dummy.txt", 0, ref shfi, (uint)Marshal.SizeOf(shfi),
                    SHGFI_ICON | SHGFI_LARGEICON);

                if (result != 0 && shfi.hIcon != IntPtr.Zero)
                {
                    try
                    {
                        // アイコンの実際のサイズを取得
                        if (GetIconInfo(shfi.hIcon, out ICONINFO iconInfo))
                        {
                            if (iconInfo.hbmColor != IntPtr.Zero)
                            {
                                if (GetObject(iconInfo.hbmColor, Marshal.SizeOf<BITMAP>(), out BITMAP bitmap))
                                {
                                    logger.LogInfo($"Actual icon size from system: {bitmap.bmWidth}x{bitmap.bmHeight}");
                                    return (bitmap.bmWidth, bitmap.bmHeight);
                                }
                            }
                        }
                    }
                    finally
                    {
                        DestroyIcon(shfi.hIcon);
                    }
                }
            }
            catch (Exception ex)
            {
                logger.LogWarning($"Failed to get actual icon size: {ex.Message}");
            }

            // フォールバック：システムメトリクスを使用
            var systemIconWidth = GetSystemMetrics(SM_CXICON);
            var systemIconHeight = GetSystemMetrics(SM_CYICON);

            logger.LogInfo($"Using system metrics icon size: {systemIconWidth}x{systemIconHeight}");
            return (systemIconWidth, systemIconHeight);
        }

        /// <summary>
        /// ListViewアイテムの実際のクリック可能境界を取得
        /// </summary>
        /// <param name="listViewHandle">ListViewのハンドル</param>
        /// <param name="processHandle">プロセスハンドル</param>
        /// <param name="itemIndex">アイテムインデックス</param>
        /// <returns>クリック可能境界のRectangle（クライアント座標）</returns>
        private Rectangle GetItemClickableBounds(IntPtr listViewHandle, IntPtr processHandle, int itemIndex)
        {
            // リモートプロセスにRECT構造体用のメモリを割り当て
            var rectSize = Marshal.SizeOf<RECT>();
            var remoteRectPtr = VirtualAllocEx(processHandle, IntPtr.Zero, (uint)rectSize, MEM_COMMIT, PAGE_READWRITE);

            if (remoteRectPtr == IntPtr.Zero)
            {
                logger.LogWarning($"Failed to allocate memory for item {itemIndex} bounds");
                return Rectangle.Empty;
            }

            try
            {
                // LVIR_SELECTBOUNDSを使用してクリック可能領域を取得
                var wParam = new IntPtr(itemIndex);
                var lParam = remoteRectPtr;

                // 最初のパラメータでLVIR_SELECTBOUNDSを指定
                var rect = new RECT();
                rect.Left = LVIR_SELECTBOUNDS; // この値が境界タイプを指定

                var rectBytes = StructureToByteArray(rect);
                WriteProcessMemory(processHandle, remoteRectPtr, rectBytes, (uint)rectBytes.Length, out _);

                var result = SendMessage(listViewHandle, LVM_GETITEMRECT, wParam, lParam);
                if (result == IntPtr.Zero)
                {
                    logger.LogWarning($"Failed to get item rect for item {itemIndex}");
                    return Rectangle.Empty;
                }

                // 結果を読み込み
                var resultBuffer = new byte[rectSize];
                if (!ReadProcessMemory(processHandle, remoteRectPtr, resultBuffer, resultBuffer.Length, out _))
                {
                    logger.LogWarning($"Failed to read item rect for item {itemIndex}");
                    return Rectangle.Empty;
                }

                var resultRect = ByteArrayToStructure<RECT>(resultBuffer);
                var clickableBounds = new Rectangle(
                    resultRect.Left,
                    resultRect.Top,
                    resultRect.Right - resultRect.Left,
                    resultRect.Bottom - resultRect.Top
                );

                logger.LogInfo($"Item {itemIndex} clickable bounds (client): {clickableBounds}");
                return clickableBounds;
            }
            finally
            {
                VirtualFreeEx(processHandle, remoteRectPtr, 0, MEM_RELEASE);
            }
        }

        private double GetDpiScaleX()
        {
            var hdc = GetDC(IntPtr.Zero);
            var dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
            ReleaseDC(IntPtr.Zero, hdc);
            return dpiX / 96.0; // 96 DPIが標準
        }

        private double GetDpiScaleY()
        {
            var hdc = GetDC(IntPtr.Zero);
            var dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
            ReleaseDC(IntPtr.Zero, hdc);
            return dpiY / 96.0; // 96 DPIが標準
        }

        public bool IsInitialized => isInitialized;

        public List<DesktopIcon> GetDesktopIcons()
        {
            try
            {
                lock (cacheLock)
                {
                    // 初回は同期更新
                    if (!isInitialized)
                    {
                        RefreshIcons();
                        return new List<DesktopIcon>(cachedIcons);
                    }

                    // Shell通知で自動更新されるため、定期更新は不要
                    return new List<DesktopIcon>(cachedIcons);
                }
            }
            catch (Exception ex)
            {
                errorHandler.HandleError(new GameError(
                    "Failed to get desktop icons",
                    ErrorSeverity.Medium,
                    ex,
                    "DesktopIconManager.GetDesktopIcons"
                ));
                return new List<DesktopIcon>();
            }
        }

        public void RefreshIcons()
        {
            try
            {
                logger.LogInfo("=== Desktop Icon Detection Started ===");
                var icons = FetchDesktopIcons();

                // Win32方式で取得できない場合はダミーデータを作成
                if (icons.Count == 0)
                {
                    logger.LogWarning("Win32 method failed, creating dummy desktop icons for testing");
                    icons = CreateDummyIcons();
                }

                // 差分検出：アイコンが変更されていない場合はスキップ
                if (isInitialized && !HasIconsChanged(icons))
                {
                    logger.LogInfo("No icon changes detected, skipping update");
                    return;
                }

                // 取得したアイコンの詳細をログに記録
                if (!isInitialized || HasIconsChanged(icons))
                {
                    LogAllIconDetails(icons);
                }

                lock (cacheLock)
                {
                    cachedIcons = icons;
                    UpdateIconHashCache(icons);
                    isInitialized = true;
                }

                logger.LogInfo($"=== Desktop Icon Detection Complete: {icons.Count} icons found ===");
            }
            catch (Exception ex)
            {
                errorHandler.HandleError(new GameError(
                    "Failed to refresh desktop icons",
                    ErrorSeverity.High,
                    ex,
                    "DesktopIconManager.RefreshIcons"
                ));

                // エラーの場合もダミーデータで初期化
                lock (cacheLock)
                {
                    cachedIcons = CreateDummyIcons();
                    UpdateIconHashCache(cachedIcons);
                    isInitialized = true;
                }
                logger.LogInfo("Initialized with dummy icons due to error");
            }
        }

        /// <summary>
        /// 非同期でアイコンを更新（メインスレッドをブロックしない）
        /// </summary>
        public async Task RefreshIconsAsync()
        {
            try
            {
                // 既に実行中の場合は完了を待つ
                if (refreshTask != null && !refreshTask.IsCompleted)
                {
                    await refreshTask;
                    return;
                }

                refreshTask = Task.Run(() =>
                {
                    try
                    {
                        cancellationTokenSource.Token.ThrowIfCancellationRequested();

                        logger.LogInfo("=== Async Desktop Icon Detection Started ===");
                        var icons = FetchDesktopIcons();

                        cancellationTokenSource.Token.ThrowIfCancellationRequested();

                        // Win32方式で取得できない場合はダミーデータを作成
                        if (icons.Count == 0)
                        {
                            logger.LogWarning("Win32 method failed, creating dummy desktop icons for testing");
                            icons = CreateDummyIcons();
                        }

                        cancellationTokenSource.Token.ThrowIfCancellationRequested();

                        // 差分検出：アイコンが変更されていない場合はスキップ
                        if (isInitialized && !HasIconsChanged(icons))
                        {
                            logger.LogInfo("No icon changes detected in async update, skipping");
                            return;
                        }

                        // スレッドセーフなキャッシュ更新
                        lock (cacheLock)
                        {
                            cachedIcons = icons;
                            UpdateIconHashCache(icons);
                            isInitialized = true;
                        }

                        logger.LogInfo($"=== Async Desktop Icon Detection Complete: {icons.Count} icons found ===");
                    }
                    catch (OperationCanceledException)
                    {
                        logger.LogInfo("Async icon refresh was cancelled");
                    }
                    catch (Exception ex)
                    {
                        logger.LogError($"Async icon refresh failed: {ex.Message}");

                        // エラーの場合もダミーデータで初期化
                        lock (cacheLock)
                        {
                            if (!isInitialized)
                            {
                                cachedIcons = CreateDummyIcons();
                                UpdateIconHashCache(cachedIcons);
                                isInitialized = true;
                            }
                        }
                    }
                }, cancellationTokenSource.Token);

                await refreshTask;
            }
            catch (Exception ex)
            {
                logger.LogError($"RefreshIconsAsync failed: {ex.Message}");
            }
        }

        /// <summary>
        /// リソースのクリーンアップ
        /// </summary>
        public void Dispose()
        {
            try
            {
                // 非同期タスクのキャンセル
                cancellationTokenSource.Cancel();

                // 進行中のタスクの完了を待つ（タイムアウト付き）
                if (refreshTask != null && !refreshTask.IsCompleted)
                {
                    try
                    {
                        refreshTask.Wait(TimeSpan.FromSeconds(2));
                    }
                    catch (AggregateException)
                    {
                        // タスクがキャンセルされた場合は無視
                    }
                }

                // 変更監視を停止
                StopChangeNotification();

                // レジストリ監視を停止
                registryWatcher?.Dispose();
                registryWatcher = null;

                // リソース解放
                cancellationTokenSource.Dispose();

                logger.LogInfo("DesktopIconManager disposed successfully");
            }
            catch (Exception ex)
            {
                logger.LogError($"Error during DesktopIconManager disposal: {ex.Message}");
            }
        }

        private void LogAllIconDetails(List<DesktopIcon> icons)
        {
            logger.LogInfo("--- Desktop Icon Details (Dynamic Sizing) ---");
            for (int i = 0; i < icons.Count; i++)
            {
                var icon = icons[i];
                var safeName = SanitizeForLog(icon.Name);

                logger.LogInfo($"Icon {i + 1}:");
                logger.LogInfo($"  Name: '{safeName}'");
                logger.LogInfo($"  Display Bounds: {icon.Bounds}");
                logger.LogInfo($"  Collision Bounds: {icon.CollisionBounds}");
                logger.LogInfo($"  Display Size: {icon.Bounds.Width}x{icon.Bounds.Height}");
                logger.LogInfo($"  Collision Size: {icon.CollisionBounds.Width}x{icon.CollisionBounds.Height}");
                logger.LogInfo($"  Valid: {icon.IsValid}");
                logger.LogInfo($"  Handle: 0x{icon.IconHandle.ToInt64():X8}");
                logger.LogInfo("");
            }
            logger.LogInfo("--- End of Icon Details ---");
        }

        private static string SanitizeForLog(string input)
        {
            if (string.IsNullOrEmpty(input))
                return "Empty";

            var result = new StringBuilder();
            foreach (char c in input)
            {
                // 制御文字と方向制御文字をチェック
                if (char.IsControl(c))
                {
                    result.Append($"[CTRL:0x{(int)c:X2}]");
                }
                else if (IsDirectionalCharacter(c))
                {
                    result.Append($"[DIR:U+{(int)c:X4}]");
                }
                else if (c >= 32 && c <= 126)
                {
                    result.Append(c); // ASCII印刷可能文字
                }
                else if (c >= 0x3040 && c <= 0x309F) // ひらがな
                {
                    result.Append(c);
                }
                else if (c >= 0x30A0 && c <= 0x30FF) // カタカナ
                {
                    result.Append(c);
                }
                else if (c >= 0x4E00 && c <= 0x9FAF) // CJK漢字
                {
                    result.Append(c);
                }
                else if (c >= 0xFF00 && c <= 0xFFEF) // 全角ASCII
                {
                    result.Append(c);
                }
                else if (c <= 0xFF) // 拡張ASCII
                {
                    result.Append($"[EXT:0x{(int)c:X2}]");
                }
                else
                {
                    result.Append($"[UNI:U+{(int)c:X4}]");
                }
            }
            return result.ToString();
        }

        private static bool IsDirectionalCharacter(char c)
        {
            return (c >= 0x200E && c <= 0x200F) || // LTR/RTLマーク
                   (c >= 0x202A && c <= 0x202E) || // 方向埋め込み/オーバーライド
                   (c >= 0x2066 && c <= 0x2069) || // 方向アイソレート
                   (c == 0x061C);                   // アラビア文字マーク
        }

        private List<DesktopIcon> CreateDummyIcons()
        {
            var dummyIcons = new List<DesktopIcon>();

            // デスクトップの一般的な場所にダミーアイコンを配置
            // 実際のクリック可能領域をシミュレート（通常のアイコンサイズより大きい）
            var clickableIconWidth = 75;  // 典型的なクリック可能幅
            var clickableIconHeight = 75; // 典型的なクリック可能高さ
            var spacing = 80;

            logger.LogInfo($"Creating dummy desktop icons with clickable bounds ({clickableIconWidth}x{clickableIconHeight})...");
            for (int i = 0; i < 5; i++)
            {
                var x = 50 + (i % 3) * spacing;
                var y = 50 + (i / 3) * spacing;

                // 表示境界と衝突境界を同じにする（実際のクリック可能領域）
                var bounds = new Rectangle(x, y, clickableIconWidth, clickableIconHeight);
                var name = $"Dummy Icon {i + 1}";

                dummyIcons.Add(new DesktopIcon(bounds, name, IntPtr.Zero, bounds));
                logger.LogInfo($"  Created dummy icon: {name} at ({x}, {y}) clickable {clickableIconWidth}x{clickableIconHeight}");
            }

            logger.LogInfo($"Created {dummyIcons.Count} dummy desktop icons with clickable bounds");
            return dummyIcons;
        }

        private List<DesktopIcon> FetchDesktopIcons()
        {
            var icons = new List<DesktopIcon>();

            try
            {
                logger.LogInfo("Starting desktop icon fetch...");

                // デスクトップのListViewハンドルを取得
                var listViewHandle = GetDesktopListViewHandle();
                if (listViewHandle == IntPtr.Zero)
                {
                    logger.LogError("Could not find desktop ListView handle");
                    return icons;
                }

                logger.LogInfo($"Found desktop ListView handle: 0x{listViewHandle.ToInt64():X8}");

                // 作業領域（タスクバーを除いたスクリーン領域）を取得
                var workArea = new RECT();
                if (SystemParametersInfo(SPI_GETWORKAREA, 0, ref workArea, 0))
                {
                    logger.LogInfo($"Work area: Left={workArea.Left}, Top={workArea.Top}, Right={workArea.Right}, Bottom={workArea.Bottom}");
                }

                // ListViewウィンドウの位置とサイズを取得
                if (GetWindowRect(listViewHandle, out RECT listViewRect))
                {
                    logger.LogInfo($"ListView window rect: Left={listViewRect.Left}, Top={listViewRect.Top}, Right={listViewRect.Right}, Bottom={listViewRect.Bottom}");
                    logger.LogInfo($"ListView size: Width={listViewRect.Right - listViewRect.Left}, Height={listViewRect.Bottom - listViewRect.Top}");
                }

                // ListViewのプロセスIDを取得
                uint processId;
                var threadId = GetWindowThreadProcessId(listViewHandle, out processId);
                logger.LogInfo($"Desktop ListView process ID: {processId}, thread ID: {threadId}");

                // プロセスハンドルを開く
                var processHandle = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, false, processId);
                if (processHandle == IntPtr.Zero)
                {
                    var error = Marshal.GetLastWin32Error();
                    logger.LogError($"Could not open desktop process. Error: {error}");
                    return icons;
                }

                logger.LogInfo($"Opened process handle: 0x{processHandle.ToInt64():X8}");

                try
                {
                    // アイテム数を取得
                    var itemCount = (int)SendMessage(listViewHandle, LVM_GETITEMCOUNT, IntPtr.Zero, IntPtr.Zero);
                    logger.LogInfo($"Desktop has {itemCount} items");

                    if (itemCount > 0)
                    {
                        logger.LogInfo($"Attempting to get icons from ListView...");
                        icons = GetIconsFromListView(listViewHandle, processHandle, itemCount);
                        logger.LogInfo($"Successfully retrieved {icons.Count} icons");
                    }
                    else
                    {
                        logger.LogWarning("No items found on desktop");
                    }
                }
                finally
                {
                    CloseHandle(processHandle);
                }
            }
            catch (Exception ex)
            {
                logger.LogError($"Error fetching desktop icons: {ex.Message}");
                logger.LogError($"Stack trace: {ex.StackTrace}");
            }

            return icons;
        }

        private IntPtr GetDesktopListViewHandle()
        {
            logger.LogInfo("Starting desktop ListView handle search...");

            // Program Manager ウィンドウを取得
            var progmanHandle = FindWindow("Progman", "Program Manager");
            logger.LogInfo($"Program Manager handle: 0x{progmanHandle.ToInt64():X8}");
            if (progmanHandle == IntPtr.Zero)
            {
                logger.LogError("Could not find Program Manager window");
                return IntPtr.Zero;
            }

            // SHELLDLL_DefView を取得
            var defViewHandle = FindWindowEx(progmanHandle, IntPtr.Zero, "SHELLDLL_DefView", string.Empty);
            logger.LogInfo($"SHELLDLL_DefView handle: 0x{defViewHandle.ToInt64():X8}");
            if (defViewHandle == IntPtr.Zero)
            {
                logger.LogError("Could not find SHELLDLL_DefView window");
                // Windows 10/11では別の方法を試す
                return TryAlternativeDesktopSearch();
            }

            // SysListView32 (FolderView) を取得
            var listViewHandle = FindWindowEx(defViewHandle, IntPtr.Zero, "SysListView32", "FolderView");
            logger.LogInfo($"SysListView32 FolderView handle: 0x{listViewHandle.ToInt64():X8}");
            if (listViewHandle == IntPtr.Zero)
            {
                // ウィンドウ名なしで再試行
                listViewHandle = FindWindowEx(defViewHandle, IntPtr.Zero, "SysListView32", string.Empty);
                logger.LogInfo($"SysListView32 (no title) handle: 0x{listViewHandle.ToInt64():X8}");

                if (listViewHandle == IntPtr.Zero)
                {
                    logger.LogError("Could not find SysListView32 window");
                    return IntPtr.Zero;
                }
            }

            logger.LogInfo("Successfully found desktop ListView handle");
            return listViewHandle;
        }

        private IntPtr TryAlternativeDesktopSearch()
        {
            logger.LogInfo("Trying alternative desktop search method...");

            // WorkerWウィンドウでの検索を試す
            IntPtr workerWHandle = IntPtr.Zero;
            EnumWindows((hWnd, lParam) =>
            {
                var className = new StringBuilder(256);
                GetClassName(hWnd, className, className.Capacity);

                if (className.ToString() == "WorkerW")
                {
                    var shelldllDefView = FindWindowEx(hWnd, IntPtr.Zero, "SHELLDLL_DefView", string.Empty);
                    if (shelldllDefView != IntPtr.Zero)
                    {
                        var listView = FindWindowEx(shelldllDefView, IntPtr.Zero, "SysListView32", string.Empty);
                        if (listView != IntPtr.Zero)
                        {
                            workerWHandle = listView;
                            logger.LogInfo($"Found ListView in WorkerW: 0x{listView.ToInt64():X8}");
                            return false; // 停止
                        }
                    }
                }
                return true; // 続行
            }, IntPtr.Zero);

            return workerWHandle;
        }

        private List<DesktopIcon> GetIconsFromListView(IntPtr listViewHandle, IntPtr processHandle, int itemCount)
        {
            var icons = new List<DesktopIcon>();

            // リモートプロセスにメモリを割り当て
            var pointSize = Marshal.SizeOf<POINT>();
            var remotePointPtr = VirtualAllocEx(processHandle, IntPtr.Zero, (uint)pointSize, MEM_COMMIT, PAGE_READWRITE);

            var lvItemSize = Marshal.SizeOf<LVITEM>();
            var remoteLvItemPtr = VirtualAllocEx(processHandle, IntPtr.Zero, (uint)lvItemSize, MEM_COMMIT, PAGE_READWRITE);

            var textBufferSize = 256;
            var remoteTextPtr = VirtualAllocEx(processHandle, IntPtr.Zero, (uint)textBufferSize, MEM_COMMIT, PAGE_READWRITE);

            if (remotePointPtr == IntPtr.Zero || remoteLvItemPtr == IntPtr.Zero || remoteTextPtr == IntPtr.Zero)
            {
                logger.LogWarning("Could not allocate memory in remote process");
                return icons;
            }

            try
            {
                for (int i = 0; i < itemCount && i < 100; i++) // 最大100個に制限
                {
                    try
                    {
                        var icon = GetSingleIconWithClickableBounds(listViewHandle, processHandle, i, remotePointPtr, remoteLvItemPtr, remoteTextPtr, textBufferSize);
                        if (icon.IsValid)
                        {
                            icons.Add(icon);
                        }
                    }
                    catch (Exception ex)
                    {
                        logger.LogWarning($"Failed to get icon {i}: {ex.Message}");
                    }
                }
            }
            finally
            {
                // メモリを解放
                VirtualFreeEx(processHandle, remotePointPtr, 0, MEM_RELEASE);
                VirtualFreeEx(processHandle, remoteLvItemPtr, 0, MEM_RELEASE);
                VirtualFreeEx(processHandle, remoteTextPtr, 0, MEM_RELEASE);
            }

            return icons;
        }

        private DesktopIcon GetSingleIconWithClickableBounds(IntPtr listViewHandle, IntPtr processHandle, int index,
            IntPtr remotePointPtr, IntPtr remoteLvItemPtr, IntPtr remoteTextPtr, int textBufferSize)
        {
            // アイコンのテキストを取得
            var text = GetIconText(listViewHandle, processHandle, index, remoteLvItemPtr, remoteTextPtr, textBufferSize);

            // アイコンの実際のクリック可能境界を取得
            var clickableBounds = GetItemClickableBounds(listViewHandle, processHandle, index);
            if (clickableBounds.IsEmpty)
            {
                logger.LogWarning($"Failed to get clickable bounds for icon {index}");
                return new DesktopIcon { IsValid = false };
            }

            // クライアント座標をスクリーン座標に変換
            var topLeft = new POINT { X = clickableBounds.Left, Y = clickableBounds.Top };
            var bottomRight = new POINT { X = clickableBounds.Right, Y = clickableBounds.Bottom };

            if (!ClientToScreen(listViewHandle, ref topLeft) || !ClientToScreen(listViewHandle, ref bottomRight))
            {
                logger.LogWarning($"Failed to convert coordinates to screen for icon {index}");
                return new DesktopIcon { IsValid = false };
            }

            // スクリーン座標での境界を作成
            // 座標変換なしでそのまま使用
            var screenBounds = new Rectangle(
                topLeft.X,
                topLeft.Y,
                bottomRight.X - topLeft.X,
                bottomRight.Y - topLeft.Y
            );

            logger.LogInfo($"Icon {index} client bounds: {clickableBounds}");
            logger.LogInfo($"Icon {index} screen coords: topLeft=({topLeft.X},{topLeft.Y}), bottomRight=({bottomRight.X},{bottomRight.Y})");
            logger.LogInfo($"Icon {index} Y coordinate: {topLeft.Y}");
            logger.LogInfo($"Icon {index} final bounds: {screenBounds}");
            logger.LogInfo($"Icon {index} text: '{text}'");

            // 表示境界と衝突境界を同じにする（実際のクリック可能領域を使用）
            return new DesktopIcon(screenBounds, text, IntPtr.Zero, screenBounds);
        }

        /// <summary>
        /// アイコンのテキストを取得
        /// </summary>
        private string GetIconText(IntPtr listViewHandle, IntPtr processHandle, int index, IntPtr remoteLvItemPtr, IntPtr remoteTextPtr, int textBufferSize)
        {
            try
            {
                // アイコンのテキストを取得
                var lvItem = new LVITEM
                {
                    mask = LVIF_TEXT,
                    iItem = index,
                    iSubItem = 0,
                    pszText = remoteTextPtr,
                    cchTextMax = textBufferSize
                };

                var lvItemBytes = StructureToByteArray(lvItem);
                if (!WriteProcessMemory(processHandle, remoteLvItemPtr, lvItemBytes, (uint)lvItemBytes.Length, out _))
                {
                    return "Unknown Icon";
                }

                SendMessage(listViewHandle, LVM_GETITEMTEXT, new IntPtr(index), remoteLvItemPtr);

                // テキストを読み込み
                var textBuffer = new byte[textBufferSize];
                if (!ReadProcessMemory(processHandle, remoteTextPtr, textBuffer, textBuffer.Length, out _))
                {
                    return "Unknown Icon";
                }

                // Unicode文字列を適切にデコード（文字化け対策）
                return DecodeIconText(textBuffer);
            }
            catch (Exception ex)
            {
                logger.LogWarning($"Failed to get text for icon {index}: {ex.Message}");
                return "Unknown Icon";
            }
        }

        private DesktopIcon GetSingleIcon(IntPtr listViewHandle, IntPtr processHandle, int index,
            IntPtr remotePointPtr, IntPtr remoteLvItemPtr, IntPtr remoteTextPtr, int textBufferSize,
            int displayWidth, int displayHeight, int collisionWidth, int collisionHeight, int offsetX, int offsetY)
        {
            // アイコンの位置を取得
            var result = SendMessage(listViewHandle, LVM_GETITEMPOSITION, new IntPtr(index), remotePointPtr);
            if (result == IntPtr.Zero)
            {
                return new DesktopIcon { IsValid = false };
            }

            // 位置データを読み込み
            var pointBuffer = new byte[Marshal.SizeOf<POINT>()];
            if (!ReadProcessMemory(processHandle, remotePointPtr, pointBuffer, pointBuffer.Length, out _))
            {
                return new DesktopIcon { IsValid = false };
            }

            var point = ByteArrayToStructure<POINT>(pointBuffer);

            // ListViewのクライアント座標をスクリーン座標に変換
            var screenPoint = point;
            if (!ClientToScreen(listViewHandle, ref screenPoint))
            {
                logger.LogWarning($"Failed to convert client coordinates to screen coordinates for icon {index}");
                // 変換に失敗した場合は、デスクトップの位置を推定
                GetWindowRect(listViewHandle, out RECT desktopRect);
                screenPoint.X = desktopRect.Left + point.X;
                screenPoint.Y = desktopRect.Top + point.Y;
            }

            // DPIスケーリングを適用
            var dpiScaleX = GetDpiScaleX();
            var dpiScaleY = GetDpiScaleY();

            logger.LogInfo($"Icon {index} client coords: ({point.X}, {point.Y}) -> screen coords: ({screenPoint.X}, {screenPoint.Y}), DPI: {dpiScaleX:F2}x{dpiScaleY:F2}");

            // DPI認識設定により座標が既にスケールされている場合があるため、
            // PROCESS_SYSTEM_DPI_AWAREモードでは座標の追加変換は不要

            // アイコンのテキストを取得
            var lvItem = new LVITEM
            {
                mask = LVIF_TEXT,
                iItem = index,
                iSubItem = 0,
                pszText = remoteTextPtr,
                cchTextMax = textBufferSize
            };

            var lvItemBytes = StructureToByteArray(lvItem);
            if (!WriteProcessMemory(processHandle, remoteLvItemPtr, lvItemBytes, (uint)lvItemBytes.Length, out _))
            {
                return new DesktopIcon { IsValid = false };
            }

            SendMessage(listViewHandle, LVM_GETITEMTEXT, new IntPtr(index), remoteLvItemPtr);

            // テキストを読み込み
            var textBuffer = new byte[textBufferSize];
            if (!ReadProcessMemory(processHandle, remoteTextPtr, textBuffer, textBuffer.Length, out _))
            {
                return new DesktopIcon { IsValid = false };
            }

            // デバッグ用：バイトデータを16進文字列として記録
            var hexBytes = BitConverter.ToString(textBuffer, 0, Math.Min(32, textBuffer.Length));
            logger.LogInfo($"Raw bytes for icon {index}: {hexBytes}");

            // Unicode文字列を適切にデコード（文字化け対策）
            var text = DecodeIconText(textBuffer);
            logger.LogInfo($"Decoded text for icon {index}: '{text}'");

            // OverlayFormのタイトルバーオフセットを考慮（30px）
            const int titleBarHeight = 30;

            // 表示用の境界を計算（視覚的な正確性のため）
            var displayBounds = new Rectangle(
                screenPoint.X + offsetX,
                screenPoint.Y + offsetY + titleBarHeight, // タイトルバーオフセットを追加
                displayWidth,
                displayHeight
            );

            // 衝突判定用の境界を計算（実際のアイコンサイズに基づく）
            var collisionBounds = new Rectangle(
                screenPoint.X + offsetX + (displayWidth - collisionWidth) / 2,  // 表示境界の中央に配置
                screenPoint.Y + offsetY + titleBarHeight + (displayHeight - collisionHeight) / 2, // タイトルバーオフセットを追加
                collisionWidth,
                collisionHeight
            );

            logger.LogInfo($"Icon {index} bounds - Display: {displayBounds}, Collision: {collisionBounds}");

            return new DesktopIcon(displayBounds, text, IntPtr.Zero, collisionBounds);
        }

        private static T ByteArrayToStructure<T>(byte[] bytes) where T : struct
        {
            var handle = GCHandle.Alloc(bytes, GCHandleType.Pinned);
            try
            {
                return Marshal.PtrToStructure<T>(handle.AddrOfPinnedObject());
            }
            finally
            {
                handle.Free();
            }
        }

        private static byte[] StructureToByteArray<T>(T structure) where T : struct
        {
            var size = Marshal.SizeOf<T>();
            var bytes = new byte[size];
            var ptr = Marshal.AllocHGlobal(size);
            try
            {
                Marshal.StructureToPtr(structure, ptr, true);
                Marshal.Copy(ptr, bytes, 0, size);
            }
            finally
            {
                Marshal.FreeHGlobal(ptr);
            }
            return bytes;
        }

        private static string DecodeIconText(byte[] textBuffer)
        {
            try
            {
                // バッファが空または小さすぎる場合
                if (textBuffer == null || textBuffer.Length < 2)
                {
                    return "Unknown Icon";
                }

                // Win32 APIから返されるテキストは通常ANSIまたはUnicode
                // まずはANSI（システムのデフォルトコードページ）で試す
                var ansiText = TryDecodeAnsi(textBuffer);
                if (!string.IsNullOrEmpty(ansiText) && IsReadableText(ansiText))
                {
                    return ansiText;
                }

                // 次にUTF-16 LE（Windows標準Unicode）で試す
                var unicodeText = TryDecodeUnicode(textBuffer);
                if (!string.IsNullOrEmpty(unicodeText) && IsReadableText(unicodeText))
                {
                    return unicodeText;
                }

                // UTF-8で試す
                var utf8Text = TryDecodeUtf8(textBuffer);
                if (!string.IsNullOrEmpty(utf8Text) && IsReadableText(utf8Text))
                {
                    return utf8Text;
                }

                // 最後の手段：バイト値から推測してファイル名パターンを探す
                return ExtractLikelyFilename(textBuffer);
            }
            catch
            {
                return "Unknown Icon";
            }
        }

        private static string TryDecodeAnsi(byte[] buffer)
        {
            try
            {
                // null終端を探して切り詰める
                int end = Array.IndexOf(buffer, (byte)0);
                if (end >= 0)
                {
                    Array.Resize(ref buffer, end);
                }

                return Encoding.Default.GetString(buffer).Trim();
            }
            catch
            {
                return string.Empty;
            }
        }

        private static string TryDecodeUnicode(byte[] buffer)
        {
            try
            {
                // Unicode null終端を探す
                int nullPos = FindNullTerminator(buffer);
                if (nullPos > 0 && nullPos < buffer.Length)
                {
                    Array.Resize(ref buffer, nullPos);
                }

                var text = Encoding.Unicode.GetString(buffer).TrimEnd('\0');
                return RemoveDirectionalCharacters(text).Trim();
            }
            catch
            {
                return string.Empty;
            }
        }

        private static string TryDecodeUtf8(byte[] buffer)
        {
            try
            {
                int end = Array.IndexOf(buffer, (byte)0);
                if (end >= 0)
                {
                    Array.Resize(ref buffer, end);
                }

                var text = Encoding.UTF8.GetString(buffer).Trim();
                return RemoveDirectionalCharacters(text);
            }
            catch
            {
                return string.Empty;
            }
        }

        private static string ExtractLikelyFilename(byte[] buffer)
        {
            var result = new StringBuilder();

            // ASCIIの範囲で印刷可能文字を探す
            for (int i = 0; i < buffer.Length; i++)
            {
                byte b = buffer[i];
                if (b == 0) break; // null終端

                // 印刷可能ASCII文字（ファイル名に使用されそうな文字）
                if ((b >= 32 && b <= 126) || b == 46) // 32-126 + '.'
                {
                    char c = (char)b;
                    // ファイル名として有効そうな文字のみ
                    if (char.IsLetterOrDigit(c) || c == '.' || c == '_' || c == '-' || c == ' ')
                    {
                        result.Append(c);
                    }
                }
            }

            var text = result.ToString().Trim();
            return string.IsNullOrEmpty(text) ? "Unknown Icon" : text;
        }

        private static bool IsReadableText(string text)
        {
            if (string.IsNullOrWhiteSpace(text) || text.Length > 200)
                return false;

            int readableChars = 0;
            int totalChars = 0;

            foreach (char c in text)
            {
                totalChars++;

                // 読みやすい文字をカウント
                if (char.IsLetterOrDigit(c) ||
                    char.IsPunctuation(c) ||
                    char.IsSymbol(c) ||
                    c == ' ' || c == '.' || c == '_' || c == '-')
                {
                    readableChars++;
                }
            }

            // 少なくとも60%が読みやすい文字である必要がある
            return totalChars > 0 && (readableChars / (double)totalChars) >= 0.6;
        }

        private static bool IsValidDisplayText(string text)
        {
            if (string.IsNullOrEmpty(text) || text.Length > 100)
                return false;

            int validCharCount = 0;
            int totalChars = 0;

            foreach (char c in text)
            {
                totalChars++;

                // 制御文字をチェック（一部は許可）
                if (char.IsControl(c))
                {
                    if (c != '\0' && c != '\r' && c != '\n' && c != '\t')
                        return false;
                    continue;
                }

                // サロゲートペアや異常に高い値の文字を除外
                if (c > 0xFFFF)
                    return false;

                // 有効な文字の種類をカウント
                if ((c >= 32 && c <= 126) ||        // ASCII印刷可能文字
                    (c >= 0x3040 && c <= 0x309F) || // ひらがな
                    (c >= 0x30A0 && c <= 0x30FF) || // カタカナ
                    (c >= 0x4E00 && c <= 0x9FAF) || // CJK漢字
                    (c >= 0xFF00 && c <= 0xFFEF))   // 全角ASCII
                {
                    validCharCount++;
                }
            }

            // 有効な文字が80%以上の場合のみ有効とする
            return totalChars > 0 && (validCharCount / (double)totalChars) >= 0.8;
        }

        private static string ExtractValidCharacters(byte[] textBuffer)
        {
            var result = new StringBuilder();

            // Unicode文字として解析（Little Endian）
            for (int i = 0; i < textBuffer.Length - 1; i += 2)
            {
                if (i + 1 < textBuffer.Length)
                {
                    char c = (char)(textBuffer[i] | (textBuffer[i + 1] << 8));

                    if (c == 0) // null終端
                        break;

                    // 有効な文字のみを抽出
                    if ((c >= 32 && c <= 126) ||        // ASCII印刷可能文字
                        (c >= 0x3040 && c <= 0x309F) || // ひらがな
                        (c >= 0x30A0 && c <= 0x30FF) || // カタカナ
                        (c >= 0x4E00 && c <= 0x9FAF) || // CJK漢字
                        (c >= 0xFF00 && c <= 0xFFEF))   // 全角ASCII
                    {
                        result.Append(c);
                    }
                }
            }

            var text = result.ToString().Trim();
            return string.IsNullOrEmpty(text) ? "Unknown Icon" : text;
        }

        private static int FindNullTerminator(byte[] buffer)
        {
            // Unicode文字列のnull終端を探す（2バイトずつチェック）
            for (int i = 0; i < buffer.Length - 1; i += 2)
            {
                if (buffer[i] == 0 && buffer[i + 1] == 0)
                {
                    return i;
                }
            }
            return buffer.Length;
        }

        private static string RemoveDirectionalCharacters(string text)
        {
            if (string.IsNullOrEmpty(text))
                return text;

            var result = new StringBuilder();
            foreach (char c in text)
            {
                // Unicode方向制御文字を除去
                if (c >= 0x200E && c <= 0x200F) continue; // LTR/RTLマーク
                if (c >= 0x202A && c <= 0x202E) continue; // 方向埋め込み/オーバーライド
                if (c >= 0x2066 && c <= 0x2069) continue; // 方向アイソレート
                if (c == 0x061C) continue; // アラビア文字マーク

                result.Append(c);
            }
            return result.ToString();
        }

    }
}