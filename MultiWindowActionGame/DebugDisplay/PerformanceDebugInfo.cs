using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using MultiWindowActionGame.Core;
using MultiWindowActionGame.Interfaces;

namespace MultiWindowActionGame.Debug
{
    public class PerformanceDebugInfo
    {
        private static readonly Font DebugFont = new Font("Consolas", 10);
        private readonly IPerformanceMonitor? performanceMonitor;

        public PerformanceDebugInfo(IPerformanceMonitor? performanceMonitor = null)
        {
            this.performanceMonitor = performanceMonitor;
        }

        private IPerformanceMonitor GetPerformanceMonitorSafely()
        {
            return performanceMonitor ?? PerformanceMonitor.Current;
        }
        private const int PANEL_X = 1600;
        private const int PANEL_Y = 100;  // プレイヤーとウィンドウ情報の下に表示

        public void Draw(Graphics g)
        {
            if (!MainGame.IsDebugMode) return;

            var monitor = GetPerformanceMonitorSafely();
            var performanceInfo = new List<string>
            {
                "=== Performance ===",
                $"FPS: {monitor.GetCurrentFPS():F1}",
                $"Frame Time: {monitor.GetAverageFrameTime() * 1000:F2}ms",
                "",
                "Timings:"
            };

            foreach (var timing in monitor.GetTimings().OrderByDescending(t => t.Value))
            {
                performanceInfo.Add($"  {timing.Key}: {timing.Value:F2}ms");
            }

            DrawInfoPanel(g, performanceInfo.ToArray(), new Point(PANEL_X, PANEL_Y));
        }

        private void DrawInfoPanel(Graphics g, string[] lines, Point location)
        {
            var padding = 5;
            var lineHeight = DebugFont.Height + 2;
            var blockHeight = lines.Length * lineHeight + padding * 2;
            var blockWidth = lines.Max(l => TextRenderer.MeasureText(l, DebugFont).Width) + padding * 2;

            using (var brush = new SolidBrush(Color.FromArgb(180, Color.DarkRed)))
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
