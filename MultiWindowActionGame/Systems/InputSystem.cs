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

        public override string SystemName => "Input System";
        public override GameSystemPriority Priority => GameSystemPriority.Input;

        public InputSystem(ILogger logger, IErrorHandler errorHandler, IMainGame mainGame, IWindowManager windowManager, IInputService inputService)
            : base(logger, errorHandler)
        {
            this.mainGame = mainGame ?? throw new ArgumentNullException(nameof(mainGame));
            this.windowManager = windowManager ?? throw new ArgumentNullException(nameof(windowManager));
            this.inputService = inputService ?? throw new ArgumentNullException(nameof(inputService));
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
                // グローバル入力の処理
                HandleGlobalInput();

                // デバッグ入力
                if (inputService.IsKeyDown(Keys.F3))
                {
                    mainGame.ToggleDebugMode();
                    logger.LogDebug($"Debug mode toggled (now: {mainGame.IsDebugMode})", SystemName);

                    // 連続トグルを防ぐための小さな遅延
                    await Task.Delay(200);
                }

                // 設定入力
                if (inputService.IsKeyDown(Keys.F1))
                {
                    logger.LogInfo("Settings screen requested", SystemName);
                    // 設定画面の表示をトリガーできる
                }

                // プレイヤー入力はPlayerInputHandlerコンポーネントで処理される

            }
            catch (Exception ex)
            {
                errorHandler.HandleError("Error processing input", ErrorSeverity.Medium, ex, SystemName);
            }
        }

        private void HandleGlobalInput()
        {
            // 特定のエンティティに属さないシステム全体の入力を処理する

            // ウィンドウ管理ショートカット
            if (inputService.IsKeyDown(Keys.F11))
            {
                logger.LogDebug("Window management toggle requested", SystemName);
                // ウィンドウボーダーなどをトグルできる
            }

            // 緊急終了
            if (inputService.IsKeyDown(Keys.Escape) && inputService.IsKeyDown(Keys.LShiftKey))
            {
                logger.LogWarning("Emergency exit requested", null, SystemName);
                Application.Exit();
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