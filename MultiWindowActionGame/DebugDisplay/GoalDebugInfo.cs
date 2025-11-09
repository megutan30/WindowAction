using MultiWindowActionGame.Windows;
using MultiWindowActionGame.Core;

namespace MultiWindowActionGame.Debug
{
    public class GoalDebugInfo
    {
        private readonly Goal goal;
        private const int DEBUG_PANEL_X = 10;
        private const int DEBUG_PANEL_Y = 250;
        private static readonly Font DebugFont = new Font("Consolas", 10);

        public GoalDebugInfo(Goal goal)
        {
            this.goal = goal;
        }

        public void Draw(Graphics g)
        {
            if (!MainGame.IsDebugMode) return;

            // ゴールに関する情報をデスクトップに表示
            var stateInfo = new[]
            {
                "=== Goal Debug ===",
                $"Collision Pos: ({goal.Bounds.X}, {goal.Bounds.Y})",
                $"Collision Size: {goal.Bounds.Width}x{goal.Bounds.Height}",
                $"Display Pos: ({goal.Location.X}, {goal.Location.Y})",
                $"Display Size: {goal.Size.Width}x{goal.Size.Height}",
                $"Parent Window: {(goal.Parent != null ? goal.Parent.Id.ToString() : "None")}"
            };

            DrawInfoPanel(g, stateInfo, new Point(DEBUG_PANEL_X, DEBUG_PANEL_Y));

            // ゴールの各領域を線で描画
            DrawGoalBounds(g);
        }

        private void DrawGoalBounds(Graphics g)
        {
            // 当たり判定領域を赤の線で描画
            var collisionBounds = goal.CollisionBounds;
            using (var collisionPen = new Pen(Color.Red, 2))
            {
                g.DrawRectangle(collisionPen, collisionBounds);
            }

            // 描画領域をオレンジの線で描画
            var renderBounds = goal.RenderBounds;
            using (var renderPen = new Pen(Color.Orange, 2))
            {
                renderPen.DashStyle = System.Drawing.Drawing2D.DashStyle.Dash;
                g.DrawRectangle(renderPen, renderBounds);
            }

            // 表示領域（Form領域）を黄色の線で描画
            var displayBounds = goal.DisplayBounds;
            using (var displayPen = new Pen(Color.Yellow, 2))
            {
                displayPen.DashStyle = System.Drawing.Drawing2D.DashStyle.Dot;
                g.DrawRectangle(displayPen, displayBounds);
            }

            // 凡例を表示
            DrawBoundsLegend(g);
        }

        private void DrawBoundsLegend(Graphics g)
        {
            var legendX = DEBUG_PANEL_X + 350;
            var legendY = DEBUG_PANEL_Y;
            var lineHeight = 20;

            var legends = new[]
            {
                (Color.Red, "━━", "Collision Bounds (当たり判定)"),
                (Color.Orange, "- - -", "Render Bounds (描画領域)"),
                (Color.Yellow, "· · ·", "Display Bounds (表示領域)")
            };

            using (var font = new Font("Consolas", 9))
            using (var brush = new SolidBrush(Color.White))
            using (var bgBrush = new SolidBrush(Color.FromArgb(180, Color.Black)))
            {
                // 背景
                var legendWidth = 280;
                var legendHeight = legends.Length * lineHeight + 10;
                g.FillRectangle(bgBrush, legendX, legendY, legendWidth, legendHeight);

                // 各凡例
                for (int i = 0; i < legends.Length; i++)
                {
                    var (color, pattern, text) = legends[i];
                    var y = legendY + 5 + (i * lineHeight);

                    // 色のサンプル線
                    using (var pen = new Pen(color, 2))
                    {
                        if (pattern.Contains('-'))
                            pen.DashStyle = System.Drawing.Drawing2D.DashStyle.Dash;
                        else if (pattern.Contains('·'))
                            pen.DashStyle = System.Drawing.Drawing2D.DashStyle.Dot;

                        g.DrawLine(pen, legendX + 5, y + 8, legendX + 30, y + 8);
                    }

                    // テキスト
                    g.DrawString(text, font, brush, legendX + 40, y);
                }
            }
        }

        private void DrawInfoPanel(Graphics g, string[] lines, Point location)
        {
            // 半透明の背景パネル
            var padding = 5;
            var lineHeight = DebugFont.Height + 2;
            var blockHeight = lines.Length * lineHeight + padding * 2;
            var blockWidth = lines.Max(l => TextRenderer.MeasureText(l, DebugFont).Width) + padding * 2;

            using (var brush = new SolidBrush(Color.FromArgb(180, Color.Black)))
            {
                g.FillRectangle(brush, location.X, location.Y, blockWidth, blockHeight);
            }

            // テキストの描画
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
