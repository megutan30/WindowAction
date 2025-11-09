using MultiWindowActionGame;
using MultiWindowActionGame.Player;
using MultiWindowActionGame.Core;
using MultiWindowActionGame.Extensions;

namespace MultiWindowActionGame.Debug
{
    public class PlayerDebugInfo
    {
        private readonly PlayerForm player;
        private const int DEBUG_PANEL_X = 10;
        private const int DEBUG_PANEL_Y = 10;
        private static readonly Font DebugFont = new Font("Consolas", 10);

        public PlayerDebugInfo(PlayerForm player)
        {
            this.player = player;
        }

        public void Draw(Graphics g)
        {
            if (!MainGame.IsDebugMode) return;

            // プレイヤーに関する情報をデスクトップの左上に表示
            var stateInfo = new[]
            {
            "=== Player Debug ===",
            $"State: {player.StateMachine?.CurrentStateType.ToString() ?? "Unknown"}",
            $"Collision Pos: ({player.Bounds.X}, {player.Bounds.Y})",
            $"Collision Size: {player.Bounds.Width}x{player.Bounds.Height}",
            $"Display Pos: ({player.Location.X}, {player.Location.Y})",
            $"Display Size: {player.Size.Width}x{player.Size.Height}",
            $"Grounded: {player.IsGrounded}",
            $"Velocity: {player.VerticalVelocity:F2}",
            $"Parent Window: {(player.Parent != null ? player.Parent.Id.ToString() : "None")}",
            $"Last Valid Parent: {(player.LastValidParent != null ? player.LastValidParent.Id.ToString() : "None")}"
        };

            DrawInfoPanel(g, stateInfo, new Point(DEBUG_PANEL_X, DEBUG_PANEL_Y));

            // プレイヤーの各領域を線で描画
            DrawPlayerBounds(g);

            // 重い描画処理は条件付きで実行
            if (ShouldDrawDetailedDebug())
            {
                DrawMovableRegion(g);
                DrawPlayerConnections(g);
            }
        }

        private bool ShouldDrawDetailedDebug()
        {
            // プレイヤーがゲームウィンドウ内にいる場合のみ詳細描画
            return player.Parent != null;
        }

        private void DrawPlayerBounds(Graphics g)
        {
            // 当たり判定領域を緑の線で描画
            var collisionBounds = player.CollisionBounds;
            using (var collisionPen = new Pen(Color.Lime, 2))
            {
                g.DrawRectangle(collisionPen, collisionBounds);
            }

            // 描画領域を青の線で描画
            var renderBounds = player.RenderBounds;
            using (var renderPen = new Pen(Color.Cyan, 2))
            {
                renderPen.DashStyle = System.Drawing.Drawing2D.DashStyle.Dash;
                g.DrawRectangle(renderPen, renderBounds);
            }

            // 表示領域（Form領域）を黄色の線で描画
            var displayBounds = player.DisplayBounds;
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
            (Color.Lime, "━━", "Collision Bounds (当たり判定)"),
            (Color.Cyan, "- - -", "Render Bounds (描画領域)"),
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

        private void DrawMovableRegion(Graphics g)
        {
            // デスクトップ全体への描画は重いため、プレイヤーの近くのみ描画
            var collisionBounds = player.Bounds;  // 当たり判定領域
            var drawRadius = 200; // プレイヤー周辺200px以内のみ描画
            var drawArea = new Rectangle(
                collisionBounds.X - drawRadius,
                collisionBounds.Y - drawRadius,
                collisionBounds.Width + drawRadius * 2,
                collisionBounds.Height + drawRadius * 2
            );

            // クリッピング領域を設定して描画範囲を制限
            var originalClip = g.Clip;
            g.SetClip(drawArea);

            try
            {
                using (var pen = new Pen(Color.Yellow, 1))
                {
                    pen.DashStyle = System.Drawing.Drawing2D.DashStyle.Dash;

                    // 移動可能領域（制限された範囲のみ）
                    var region = player.GetMovableRegion();
                    using (var path = region.GetRegionPath())
                    {
                        g.DrawPath(pen, path);
                    }

                    // 接地判定領域
                    var groundArea = player.GetGroundCheckArea();
                    using (var groundPen = new Pen(Color.Red, 1))
                    {
                        g.DrawRectangle(groundPen, groundArea);
                    }
                }
            }
            finally
            {
                g.Clip = originalClip;
            }
        }

        private void DrawPlayerConnections(Graphics g)
        {
            if (player.Parent == null) return;

            using (var pen = new Pen(Color.Yellow, 1))
            {
                pen.DashStyle = System.Drawing.Drawing2D.DashStyle.Dot;

                // 当たり判定領域の中心を使用
                var collisionBounds = player.Bounds;
                var playerCenter = new Point(
                    collisionBounds.X + collisionBounds.Width / 2,
                    collisionBounds.Y + collisionBounds.Height / 2
                );

                var parentCenter = new Point(
                    player.Parent.Bounds.X + player.Parent.Bounds.Width / 2,
                    player.Parent.Bounds.Y + player.Parent.Bounds.Height / 2
                );

                g.DrawLine(pen, playerCenter, parentCenter);
            }
        }
    }
}