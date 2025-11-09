namespace MultiWindowActionGame.Windows
{
    public interface IWindowObserver
    {
        void OnWindowChanged(GameWindow window, WindowChangeType changeType);
    }

    public enum WindowChangeType
    {
        Moved,
        Resized,
        Deleted
    }

    public interface IWindowSubject
    {
        void AddObserver(IWindowObserver observer);
        void RemoveObserver(IWindowObserver observer);
        void NotifyObservers(WindowChangeType changeType);
    }
}