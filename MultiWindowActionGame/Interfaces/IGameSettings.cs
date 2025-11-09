using System;
using MultiWindowActionGame.Utilities;
using static MultiWindowActionGame.Utilities.GameSettings;

namespace MultiWindowActionGame.Interfaces
{
    public interface IGameSettings : IDisposable
    {
        PlayerSettings Player { get; }
        WindowSettings Window { get; }
        GameplaySettings Gameplay { get; }

        event EventHandler<SettingsChangedEventArgs>? SettingsChanged;

        void UpdatePlayerSettings(PlayerSettings newSettings);
        void UpdateWindowSettings(WindowSettings newSettings);
        void UpdateGameplaySettings(GameplaySettings newSettings);
        void SaveSettings();
    }
}