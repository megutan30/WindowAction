using System;
using System.Runtime.InteropServices;
using System.Threading;
using MultiWindowActionGame.Core;
using MultiWindowActionGame.Interfaces;
using Timer = System.Threading.Timer;

namespace MultiWindowActionGame.Utilities
{
    /// <summary>
    /// Windowsレジストリを監視してデスクトップアイコンの位置変更を検出
    /// </summary>
    public class RegistryIconPositionWatcher : IDisposable
    {
        #region Win32 API定義

        private const uint HKEY_CURRENT_USER = 0x80000001;
        private const int REG_NOTIFY_CHANGE_LAST_SET = 0x00000004;
        private const int ERROR_SUCCESS = 0;

        [DllImport("advapi32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
        private static extern int RegOpenKeyEx(
            UIntPtr hKey,
            string lpSubKey,
            uint ulOptions,
            int samDesired,
            out UIntPtr phkResult);

        [DllImport("advapi32.dll", SetLastError = true)]
        private static extern int RegNotifyChangeKeyValue(
            UIntPtr hKey,
            bool bWatchSubtree,
            int dwNotifyFilter,
            IntPtr hEvent,
            bool fAsynchronous);

        [DllImport("advapi32.dll", SetLastError = true)]
        private static extern int RegCloseKey(UIntPtr hKey);

        private const int KEY_NOTIFY = 0x0010;

        #endregion

        #region フィールド

        private readonly ILogger logger;
        private readonly Action onPositionChanged;
        private UIntPtr registryKey = UIntPtr.Zero;
        private Thread? watcherThread;
        private ManualResetEvent? stopEvent;
        private AutoResetEvent? changeEvent;
        private Timer? debounceTimer;
        private readonly object debounceTimerLock = new object();
        private bool isDisposed = false;

        private const int DEBOUNCE_DELAY_MS = 500;
        private const string REGISTRY_PATH = @"Software\Microsoft\Windows\Shell\Bags";

        #endregion

        #region コンストラクタ

        /// <summary>
        /// RegistryIconPositionWatcherのコンストラクタ
        /// </summary>
        /// <param name="logger">ロガー</param>
        /// <param name="onPositionChanged">位置変更時のコールバック</param>
        public RegistryIconPositionWatcher(ILogger logger, Action onPositionChanged)
        {
            this.logger = logger ?? throw new ArgumentNullException(nameof(logger));
            this.onPositionChanged = onPositionChanged ?? throw new ArgumentNullException(nameof(onPositionChanged));
        }

        #endregion

        #region 公開メソッド

        /// <summary>
        /// レジストリ監視を開始
        /// </summary>
        public void StartWatching()
        {
            if (isDisposed)
            {
                throw new ObjectDisposedException(nameof(RegistryIconPositionWatcher));
            }

            try
            {
                // レジストリキーを開く
                int result = RegOpenKeyEx(
                    new UIntPtr(HKEY_CURRENT_USER),
                    REGISTRY_PATH,
                    0,
                    KEY_NOTIFY,
                    out registryKey);

                if (result != ERROR_SUCCESS)
                {
                    logger.LogError($"Failed to open registry key: {REGISTRY_PATH} (Error code: {result})");
                    return;
                }

                logger.LogInfo($"Registry key opened successfully: {REGISTRY_PATH}");

                // イベントオブジェクトの作成
                stopEvent = new ManualResetEvent(false);
                changeEvent = new AutoResetEvent(false);

                // 監視スレッドを起動
                watcherThread = new Thread(WatcherThreadProc)
                {
                    IsBackground = true,
                    Name = "RegistryIconPositionWatcher"
                };
                watcherThread.Start();

                logger.LogInfo("Registry icon position watcher started");
            }
            catch (Exception ex)
            {
                logger.LogError($"Failed to start registry watcher: {ex.Message}");
                CleanupResources();
                throw;
            }
        }

        /// <summary>
        /// レジストリ監視を停止
        /// </summary>
        public void StopWatching()
        {
            if (isDisposed)
            {
                return;
            }

            try
            {
                logger.LogInfo("Stopping registry icon position watcher...");

                // 停止イベントを設定
                stopEvent?.Set();

                // スレッドの終了を待機（最大2秒）
                if (watcherThread != null && watcherThread.IsAlive)
                {
                    if (!watcherThread.Join(TimeSpan.FromSeconds(2)))
                    {
                        logger.LogWarning("Watcher thread did not exit gracefully within timeout");
                    }
                }

                CleanupResources();
                logger.LogInfo("Registry icon position watcher stopped");
            }
            catch (Exception ex)
            {
                logger.LogError($"Error stopping registry watcher: {ex.Message}");
            }
        }

        #endregion

        #region プライベートメソッド

        /// <summary>
        /// 監視スレッドのメインループ
        /// </summary>
        private void WatcherThreadProc()
        {
            try
            {
                logger.LogDebug("Registry watcher thread started");

                while (!isDisposed && stopEvent != null && changeEvent != null)
                {
                    // レジストリ変更通知を登録
                    int result = RegNotifyChangeKeyValue(
                        registryKey,
                        true,  // サブキーも監視
                        REG_NOTIFY_CHANGE_LAST_SET,
                        changeEvent.SafeWaitHandle.DangerousGetHandle(),
                        true);  // 非同期

                    if (result != ERROR_SUCCESS)
                    {
                        logger.LogError($"RegNotifyChangeKeyValue failed (Error code: {result})");
                        break;
                    }

                    // 変更イベントまたは停止イベントを待機
                    WaitHandle[] waitHandles = new WaitHandle[] { stopEvent, changeEvent };
                    int index = WaitHandle.WaitAny(waitHandles);

                    if (index == 0)
                    {
                        // 停止イベントが発生
                        logger.LogDebug("Stop event received, exiting watcher thread");
                        break;
                    }
                    else if (index == 1)
                    {
                        // 変更イベントが発生
                        logger.LogDebug("Registry change detected");
                        TriggerDebounce();
                    }
                }

                logger.LogDebug("Registry watcher thread exiting normally");
            }
            catch (Exception ex)
            {
                logger.LogError($"Exception in registry watcher thread: {ex.Message}");
                logger.LogError($"Stack trace: {ex.StackTrace}");
            }
        }

        /// <summary>
        /// デバウンスタイマーをトリガー
        /// </summary>
        private void TriggerDebounce()
        {
            lock (debounceTimerLock)
            {
                try
                {
                    // 既存のタイマーを破棄
                    debounceTimer?.Dispose();

                    // 新しいタイマーを作成（500ms後に実行）
                    debounceTimer = new Timer(
                        OnDebounceTimerElapsed,
                        null,
                        DEBOUNCE_DELAY_MS,
                        Timeout.Infinite);

                    logger.LogTrace("Debounce timer reset");
                }
                catch (Exception ex)
                {
                    logger.LogError($"Error triggering debounce: {ex.Message}");
                }
            }
        }

        /// <summary>
        /// デバウンスタイマー経過時のコールバック
        /// </summary>
        private void OnDebounceTimerElapsed(object? state)
        {
            try
            {
                logger.LogDebug("Debounce timer elapsed, executing position changed callback");
                onPositionChanged?.Invoke();
            }
            catch (Exception ex)
            {
                logger.LogError($"Error in position changed callback: {ex.Message}");
            }
        }

        /// <summary>
        /// リソースのクリーンアップ
        /// </summary>
        private void CleanupResources()
        {
            try
            {
                // デバウンスタイマーの破棄
                lock (debounceTimerLock)
                {
                    debounceTimer?.Dispose();
                    debounceTimer = null;
                }

                // イベントオブジェクトの破棄
                changeEvent?.Dispose();
                changeEvent = null;

                stopEvent?.Dispose();
                stopEvent = null;

                // レジストリキーのクローズ
                if (registryKey != UIntPtr.Zero)
                {
                    RegCloseKey(registryKey);
                    registryKey = UIntPtr.Zero;
                    logger.LogDebug("Registry key closed");
                }
            }
            catch (Exception ex)
            {
                logger.LogError($"Error during resource cleanup: {ex.Message}");
            }
        }

        #endregion

        #region IDisposable実装

        /// <summary>
        /// リソースを解放
        /// </summary>
        public void Dispose()
        {
            if (isDisposed)
            {
                return;
            }

            isDisposed = true;
            StopWatching();
            GC.SuppressFinalize(this);
        }

        /// <summary>
        /// ファイナライザ
        /// </summary>
        ~RegistryIconPositionWatcher()
        {
            Dispose();
        }

        #endregion
    }
}
