using MultiWindowActionGame.Interfaces;

namespace MultiWindowActionGame.UI
{
    public class ButtonFactory : IButtonFactory
    {
        private readonly IWindowManager windowManager;

        public ButtonFactory(IWindowManager windowManager)
        {
            this.windowManager = windowManager ?? throw new ArgumentNullException(nameof(windowManager));
        }

        public RetryButton CreateRetryButton(Point position)
        {
            return new RetryButton(position, null, windowManager);
        }

        public StartButton CreateStartButton(Point position)
        {
            return new StartButton(position, null, windowManager);
        }

        public ToTitleButton CreateToTitleButton(Point position)
        {
            return new ToTitleButton(position, null, windowManager);
        }

        public ExitButton CreateExitButton(Point position)
        {
            return new ExitButton(position, windowManager);
        }
    }
}