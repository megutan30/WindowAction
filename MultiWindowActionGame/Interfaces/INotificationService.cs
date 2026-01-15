using System.Drawing;
using MultiWindowActionGame.Utilities;

namespace MultiWindowActionGame.Interfaces
{
    public interface INotificationService
    {
        void AddNotification(GameSettings.SettingType type, string details);
        void Update(float deltaTime);
        void Draw(Graphics g);

        // 放置警告用
        void ShowIdleWarning(int remainingSeconds);
        void HideIdleWarning();
        bool IsIdleWarningVisible { get; }
    }
}