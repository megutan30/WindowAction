using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using MultiWindowActionGame.Windows;

namespace MultiWindowActionGame.Rendering
{
    public static class OutlineRenderer
    {
        private const float OUTLINE_WIDTH = 5.0f;

        public static Color CalculateOutlineColor(Color parentColor)
        {
            float brightness = (parentColor.R * 0.299f +
                              parentColor.G * 0.587f +
                              parentColor.B * 0.114f) / 255f;

            return brightness < 0.5f ?
                Color.FromArgb(
                    Math.Min(255, parentColor.R + 100),
                    Math.Min(255, parentColor.G + 100),
                    Math.Min(255, parentColor.B + 100)
                ) :
                Color.FromArgb(
                    Math.Max(0, parentColor.R - 50),
                    Math.Max(0, parentColor.G - 50),
                    Math.Max(0, parentColor.B - 50)
                );
        }
        public static void DrawFormOutline(Graphics g, Rectangle bounds, Color outlineColor)
        {
            using (var pen = new Pen(outlineColor, OUTLINE_WIDTH))
            {
                g.DrawRectangle(pen, bounds);
            }
        }
        public static void DrawTextOutline(Graphics g, string text, Font font, Color outlineColor,
            float offset, PointF location)
        {
            for (int x = -1; x <= 1; x++)
            {
                for (int y = -1; y <= 1; y++)
                {
                    if (x != 0 || y != 0)
                    {
                        g.DrawString(text, font, new SolidBrush(outlineColor),
                            location.X + x * offset,
                            location.Y + y * offset);
                    }
                }
            }
        }
        public static void DrawClippedOutline(Graphics g, IEffectTarget target,
          IEnumerable<IEffectTarget> coveringTargets, Rectangle outlineBounds)
        {
            // 不可侵ウィンドウの場合は赤色アウトラインを描画（親子関係に関係なく）
            if (target is GameWindow window && window.IsNoEntryWindow)
            {
                using (var clipRegion = new Region(outlineBounds))
                {
                    foreach (var coveringTarget in coveringTargets)
                    {
                        if (coveringTarget is GameWindow coveringWindow)
                        {
                            clipRegion.Exclude(coveringWindow.CollisionBounds);
                        }
                        else
                        {
                            clipRegion.Exclude(coveringTarget.Bounds);
                        }
                    }

                    var originalClip = g.Clip;
                    g.Clip = clipRegion;

                    DrawFormOutline(g, outlineBounds, Color.Red);

                    g.Clip = originalClip;
                }
                return;
            }

            // 通常の親子関係アウトライン処理
            if (target.Parent == null) return;

            using (var clipRegion = new Region(outlineBounds))
            {
                foreach (var coveringTarget in coveringTargets)
                {
                    // 被っているターゲットもGameWindowの場合はCollisionBoundsを使用
                    if (coveringTarget is GameWindow coveringWindow)
                    {
                        clipRegion.Exclude(coveringWindow.CollisionBounds);
                    }
                    else
                    {
                        clipRegion.Exclude(coveringTarget.Bounds);
                    }
                }

                var originalClip = g.Clip;
                g.Clip = clipRegion;

                var outlineColor = CalculateOutlineColor(target.Parent.BackColor);
                DrawFormOutline(g, outlineBounds, outlineColor);

                g.Clip = originalClip;
            }
        }
    }
}
