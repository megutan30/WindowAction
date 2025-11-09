namespace MultiWindowActionGame.Interfaces
{
    public interface IGameTimeService
    {
        float DeltaTime { get; }
        float TotalTime { get; }

        void Start();
        void SetPaused(bool paused);
        void Update();
    }
}