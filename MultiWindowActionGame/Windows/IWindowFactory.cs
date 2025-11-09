using MultiWindowActionGame.Windows;

namespace MultiWindowActionGame.Windows
{
    public interface IWindowFactory
    {
        GameWindow CreateWindow(WindowType type, Point location, Size size, string? text = null, bool showImmediately = true);
        Goal CreateGoal(Point location, bool isInFront = false);
        NoEntryZone CreateNoEntryZone(Point location, Size size);
    }
}