using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using MultiWindowActionGame.Core;
using MultiWindowActionGame.Utilities;
using MultiWindowActionGame.Windows;

namespace MultiWindowActionGame.Debug
{
    public class DebugDisplay
    {
        public static void DrawSettingsInfo(Graphics g, Point position)
        {
            if (!MainGame.IsDebugMode) return;

            // TODO: GameSettings.Current - 優先度:低（実装不要）
            // 判断理由（2025-01-11）:
            // - デバッグ機能のため本番環境で問題にならない
            // - ROI低（30分実装に対して具体的メリットなし）
            // - Phase 1-7で主要な静的参照は完全削除済み（DI化達成率88%以上）
            // 再検討条件: UI/レンダリング設計の大規模見直し時
            var settings = GameSettings.Current;
            var player = settings.Player;
            var window = settings.Window;
            var gameplay = settings.Gameplay;

            var debugInfo = new List<string>
            {
                "=== Settings ===",
                $"Player Speed: {player.MovementSpeed}",
                $"Gravity: {player.Gravity}",
                $"Jump Force: {player.JumpForce}",
                $"Window Min Size: {window.MinimumSize}",
                $"Target FPS: {gameplay.TargetFPS}"
            };

            using (var font = new Font("Arial", 10))
            using (var brush = new SolidBrush(Color.Yellow))
            {
                float y = position.Y;
                foreach (var line in debugInfo)
                {
                    g.DrawString(line, font, brush, position.X, y);
                    y += 15;
                }
            }
        }
    }
}
