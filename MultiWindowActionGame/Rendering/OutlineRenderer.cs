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

        // NoEntryウィンドウのアニメーション用定数（NoEntryZoneと同じ値）
        private const int STRIPE_WIDTH = 20;
        private const int TOTAL_PATTERN_HEIGHT = STRIPE_WIDTH * 2;
        private const int BORDER_STRIP_WIDTH = 5;
        private const float ANIMATION_SPEED = 40f; // px/秒（NoEntryZoneの2px/50msと同等）
        private static float noEntryAnimationOffset = 0f;

        public static void UpdateAnimation(float deltaTime)
        {
            noEntryAnimationOffset = (noEntryAnimationOffset + deltaTime * ANIMATION_SPEED) % TOTAL_PATTERN_HEIGHT;
        }

        // 右回りに循環するアニメーション枠を描画する
        // 周回距離に基づいて各辺の縞パターンを決定し、辺ごとに方向を変えて描画する
        private static void DrawNoEntryAnimatedBorder(Graphics g, Rectangle bounds)
        {
            int offset = (int)noEntryAnimationOffset;
            int w = bounds.Width;
            int h = bounds.Height;

            // GameFormのTransparencyKey = Color.Blackのため、完全不透明の赤と濃いグレーを使用する
            using (var redBrush = new SolidBrush(Color.Red))
            using (var darkBrush = new SolidBrush(Color.FromArgb(30, 30, 30)))
            {
                // 上辺: 左→右 (周回距離 0〜W)
                DrawClockwiseSide(g, redBrush, darkBrush, true, true,
                    new Rectangle(bounds.X, bounds.Y, w, BORDER_STRIP_WIDTH),
                    0, w, offset);

                // 右辺: 上→下 (周回距離 W〜W+H)
                DrawClockwiseSide(g, redBrush, darkBrush, false, true,
                    new Rectangle(bounds.Right - BORDER_STRIP_WIDTH, bounds.Y, BORDER_STRIP_WIDTH, h),
                    w, h, offset);

                // 下辺: 右→左 (周回距離 W+H〜2W+H)
                DrawClockwiseSide(g, redBrush, darkBrush, true, false,
                    new Rectangle(bounds.X, bounds.Bottom - BORDER_STRIP_WIDTH, w, BORDER_STRIP_WIDTH),
                    w + h, w, offset);

                // 左辺: 下→上 (周回距離 2W+H〜2(W+H))
                DrawClockwiseSide(g, redBrush, darkBrush, false, false,
                    new Rectangle(bounds.X, bounds.Y, BORDER_STRIP_WIDTH, h),
                    2 * w + h, h, offset);
            }
        }

        // 1辺分の縞セグメントを描画する
        // horizontal: 水平辺かどうか、forward: 進行方向が座標軸と同じかどうか
        // periStart: この辺の開始周回距離、sideLength: この辺の長さ
        private static void DrawClockwiseSide(Graphics g, Brush redBrush, Brush darkBrush,
            bool horizontal, bool forward, Rectangle strip, int periStart, int sideLength, int offset)
        {
            // この辺の開始点での縞フェーズ
            int startPhase = (periStart + offset) % TOTAL_PATTERN_HEIGHT;
            int pos = 0;

            while (pos < sideLength)
            {
                int phase = (startPhase + pos) % TOTAL_PATTERN_HEIGHT;
                bool isRed = phase < STRIPE_WIDTH;

                // 次の色変化までの長さ（辺末端も考慮）
                int segLength = Math.Min(
                    isRed ? STRIPE_WIDTH - phase : TOTAL_PATTERN_HEIGHT - phase,
                    sideLength - pos
                );

                // 周回方向を座標に変換して矩形を作成
                Rectangle segRect;
                if (horizontal)
                {
                    int drawX = forward ? (strip.X + pos) : (strip.Right - pos - segLength);
                    segRect = new Rectangle(drawX, strip.Y, segLength, strip.Height);
                }
                else
                {
                    int drawY = forward ? (strip.Y + pos) : (strip.Bottom - pos - segLength);
                    segRect = new Rectangle(strip.X, drawY, strip.Width, segLength);
                }

                g.FillRectangle(isRed ? redBrush : darkBrush, segRect);
                pos += segLength;
            }
        }

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

                    DrawNoEntryAnimatedBorder(g, outlineBounds);

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
