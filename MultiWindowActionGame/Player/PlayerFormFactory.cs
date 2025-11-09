using MultiWindowActionGame.DI;
using MultiWindowActionGame.Interfaces;

namespace MultiWindowActionGame.Player
{
    public class PlayerFormFactory : IPlayerFormFactory
    {
        private readonly IGameSettings gameSettings;
        private readonly IWindowManager windowManager;
        private readonly INoEntryZoneManager noEntryZoneManager;
        private readonly IServiceContainer container;

        public PlayerFormFactory(
            IGameSettings gameSettings,
            IWindowManager windowManager,
            INoEntryZoneManager noEntryZoneManager,
            IServiceContainer container)
        {
            this.gameSettings = gameSettings ?? throw new ArgumentNullException(nameof(gameSettings));
            this.windowManager = windowManager ?? throw new ArgumentNullException(nameof(windowManager));
            this.noEntryZoneManager = noEntryZoneManager ?? throw new ArgumentNullException(nameof(noEntryZoneManager));
            this.container = container ?? throw new ArgumentNullException(nameof(container));
        }

        public PlayerForm CreatePlayer(Point startPosition)
        {
            // DIコンテナからPlayerコンポーネントを解決
            var inputHandler = container.Resolve<IPlayerInputHandler>();
            var physics = container.Resolve<IPlayerPhysics>();
            var stateMachine = container.Resolve<IPlayerStateMachine>();

            // PlayerFormを作成
            var playerForm = new PlayerForm(
                startPosition,
                gameSettings,
                windowManager,
                noEntryZoneManager,
                inputHandler,
                physics,
                null, // windowInteractionは後で設定
                stateMachine);

            // PlayerWindowInteractionを作成（PlayerFormの参照を渡す）
            var windowInteraction = new PlayerWindowInteraction(
                windowManager,
                gameSettings.Player.DefaultSize,
                noEntryZoneManager,
                playerForm);

            // PlayerFormにwindowInteractionを設定
            playerForm.SetWindowInteraction(windowInteraction);

            return playerForm;
        }
    }
}