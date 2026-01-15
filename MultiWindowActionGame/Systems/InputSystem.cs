using MultiWindowActionGame.Core;
using MultiWindowActionGame.Core.Systems;
using MultiWindowActionGame.Interfaces;
using System;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace MultiWindowActionGame.Core.Systems
{
    public class InputSystem : BaseGameSystem
    {
        private readonly IMainGame mainGame;
        private readonly IWindowManager windowManager;
        private readonly IInputService inputService;
        private readonly IStageManager stageManager;
        private readonly INotificationService notificationService;

        // 放置検出用
        private float idleTime = 0f;
        private const float IDLE_TIMEOUT_SECONDS = 60f;  // 3分
        private const float IDLE_WARNING_SECONDS = 10f;   // 警告表示開始

        public override string SystemName => "Input System";
        public override GameSystemPriority Priority => GameSystemPriority.Input;

        public InputSystem(ILogger logger, IErrorHandler errorHandler, IMainGame mainGame, IWindowManager windowManager, IInputService inputService, IStageManager stageManager, INotificationService notificationService)
            : base(logger, errorHandler)
        {
            this.mainGame = mainGame ?? throw new ArgumentNullException(nameof(mainGame));
            this.windowManager = windowManager ?? throw new ArgumentNullException(nameof(windowManager));
            this.inputService = inputService ?? throw new ArgumentNullException(nameof(inputService));
            this.stageManager = stageManager ?? throw new ArgumentNullException(nameof(stageManager));
            this.notificationService = notificationService ?? throw new ArgumentNullException(nameof(notificationService));
        }

        protected override async Task OnInitializeAsync()
        {
            logger.LogInfo("Input system ready for player input processing", SystemName);
            await Task.CompletedTask;
        }

        protected override async Task OnUpdateAsync(float deltaTime)
        {
            try
            {
                // 放置検出
                CheckIdleTimeout(deltaTime);

                // Global input handling
                HandleGlobalInput();

                // Debug input
                //if (inputService.IsKeyDown(Keys.F3))
                //{
                //    mainGame.ToggleDebugMode();
                //    logger.LogDebug($"Debug mode toggled (now: {mainGame.IsDebugMode})", SystemName);

                //    // Small delay to prevent rapid toggling
                //    await Task.Delay(200);
                //}

                // Settings input
                if (inputService.IsKeyDown(Keys.F1))
                {
                    logger.LogInfo("Settings screen requested", SystemName);
                    // Could trigger settings display
                }

                // Player input is handled by PlayerInputHandler component

            }
            catch (Exception ex)
            {
                errorHandler.HandleError("Error processing input", ErrorSeverity.Medium, ex, SystemName);
            }
        }

        private void HandleGlobalInput()
        {
            // Handle system-wide input that doesn't belong to specific entities

            // Window management shortcuts
            if (inputService.IsKeyDown(Keys.F11))
            {
                logger.LogDebug("Window management toggle requested", SystemName);
                // Could toggle window borders, etc.
            }

            // Emergency exit
            if (inputService.IsKeyDown(Keys.Escape) && inputService.IsKeyDown(Keys.LShiftKey))
            {
                logger.LogWarning("Emergency exit requested", null, SystemName);
                Application.Exit();
            }
        }

        /// <summary>
        /// 放置検出：一定時間入力がなければタイトル画面に戻る
        /// </summary>
        private void CheckIdleTimeout(float deltaTime)
        {
            // タイトルステージ（ステージ0）では放置検出しない
            if (stageManager.CurrentStageIndex == 0)
            {
                idleTime = 0f;
                notificationService.HideIdleWarning();
                return;
            }

            // 入力検出（キーボード or マウス移動）
            if (inputService.IsAnyGameKeyPressed() || inputService.HasMouseMoved())
            {
                idleTime = 0f;
                notificationService.HideIdleWarning();
            }
            else
            {
                idleTime += deltaTime;

                // 残り時間を計算
                float remainingTime = IDLE_TIMEOUT_SECONDS - idleTime;

                // 残り10秒以下で警告表示
                if (remainingTime <= IDLE_WARNING_SECONDS && remainingTime > 0)
                {
                    int remainingSeconds = (int)Math.Ceiling(remainingTime);
                    notificationService.ShowIdleWarning(remainingSeconds);
                }

                // タイムアウト
                if (idleTime >= IDLE_TIMEOUT_SECONDS)
                {
                    logger.LogInfo($"Idle timeout reached ({IDLE_TIMEOUT_SECONDS}s), returning to title", SystemName);
                    idleTime = 0f;
                    notificationService.HideIdleWarning();
                    stageManager.ToTitleStage();
                }
            }
        }

        protected override void OnPause()
        {
            logger.LogInfo("Input processing paused", SystemName);
        }

        protected override void OnResume()
        {
            logger.LogInfo("Input processing resumed", SystemName);
        }

        protected override void OnShutdown()
        {
            logger.LogInfo("Input system shutdown complete", SystemName);
        }
    }
}