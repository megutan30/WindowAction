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

        // 放置警告用
        private bool isIdleWarningVisible = false;
        private int idleWarningRemainingSeconds = 0;

        public bool IsIdleWarningVisible => isIdleWarningVisible;

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
            // 放置警告の描画（デバッグモードに関係なく表示）
            DrawIdleWarning(g);

            // 通常の通知（デバッグモードのみ）
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

        public void ShowIdleWarning(int remainingSeconds)
        {
            isIdleWarningVisible = true;
            idleWarningRemainingSeconds = remainingSeconds;
        }

        public void HideIdleWarning()
        {
            isIdleWarningVisible = false;
            idleWarningRemainingSeconds = 0;
        }

        private void DrawIdleWarning(Graphics g)
        {
            if (!isIdleWarningVisible) return;

            try
            {
                // 画面サイズを取得
                var screenBounds = Program.mainForm?.ClientRectangle ?? new Rectangle(0, 0, 800, 600);

                // 半透明の背景を描画
                using (var bgBrush = new SolidBrush(Color.FromArgb(180, 0, 0, 0)))
                {
                    int boxWidth = 500;
                    int boxHeight = 80;
                    int boxX = (screenBounds.Width - boxWidth) / 2;
                    int boxY = (screenBounds.Height - boxHeight) / 2;

                    g.FillRectangle(bgBrush, boxX, boxY, boxWidth, boxHeight);

                    // 枠線
                    using (var borderPen = new Pen(Color.OrangeRed, 3))
                    {
                        g.DrawRectangle(borderPen, boxX, boxY, boxWidth, boxHeight);
                    }
                }

                // 警告メッセージを描画
                string message = $"放置されています\nあと {idleWarningRemainingSeconds} 秒でタイトルに戻ります";

                using (var font = new Font("Yu Gothic UI", 16, FontStyle.Bold))
                using (var brush = new SolidBrush(Color.OrangeRed))
                {
                    var format = new StringFormat
                    {
                        Alignment = StringAlignment.Center,
                        LineAlignment = StringAlignment.Center
                    };

                    g.DrawString(message, font, brush, screenBounds, format);
                }
            }
            catch
            {
                // 描画エラーは無視
            }
        }
    }
}