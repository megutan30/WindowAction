using System;
using System.Diagnostics;
using System.Drawing;
using System.Numerics;
using System.Windows.Forms;
using static MultiWindowActionGame.Utilities.GameSettings;
using MultiWindowActionGame.Player;
using MultiWindowActionGame.Utilities;
using MultiWindowActionGame.UI;
using MultiWindowActionGame.Windows;
using MultiWindowActionGame.Core;
using MultiWindowActionGame.Interfaces;
using MultiWindowActionGame.Core.Systems;
using MultiWindowActionGame.DI;

namespace MultiWindowActionGame.Core
{

    public class MainGame : IMainGame
    {
        // インスタンス統一化のためのstatic参照
        private static MainGame? _current;
        public static MainGame Current
        {
            get => _current ?? throw new InvalidOperationException("MainGame is not initialized");
            internal set => _current = value;
        }
        
        private PlayerForm? player;
        private readonly IWindowManager windowManager;
        private readonly IGameSettings gameSettings;
        private readonly IPerformanceMonitor performanceMonitor;
        private readonly INoEntryZoneManager noEntryZoneManager;
        private readonly IPlayerFormFactory playerFormFactory;
        private readonly IStageManager stageManager;
        private readonly IGameTimeService gameTimeService;
        private readonly ISystemManager systemManager;
        private readonly IServiceContainer serviceContainer;
        private BufferedGraphics? graphicsBuffer;
        private bool isDebugMode = false;
        public bool IsDebugModeValue 
        { 
            get => isDebugMode; 
            private set => isDebugMode = value; 
        }
        
        // IMainGame interface implementation
        bool IMainGame.IsDebugMode => IsDebugModeValue;
        public static bool IsDebugMode => Current.IsDebugModeValue;
        private readonly GameplaySettings settings;
        private bool isPaused = false;
        private bool isTransitioningStage = false;

        // DI対応コンストラクタ
        public MainGame(IWindowManager windowManager, IGameSettings gameSettings, IPerformanceMonitor performanceMonitor, INoEntryZoneManager noEntryZoneManager, IPlayerFormFactory playerFormFactory, IStageManager stageManager, IGameTimeService gameTimeService, ISystemManager systemManager, IServiceContainer serviceContainer)
        {
            this.windowManager = windowManager ?? throw new ArgumentNullException(nameof(windowManager));
            this.gameSettings = gameSettings ?? throw new ArgumentNullException(nameof(gameSettings));
            this.performanceMonitor = performanceMonitor ?? throw new ArgumentNullException(nameof(performanceMonitor));
            this.noEntryZoneManager = noEntryZoneManager ?? throw new ArgumentNullException(nameof(noEntryZoneManager));
            this.playerFormFactory = playerFormFactory ?? throw new ArgumentNullException(nameof(playerFormFactory));
            this.stageManager = stageManager ?? throw new ArgumentNullException(nameof(stageManager));
            this.gameTimeService = gameTimeService ?? throw new ArgumentNullException(nameof(gameTimeService));
            this.systemManager = systemManager ?? throw new ArgumentNullException(nameof(systemManager));
            this.serviceContainer = serviceContainer ?? throw new ArgumentNullException(nameof(serviceContainer));
            this.settings = gameSettings.Gameplay;
        }

        public async Task InitializeAsync()
        {
            Current = this;
            windowManager.Initialize();

            InitializeGraphicsBuffer();
            if (Program.mainForm != null)
            {
                Program.mainForm.Resize += MainForm_Resize;
            }

            gameTimeService.Start();

            // システムの登録（優先度順に実行される: Input=100, Rendering=400）
            var inputSystem = serviceContainer.Resolve<InputSystem>();
            var renderingSystem = serviceContainer.Resolve<RenderingSystem>();

            systemManager.RegisterSystem(inputSystem);
            systemManager.RegisterSystem(renderingSystem);

            // システムの初期化
            await systemManager.InitializeAllAsync();

            // プレイヤーを先に初期化（デフォルト位置で）
            InitializePlayer(new Point(100, 100));

            await stageManager.StartStageAsync(0);
        }

        public void InitializePlayer(Point startPosition)
        {
            if (player == null)
            {
                player = playerFormFactory.CreatePlayer(startPosition);
                windowManager.SetPlayer(player);
                // Show()はShowAllElements()で実行されるため、ここでは呼び出さない（UIデッドロック回避）
            }
            else
            {
                player.ResetPosition(startPosition);
            }
        }
        // IMainGame interface implementation
        PlayerForm? IMainGame.GetPlayer()
        {
            return player;
        }
        
        // Instance method for DI usage
        public PlayerForm? GetPlayerInstance()
        {
            return player;
        }

        // Static method for legacy compatibility
        public static PlayerForm? GetPlayer()
        {
            return Current.player;
        }

        private void InitializeGraphicsBuffer()
        {
            if (Program.mainForm != null && Program.mainForm.ClientSize.Width > 0 && Program.mainForm.ClientSize.Height > 0)
            {
                BufferedGraphicsContext context = BufferedGraphicsManager.Current;
                graphicsBuffer = context.Allocate(Program.mainForm.CreateGraphics(),
                    Program.mainForm.ClientRectangle);
            }
        }
        private void MainForm_Resize(object? sender, EventArgs e)
        {
            InitializeGraphicsBuffer();
        }
        public void PauseGame()
        {
            isPaused = true;
            gameTimeService.SetPaused(true);
            systemManager.PauseAll();
        }
        public void ResumeGame()
        {
            isPaused = false;
            gameTimeService.SetPaused(false);
            systemManager.ResumeAll();
        }

        public void ResetStageTransition()
        {
            isTransitioningStage = false;
        }
        public async Task RunGameLoopAsync()
        {
            int targetFrameTime = 1000 / settings.TargetFPS;

            while (Program.mainForm != null && !Program.mainForm.IsDisposed)
            {
                if (isPaused)
                {
                    await Task.Delay(100);
                    return;
                }
                int startTime = Environment.TickCount;

                gameTimeService.Update();

                // システムベースの更新（InputSystem + RenderingSystem）
                await systemManager.UpdateAllAsync(gameTimeService.DeltaTime);

                // 従来のUpdateAsync()は残存（Player/Window/Goal/Button更新）
                // 将来的にこれらもシステム化を検討
                await UpdateAsync();

                int elapsedTime = Environment.TickCount - startTime;
                int sleepTime = targetFrameTime - elapsedTime;

                // F3キー処理はInputSystemに移行済み（Phase 3完了）

                if (sleepTime > 0)
                {
                    await Task.Delay(sleepTime);
                }
            }
        }
        private async Task UpdateAsync()
        {
            using (performanceMonitor.BeginScope("Total Update"))
            {
                using (performanceMonitor.BeginScope("Player Update"))
                {
                    if (player != null)
                    {
                        await player.UpdateAsync(gameTimeService.DeltaTime);
                    }
                }
                using (performanceMonitor.BeginScope("Window Update"))
                {
                    await windowManager.UpdateAsync(gameTimeService.DeltaTime);
                }
                // ゴールの更新を追加
                if (stageManager.CurrentGoal != null)
                {
                    await stageManager.CurrentGoal.UpdateAsync(gameTimeService.DeltaTime);
                }
                // ボタンの更新を追加
                var buttons = windowManager.GetAllButtons();
                foreach (var button in buttons)
                {
                    await button.UpdateAsync(gameTimeService.DeltaTime);
                }
                if (player != null && !isTransitioningStage && stageManager.CheckGoal(player))
                {
                    isTransitioningStage = true;
                    stageManager.StartNextStage();
                }
            }
            performanceMonitor.UpdateFrameTime(gameTimeService.DeltaTime);
        }
        private void ShowSettingsForm()
        {
            if (Program.mainForm != null)
            {
                isPaused = true;
                using (var settingsForm = new SettingsForm())
                {
                    settingsForm.ShowDialog(Program.mainForm);
                }
                isPaused = false;
            }
        }

        // IMainGame interface implementation
        public void ToggleDebugMode()
        {
            IsDebugModeValue = !IsDebugModeValue;
        }
    }
}