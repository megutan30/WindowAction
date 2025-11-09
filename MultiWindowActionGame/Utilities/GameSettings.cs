using System.Diagnostics;
using System.Text.Json;
using MultiWindowActionGame.Interfaces;

namespace MultiWindowActionGame.Utilities
{
    public class GameSettings : IGameSettings, IDisposable
    {
        // インスタンス統一化のためのstatic参照
        private static GameSettings? _current;
        public static GameSettings Current
        {
            get => _current ?? throw new InvalidOperationException("GameSettings is not initialized");
            internal set => _current = value;
        }
        
        private const string DEFAULT_SETTINGS_PATH = "config/settings.json";
        private static string settingsPath = DEFAULT_SETTINGS_PATH;
        private FileSystemWatcher? settingsWatcher;

        public event EventHandler<SettingsChangedEventArgs>? SettingsChanged;

        public enum SettingType
        {
            Player,
            Window,
            Gameplay,
            All
        }

        public class SettingsChangedEventArgs : EventArgs
        {
            public SettingType Type { get; }
            public SettingsChangedEventArgs(SettingType type)
            {
                Type = type;
            }
        }

        public class PlayerSettings : ICloneable, IEquatable<PlayerSettings>
        {
            public float MovementSpeed { get; set; }
            public float Gravity { get; set; }
            public float JumpForce { get; set; }
            public Size DefaultSize { get; set; }
            public int GroundCheckHeight { get; set; }

            public object Clone()
            {
                return new PlayerSettings
                {
                    MovementSpeed = this.MovementSpeed,
                    Gravity = this.Gravity,
                    JumpForce = this.JumpForce,
                    DefaultSize = this.DefaultSize,
                    GroundCheckHeight = this.GroundCheckHeight
                };
            }

            public bool Equals(PlayerSettings? other)
            {
                if (other is null) return false;
                return MovementSpeed == other.MovementSpeed &&
                       Gravity == other.Gravity &&
                       JumpForce == other.JumpForce &&
                       DefaultSize == other.DefaultSize &&
                       GroundCheckHeight == other.GroundCheckHeight;
            }
        }

        public class WindowSettings : ICloneable, IEquatable<WindowSettings>
        {
            public Size MinimumSize { get; set; }

            public object Clone()
            {
                return new WindowSettings
                {
                    MinimumSize = this.MinimumSize
                };
            }

            public bool Equals(WindowSettings? other)
            {
                if (other is null) return false;
                return MinimumSize == other.MinimumSize;
            }
        }

        public class GameplaySettings : ICloneable, IEquatable<GameplaySettings>
        {
            public int TargetFPS { get; set; }
            public float WindowSnapDistance { get; set; }

            public object Clone()
            {
                return new GameplaySettings
                {
                    TargetFPS = this.TargetFPS,
                    WindowSnapDistance = this.WindowSnapDistance
                };
            }

            public bool Equals(GameplaySettings? other)
            {
                if (other is null) return false;
                return TargetFPS == other.TargetFPS &&
                       WindowSnapDistance == other.WindowSnapDistance;
            }
        }

        public PlayerSettings Player { get; private set; } = new();
        public WindowSettings Window { get; private set; } = new();
        public GameplaySettings Gameplay { get; private set; } = new();
        private readonly INotificationService? notificationService;

        public GameSettings(INotificationService? notificationService = null)
        {
            this.notificationService = notificationService;
            LoadSettings();
            InitializeFileWatcher();
        }

        private void InitializeFileWatcher()
        {
            var directory = Path.GetDirectoryName(settingsPath);
            var filename = Path.GetFileName(settingsPath);

            if (directory != null)
            {
                settingsWatcher = new FileSystemWatcher(directory, filename)
                {
                    NotifyFilter = NotifyFilters.LastWrite,
                    EnableRaisingEvents = true
                };

                settingsWatcher.Changed += OnSettingsFileChanged;
            }
        }

        private void OnSettingsFileChanged(object sender, FileSystemEventArgs e)
        {
            Thread.Sleep(100);

            try
            {
                var oldSettings = new
                {
                    Player = (PlayerSettings)Player.Clone(),
                    Window = (WindowSettings)Window.Clone(),
                    Gameplay = (GameplaySettings)Gameplay.Clone()
                };

                LoadSettings();

                // 変更された設定を検出して通知
                if (!Player.Equals(oldSettings.Player))
                {
                    string details = GetChangedProperties(oldSettings.Player, Player);
                    notificationService?.AddNotification(SettingType.Player, details);
                }
                if (!Window.Equals(oldSettings.Window))
                {
                    string details = GetChangedProperties(oldSettings.Window, Window);
                    notificationService?.AddNotification(SettingType.Window, details);
                }
                if (!Gameplay.Equals(oldSettings.Gameplay))
                {
                    string details = GetChangedProperties(oldSettings.Gameplay, Gameplay);
                    notificationService?.AddNotification(SettingType.Gameplay, details);
                }

                OnSettingsChanged(SettingType.All);
            }
            catch (Exception ex)
            {
            }
        }

        private string GetChangedProperties<T>(T oldValue, T newValue)
        {
            var changes = new List<string>();
            var properties = typeof(T).GetProperties();

            foreach (var prop in properties)
            {
                var oldPropValue = prop.GetValue(oldValue);
                var newPropValue = prop.GetValue(newValue);

                if (!Equals(oldPropValue, newPropValue))
                {
                    changes.Add($"{prop.Name}: {oldPropValue} → {newPropValue}");
                }
            }

            return string.Join(", ", changes);
        }

        private void LoadSettings()
        {
            try
            {
                if (File.Exists(settingsPath))
                {
                    var json = File.ReadAllText(settingsPath);
                    var settings = JsonSerializer.Deserialize<GameSettingsData>(json);
                    if (settings != null)
                    {
                        Player = settings.Player;
                        Window = settings.Window;
                        Gameplay = settings.Gameplay;
                        return;
                    }
                }
            }
            catch (Exception ex)
            {
            }

            LoadDefaultSettings();
        }

        private void LoadDefaultSettings()
        {
            Player = new PlayerSettings
            {
                MovementSpeed = 400.0f,
                Gravity = 1000.0f,
                JumpForce = 700.0f,
                DefaultSize = new Size(60, 60),
                GroundCheckHeight = 15
            };

            Window = new WindowSettings
            {
                MinimumSize = new Size(100, 100),
            };

            Gameplay = new GameplaySettings
            {
                TargetFPS = 60,
                WindowSnapDistance = 20.0f
            };
        }

        public void SaveSettings()
        {
            try
            {
                var settingsData = new GameSettingsData
                {
                    Player = Player,
                    Window = Window,
                    Gameplay = Gameplay
                };

                var options = new JsonSerializerOptions
                {
                    WriteIndented = true
                };

                var json = JsonSerializer.Serialize(settingsData, options);
                var directory = Path.GetDirectoryName(settingsPath);
                if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
                {
                    Directory.CreateDirectory(directory);
                }
                File.WriteAllText(settingsPath, json);
            }
            catch (Exception ex)
            {
            }
        }

        public static void SetSettingsPath(string path)
        {
            settingsPath = path;
        }

        public void UpdatePlayerSettings(PlayerSettings newSettings)
        {
            var oldSettings = Player;
            Player = newSettings;

            if (!Player.Equals(oldSettings))
            {
                string details = GetChangedProperties(oldSettings, Player);
                notificationService?.AddNotification(SettingType.Player, details);
                OnSettingsChanged(SettingType.Player);
            }
        }

        public void UpdateWindowSettings(WindowSettings newSettings)
        {
            var oldSettings = Window;
            Window = newSettings;

            if (!Window.Equals(oldSettings))
            {
                string details = GetChangedProperties(oldSettings, Window);
                notificationService?.AddNotification(SettingType.Window, details);
                OnSettingsChanged(SettingType.Window);
            }
        }

        public void UpdateGameplaySettings(GameplaySettings newSettings)
        {
            var oldSettings = Gameplay;
            Gameplay = newSettings;

            if (!Gameplay.Equals(oldSettings))
            {
                string details = GetChangedProperties(oldSettings, Gameplay);
                notificationService?.AddNotification(SettingType.Gameplay, details);
                OnSettingsChanged(SettingType.Gameplay);
            }
        }

        protected virtual void OnSettingsChanged(SettingType type)
        {
            SettingsChanged?.Invoke(this, new SettingsChangedEventArgs(type));
        }

        private class GameSettingsData
        {
            public PlayerSettings Player { get; set; } = new();
            public WindowSettings Window { get; set; } = new();
            public GameplaySettings Gameplay { get; set; } = new();
        }

        public void Dispose()
        {
            settingsWatcher?.Dispose();
        }
    }
}