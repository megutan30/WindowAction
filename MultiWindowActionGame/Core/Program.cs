using System;
using System.Windows.Forms;
using System.Runtime.InteropServices;
using MultiWindowActionGame.Core;
using MultiWindowActionGame.Utilities;
using MultiWindowActionGame.Windows;
using MultiWindowActionGame.DI;
using MultiWindowActionGame.Interfaces;
using MultiWindowActionGame.UI;
using MultiWindowActionGame.Managers;

namespace MultiWindowActionGame.Core
{
    static class Program
    {
        public static GameForm? mainForm;

        [DllImport("user32.dll")]
        private static extern int SetWindowLong(IntPtr hWnd, int nIndex, int dwNewLong);

        [DllImport("user32.dll")]
        private static extern int GetWindowLong(IntPtr hWnd, int nIndex);

        [DllImport("user32.dll")]
        private static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);

        [DllImport("user32.dll")]
        private static extern bool SetProcessDPIAware();

        [DllImport("shcore.dll")]
        private static extern int SetProcessDpiAwareness(int awareness);

        // DPI Awareness levels
        private const int PROCESS_DPI_UNAWARE = 0;
        private const int PROCESS_SYSTEM_DPI_AWARE = 1;
        private const int PROCESS_PER_MONITOR_DPI_AWARE = 2;

        [STAThread]
        static async Task Main()
        {
            // DPI認識を設定（座標の誤差を修正）
            try
            {
                SetProcessDpiAwareness(PROCESS_SYSTEM_DPI_AWARE);
            }
            catch
            {
                // フォールバック（古いWindowsバージョン用）
                SetProcessDPIAware();
            }

            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);

            mainForm = new GameForm();

            // デバッグ情報を出力

            // DIコンテナセットアップ
            var serviceContainer = new ServiceContainer();
            ServiceRegistration.RegisterServices(serviceContainer);

            // サービス解決
            var windowManager = serviceContainer.Resolve<IWindowManager>();
            var stageManager = serviceContainer.Resolve<IStageManager>();
            var gameSettings = serviceContainer.Resolve<IGameSettings>();
            var performanceMonitor = serviceContainer.Resolve<IPerformanceMonitor>();
            var noEntryZoneManager = serviceContainer.Resolve<INoEntryZoneManager>();
            var windowEffectManager = serviceContainer.Resolve<IWindowEffectManager>();
            var buttonFactory = serviceContainer.Resolve<IButtonFactory>();
            var windowFactory = serviceContainer.Resolve<IWindowFactory>();

            // StageManagerにButtonFactoryとWindowFactoryを設定
            ((StageManager)stageManager).SetButtonFactory(buttonFactory);
            ((StageManager)stageManager).SetWindowFactory(windowFactory);

            // レガシー互換性のためCurrentプロパティを設定（段階的削除予定）
            WindowManager.Current = (WindowManager)windowManager;
            StageManager.Current = (StageManager)stageManager;
            GameSettings.Current = (GameSettings)gameSettings;
            PerformanceMonitor.Current = (PerformanceMonitor)performanceMonitor;
            NoEntryZoneManager.Current = (NoEntryZoneManager)noEntryZoneManager;
            MultiWindowActionGame.Effects.WindowEffectManager.Current = (MultiWindowActionGame.Effects.WindowEffectManager)windowEffectManager;

            ((WindowManager)windowManager).RegisterFormOrder(mainForm, MultiWindowActionGame.Managers.ZOrderPriority.Bottom);
            windowManager.Initialize();

            // デスクトップアイコン管理機能を初期化（ゲーム初期化前に実行）
            DesktopIconHelper.Initialize();

            // Shell通知を登録（mainFormのハンドルを使用）
            if (DesktopIconHelper.Current != null)
            {
                ((DesktopIconManager)DesktopIconHelper.Current).InitializeChangeNotification(mainForm);
            }

            // DIコンテナからMainGameを解決
            var game = serviceContainer.Resolve<IMainGame>();

            // StageManagerにMainGameを設定（循環依存回避）
            ((StageManager)stageManager).SetMainGame(game);

            await game.InitializeAsync();

            Task gameLoopTask = game.RunGameLoopAsync();

            Application.Run(mainForm);

            await gameLoopTask;
        }
    }
}