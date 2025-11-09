using System.Diagnostics;
using MultiWindowActionGame.Interfaces;

namespace MultiWindowActionGame.Services
{
    public class GameTimeService : IGameTimeService
    {
        private Stopwatch stopwatch = new Stopwatch();
        private long lastTime = 0;
        private bool isPaused = false;

        public float DeltaTime { get; private set; }
        public float TotalTime { get; private set; }

        public void Start()
        {
            stopwatch.Start();
        }

        public void SetPaused(bool paused)
        {
            isPaused = paused;
            if (!paused)
            {
                // ポーズ解除時に前回の時間を現在の時間に更新
                lastTime = stopwatch.ElapsedMilliseconds;
            }
        }

        public void Update()
        {
            long currentTime = stopwatch.ElapsedMilliseconds;
            // ポーズ中はDeltaTimeを0にする
            DeltaTime = isPaused ? 0 : (currentTime - lastTime) / 1000f;
            TotalTime = currentTime / 1000f;
            if (!isPaused)
            {
                lastTime = currentTime;
            }
        }
    }
}