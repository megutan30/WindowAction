using MultiWindowActionGame.UI;

namespace MultiWindowActionGame.UI
{
    public interface IButtonFactory
    {
        RetryButton CreateRetryButton(Point position);
        StartButton CreateStartButton(Point position);
        ToTitleButton CreateToTitleButton(Point position);
        ExitButton CreateExitButton(Point position);
    }
}