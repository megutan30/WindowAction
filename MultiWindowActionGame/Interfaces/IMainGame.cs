using System;
using System.Drawing;
using System.Threading.Tasks;
using MultiWindowActionGame.Player;

namespace MultiWindowActionGame.Interfaces
{
    public interface IMainGame
    {
        bool IsDebugMode { get; }

        Task InitializeAsync();
        void InitializePlayer(Point startPosition);
        PlayerForm? GetPlayer();

        void PauseGame();
        void ResumeGame();
        void ResetStageTransition();

        Task RunGameLoopAsync();

        // InputSystemからデバッグモード切り替え
        void ToggleDebugMode();
    }
}