using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using MultiWindowActionGame.Core;
using MultiWindowActionGame.Managers;

namespace MultiWindowActionGame.Debug
{
    public class DesktopIconDebugInfo
    {
        private readonly IDesktopIconManager iconManager;
        private static readonly Font DebugFont = new Font("Consolas", 9);
        private const int PANEL_X = 10;
        private const int PANEL_Y = 330; // PlayerDebugInfoの下に表示（OverlayFormの座標変換考慮）

        // キャッシュされたアイコン情報（起動時に一度だけ取得）
        private List<DesktopIcon> cachedIcons = new List<DesktopIcon>();
        private bool iconsCached = false;

        public DesktopIconDebugInfo(IDesktopIconManager iconManager)
        {
            this.iconManager = iconManager ?? throw new ArgumentNullException(nameof(iconManager));
        }

        public void Draw(Graphics g)
        {
            if (!MainGame.IsDebugMode) return;

            try
            {
                DrawIconInfo(g);
                DrawIconBounds(g);
            }
            catch (Exception ex)
            {
                // デバッグ表示でエラーが発生してもゲームを止めない
                using (var brush = new SolidBrush(Color.Red))
                {
                    g.DrawString($"Icon Debug Error: {ex.Message}", DebugFont, brush, PANEL_X, PANEL_Y);
                }
            }
        }

        private void DrawIconInfo(Graphics g)
        {
            if (!iconManager.IsInitialized)
            {
                var notInitInfo = new[]
                {
                    "=== Desktop Icons ===",
                    "Initializing...",
                    "Check logs/desktop_icons.log",
                    "for detailed information"
                };
                DrawInfoPanel(g, notInitInfo, new Point(PANEL_X, PANEL_Y), Color.FromArgb(180, Color.DarkRed));
                return;
            }

            // 起動時に一度だけアイコン情報をキャッシュ
            if (!iconsCached)
            {
                cachedIcons = iconManager.GetDesktopIcons();
                iconsCached = true;
            }

            var visibleArea = Screen.PrimaryScreen.Bounds;
            var visibleIcons = cachedIcons.Where(icon => icon.Bounds.IntersectsWith(visibleArea)).ToList();

            // 座標系デバッグ情報を追加
            var mainFormLocation = Program.mainForm?.Location ?? Point.Empty;
            var mainFormSize = Program.mainForm?.Size ?? Size.Empty;
            var screenBounds = Screen.PrimaryScreen.Bounds;
            var workingArea = Screen.PrimaryScreen.WorkingArea;

            var iconInfo = new List<string>
            {
                "=== Desktop Icons (Clickable Area) ===",
                $"Total Icons: {cachedIcons.Count}",
                $"Visible Icons: {visibleIcons.Count}",
                $"MainForm: Loc={mainFormLocation} Size={mainFormSize}",
                $"Screen: {screenBounds} Work: {workingArea}",
                $"Debug Panel: ({PANEL_X}, {PANEL_Y})",
                "(Green=Clickable Area)",
                ""
            };

            // 表示中のアイコンの詳細情報（最大5個まで）
            foreach (var icon in visibleIcons.Take(5))
            {
                var name = string.IsNullOrEmpty(icon.Name) ? "Unknown" : icon.Name;
                if (name.Length > 20) name = name.Substring(0, 17) + "...";
                iconInfo.Add($"{name}: {icon.Bounds.Width}x{icon.Bounds.Height}");
            }

            if (visibleIcons.Count > 5)
            {
                iconInfo.Add($"... and {visibleIcons.Count - 5} more");
            }

            DrawInfoPanel(g, iconInfo.ToArray(), new Point(PANEL_X, PANEL_Y), Color.FromArgb(180, Color.DarkMagenta));
        }

        private void DrawIconBounds(Graphics g)
        {
            if (!iconManager.IsInitialized || !iconsCached) return;

            var visibleArea = Screen.PrimaryScreen.Bounds;

            // キャッシュされたアイコン位置を使用（パフォーマンス最適化）
            var visibleIcons = cachedIcons.Where(icon => icon.Bounds.IntersectsWith(visibleArea));

            foreach (var icon in visibleIcons)
            {
                DrawSingleIconBounds(g, icon);
            }
        }

        private void DrawSingleIconBounds(Graphics g, DesktopIcon icon)
        {
            // クリック可能領域を緑色の実線で描画（表示境界と衝突境界が同じ）
            using (var pen = new Pen(Color.LimeGreen, 2))
            {
                g.DrawRectangle(pen, icon.Bounds);
            }

            // アイコン名を表示（短縮）
            if (!string.IsNullOrEmpty(icon.Name))
            {
                var displayName = icon.Name.Length > 15 ? icon.Name.Substring(0, 12) + "..." : icon.Name;
                var textSize = g.MeasureString(displayName, DebugFont);

                // 背景を描画してテキストを読みやすくする
                var textBounds = new RectangleF(
                    icon.Bounds.X,
                    icon.Bounds.Bottom + 2,
                    textSize.Width + 4,
                    textSize.Height + 2
                );

                using (var brush = new SolidBrush(Color.FromArgb(180, Color.Black)))
                {
                    g.FillRectangle(brush, textBounds);
                }

                using (var brush = new SolidBrush(Color.LimeGreen))
                {
                    g.DrawString(displayName, DebugFont, brush, icon.Bounds.X + 2, icon.Bounds.Bottom + 3);
                }
            }

            // クリック可能領域の中心点を描画
            using (var brush = new SolidBrush(Color.Yellow))
            {
                var centerX = icon.Bounds.X + icon.Bounds.Width / 2;
                var centerY = icon.Bounds.Y + icon.Bounds.Height / 2;
                g.FillEllipse(brush, centerX - 2, centerY - 2, 4, 4);
            }
        }

        private void DrawInfoPanel(Graphics g, string[] lines, Point location, Color backgroundColor)
        {
            var padding = 5;
            var lineHeight = DebugFont.Height + 2;
            var blockHeight = lines.Length * lineHeight + padding * 2;
            var blockWidth = lines.Max(l => TextRenderer.MeasureText(l, DebugFont).Width) + padding * 2;

            using (var brush = new SolidBrush(backgroundColor))
            {
                g.FillRectangle(brush, location.X, location.Y, blockWidth, blockHeight);
            }

            using (var brush = new SolidBrush(Color.White))
            {
                var y = location.Y + padding;
                foreach (var line in lines)
                {
                    g.DrawString(line, DebugFont, brush, location.X + padding, y);
                    y += lineHeight;
                }
            }
        }
    }
}