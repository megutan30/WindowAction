using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using MultiWindowActionGame.Windows;
using MultiWindowActionGame.Extensions;
using MultiWindowActionGame.Core;

namespace MultiWindowActionGame.Debug
{
    public class WindowDebugInfo
    {
        private static readonly Font DebugFont = new Font("Consolas", 10);
        private readonly WindowManager windowManager;
        private const int PANEL_MARGIN = 400; // プレイヤー情報とかぶらないようにする

        public WindowDebugInfo(WindowManager windowManager)
        {
            this.windowManager = windowManager;
        }

        public void Draw(Graphics g)
        {
            if (!MainGame.IsDebugMode) return;

            DrawWindowHierarchy(g);
            DrawActiveEffects(g);
            DrawZOrderInfo(g);
        }

        private void DrawWindowHierarchy(Graphics g)
        {
            var windows = windowManager.GetAllWindows();
            var hierarchyInfo = new List<string>
        {
            "=== Window Hierarchy ===",
        };

            foreach (var window in windows)
            {
                var indent = GetHierarchyIndent(window);
                var info = $"{indent}{window.Id} ({window.Strategy.GetType().Name.Replace("WindowStrategy", "")})";
                if (window.Parent != null)
                {
                    info += $" → Parent: {window.Parent.Id}";
                }
                hierarchyInfo.Add(info);
            }

            DrawInfoPanel(g, hierarchyInfo.ToArray(), new Point(PANEL_MARGIN, 10), Color.FromArgb(180, Color.DarkBlue));
        }

        private string GetHierarchyIndent(GameWindow window)
        {
            var depth = 0;
            var current = window;
            while (current.Parent != null)
            {
                depth++;
                current = current.Parent;
            }
            return new string(' ', depth * 2);
        }

        private void DrawActiveEffects(Graphics g)
        {
            // 表示中のウィンドウのみ処理
            var visibleArea = Screen.PrimaryScreen.Bounds;
            var windows = windowManager.GetAllWindows()
                .Where(w => w.Bounds.IntersectsWith(visibleArea));

            var effectInfo = new List<string>
            {
                "=== Active Effects ==="
            };

            foreach (var window in windows.Where(w => w.HasActiveEffects))
            {
                effectInfo.Add($"Window {window.Id}:");
                foreach (var effect in window.GetActiveEffects())
                {
                    effectInfo.Add($"  - {effect.Type} ({(effect.IsActive ? "Active" : "Inactive")})");
                }
            }

            if (effectInfo.Count > 1) // ヘッダーだけでない場合
            {
                DrawInfoPanel(g, effectInfo.ToArray(), new Point(PANEL_MARGIN, 200), Color.FromArgb(180, Color.DarkGreen));
            }
        }

        private void DrawZOrderInfo(Graphics g)
        {
            // 表示中のウィンドウのみ処理（パフォーマンス最適化）
            var visibleArea = Screen.PrimaryScreen.Bounds;
            var zOrderedWindows = windowManager.GetAllWindows()
                .Where(w => w.Bounds.IntersectsWith(visibleArea))
                .OrderBy(w => windowManager.GetWindowZIndex(w));

            foreach (var window in zOrderedWindows)
            {
                var bounds = window.CollisionBounds;
                var zIndex = windowManager.GetWindowZIndex(window);

                // Z-orderを示す数字を描画
                using (var brush = new SolidBrush(Color.FromArgb(150, Color.Yellow)))
                {
                    g.DrawString(
                        zIndex.ToString(),
                        DebugFont,
                        brush,
                        bounds.Right - 30,
                        bounds.Top + 5
                    );
                }

                // 親子関係を示す線を描画（父も表示中の場合のみ）
                if (window.Parent != null && window.Parent.Bounds.IntersectsWith(visibleArea))
                {
                    using (var pen = new Pen(Color.FromArgb(100, Color.Yellow), 1))
                    {
                        pen.DashStyle = System.Drawing.Drawing2D.DashStyle.Dash;
                        var from = window.Bounds.Center();
                        var to = window.Parent.Bounds.Center();
                        DrawArrowLine(g, pen, from, to);
                    }
                }
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

        private void DrawArrowLine(Graphics g, Pen pen, Point from, Point to)
        {
            g.DrawLine(pen, from, to);

            // 矢印の先端を描画
            var angle = Math.Atan2(to.Y - from.Y, to.X - from.X);
            var arrowSize = 10;
            var arrowAngle = Math.PI / 6;

            var arrowPoint1 = new Point(
                (int)(to.X - arrowSize * Math.Cos(angle + arrowAngle)),
                (int)(to.Y - arrowSize * Math.Sin(angle + arrowAngle))
            );
            var arrowPoint2 = new Point(
                (int)(to.X - arrowSize * Math.Cos(angle - arrowAngle)),
                (int)(to.Y - arrowSize * Math.Sin(angle - arrowAngle))
            );

            g.DrawLine(pen, to, arrowPoint1);
            g.DrawLine(pen, to, arrowPoint2);
        }
    }
}
