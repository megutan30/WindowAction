using System.Drawing;
using MultiWindowActionGame.Interfaces;
using MultiWindowActionGame.Utilities;
using MultiWindowActionGame.Core;

namespace MultiWindowActionGame.Services
{
    public class NotificationService : INotificationService
    {
        private readonly Queue<NotificationInfo> notifications = new Queue<NotificationInfo>();
        private readonly float displayDuration = 3.0f;

        private class NotificationInfo
        {
            public string Message { get; set; }
            public float RemainingTime { get; set; }
            public Color Color { get; set; }

            public NotificationInfo(string message, float duration, Color color)
            {
                Message = message;
                RemainingTime = duration;
                Color = color;
            }
        }

        public void AddNotification(GameSettings.SettingType type, string details)
        {
            if (!MainGame.IsDebugMode) return;

            string message = $"Settings Changed: {type}";
            if (!string.IsNullOrEmpty(details))
            {
                message += $" - {details}";
            }

            Color color = type switch
            {
                GameSettings.SettingType.Player => Color.LightBlue,
                GameSettings.SettingType.Window => Color.LightGreen,
                GameSettings.SettingType.Gameplay => Color.LightYellow,
                _ => Color.White
            };

            notifications.Enqueue(new NotificationInfo(message, displayDuration, color));
        }

        public void Update(float deltaTime)
        {
            if (notifications.Count == 0) return;

            var current = notifications.Peek();
            current.RemainingTime -= deltaTime;

            if (current.RemainingTime <= 0)
            {
                notifications.Dequeue();
            }
        }

        public void Draw(Graphics g)
        {
            if (!MainGame.IsDebugMode || notifications.Count == 0) return;

            var current = notifications.Peek();
            float alpha = Math.Min(1.0f, current.RemainingTime / displayDuration);

            using (var font = new Font("Arial", 12))
            {
                Color textColor = Color.FromArgb(
                    (int)(alpha * 255),
                    current.Color.R,
                    current.Color.G,
                    current.Color.B
                );

                using (var brush = new SolidBrush(textColor))
                {
                    g.DrawString(current.Message, font, brush, 10, 50);
                }
            }
        }
    }
}