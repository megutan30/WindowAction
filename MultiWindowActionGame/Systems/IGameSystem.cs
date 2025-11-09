using System;
using System.Threading.Tasks;

namespace MultiWindowActionGame.Core.Systems
{
    public enum GameSystemPriority
    {
        Input = 100,
        Physics = 200,
        Logic = 300,
        Rendering = 400,
        UI = 500
    }

    public interface IGameSystem
    {
        string SystemName { get; }
        GameSystemPriority Priority { get; }
        bool IsEnabled { get; set; }
        
        Task InitializeAsync();
        Task UpdateAsync(float deltaTime);
        void Shutdown();
        
        void Pause();
        void Resume();
        bool IsPaused { get; }
    }
}