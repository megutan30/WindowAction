using System;
using System.Drawing;
using System.Threading.Tasks;
using MultiWindowActionGame.Core;
using MultiWindowActionGame.Interfaces;
using MultiWindowActionGame.Windows;

namespace MultiWindowActionGame.Player
{
    public class PlayerWindowInteraction : IPlayerWindowInteraction
    {
        private readonly IWindowManager windowManager;
        private readonly INoEntryZoneManager? noEntryZoneManager;
        private readonly IEffectTarget? playerForm;
        private GameWindow? currentParent;
        private GameWindow? lastValidParent;
        private Size originalSize;
        private Action<GameWindow?>? onParentChanged;

        public GameWindow? CurrentParent => currentParent;
        public GameWindow? LastValidParent => lastValidParent;
        public Region MovableRegion => GetValidRegion(currentParent);

        public PlayerWindowInteraction(IWindowManager windowManager, Size originalSize, INoEntryZoneManager? noEntryZoneManager = null, IEffectTarget? playerForm = null)
        {
            this.windowManager = windowManager ?? throw new ArgumentNullException(nameof(windowManager));
            this.noEntryZoneManager = noEntryZoneManager;
            this.originalSize = originalSize;
            this.playerForm = playerForm;
        }

        public void SetOnParentChangedCallback(Action<GameWindow?> callback)
        {
            onParentChanged = callback;
        }

        private INoEntryZoneManager GetNoEntryZoneManagerSafely()
        {
            return noEntryZoneManager ?? NoEntryZoneManager.Current;
        }

        public void HandleWindowTransitions(Rectangle newBounds, Rectangle currentBounds)
        {
            if (currentParent == null)
            {
                var coveringWindow = windowManager.GetWindowFullyContaining(newBounds);
                if (coveringWindow != null)
                {
                    SetParent(coveringWindow);
                }
            }
            else
            {
                var newWindow = windowManager.GetTopWindowAt(newBounds, currentParent);
                if (newWindow != null && newWindow != currentParent)
                {
                    SetParent(newWindow);
                }
            }
        }

        public void SetParent(GameWindow? newParent)
        {
            if (currentParent != null)
            {
                lastValidParent = currentParent;
                if (playerForm != null)
                    currentParent.RemoveChild(playerForm);
            }

            currentParent = newParent;
            if (currentParent != null && playerForm != null)
                currentParent.AddChild(playerForm);

            // PlayerFormのParentプロパティ更新のためのコールバック
            onParentChanged?.Invoke(newParent);
        }

        public (Rectangle adjustedBounds, bool hitCeiling) HandleWindowCollisions(Rectangle newBounds, Rectangle currentBounds)
        {
            var adjustedBounds = newBounds;
            bool hitCeiling = false;
            adjustedBounds = GetNoEntryZoneManagerSafely().GetValidPosition(currentBounds, adjustedBounds);

            var intersectingWindows = windowManager.GetIntersectingWindows(adjustedBounds);
            foreach (var window in intersectingWindows)
            {
                // 不可侵ウィンドウはGetValidPositionで処理済みなのでスキップ
                if (window.IsNoEntryWindow)
                {
                    continue;
                }

                Rectangle windowBounds = window.CollisionBounds;
                if (currentBounds.Bottom <= windowBounds.Top && adjustedBounds.Bottom > windowBounds.Top)
                {
                    adjustedBounds.Y = windowBounds.Top - adjustedBounds.Height;
                }
                else if (currentBounds.Top >= windowBounds.Bottom && adjustedBounds.Top < windowBounds.Bottom)
                {
                    // 天井衝突（上昇中にウィンドウの下辺にぶつかった）
                    adjustedBounds.Y = windowBounds.Bottom;
                    hitCeiling = true;
                }
                else if (currentBounds.Right <= windowBounds.Left && adjustedBounds.Right > windowBounds.Left)
                {
                    adjustedBounds.X = windowBounds.Left - adjustedBounds.Width;
                }
                else if (currentBounds.Left >= windowBounds.Right && adjustedBounds.Left < windowBounds.Right)
                {
                    adjustedBounds.X = windowBounds.Right;
                }
            }

            return (adjustedBounds, hitCeiling);
        }

        public bool IsValidMove(Rectangle newBounds, GameWindow? currentParent)
        {
            // Z-order + Region考慮の不可侵ウィンドウ境界判定を含む
            if (GetNoEntryZoneManagerSafely().IntersectsWithAnyZone(newBounds))
            {
                return false;
            }

            if (currentParent == null)
            {
                return IsWithinMainForm(newBounds);
            }
            else
            {
                // リファクタリング修正: movableRegionを毎回計算して最新の状態を取得
                using (Region freshMovableRegion = windowManager.CalculateMovableRegion(currentParent))
                using (Graphics g = Graphics.FromHwnd(IntPtr.Zero))
                {
                    if (!freshMovableRegion.IsEmpty(g))
                    {
                        return IsCompletelyInside(newBounds, freshMovableRegion, g);
                    }
                    return IsWithinMainForm(newBounds);
                }
            }
        }

        public void OnMinimize()
        {
            if (currentParent != null)
            {
                lastValidParent = currentParent;
                if (playerForm != null)
                    currentParent.RemoveChild(playerForm);
                currentParent = null;
                onParentChanged?.Invoke(null);
            }
        }

        public Region GetValidRegion(GameWindow? currentParent)
        {
            if (currentParent != null)
            {
                return windowManager.CalculateMovableRegion(currentParent);
            }
            else if (Program.mainForm != null)
            {
                return new Region(new Rectangle(0, 0,
                    Program.mainForm.ClientSize.Width,
                    Program.mainForm.ClientSize.Height));
            }
            return new Region();
        }

        private bool IsWithinMainForm(Rectangle bounds)
        {
            if (Program.mainForm != null)
            {
                return bounds.Left >= 0 &&
                       bounds.Right <= Program.mainForm.ClientSize.Width &&
                       bounds.Top >= 0 &&
                       bounds.Bottom <= Program.mainForm.ClientSize.Height;
            }
            return false;
        }

        private bool IsCompletelyInside(Rectangle bounds, Region region, Graphics g)
        {
            return region.IsVisible(bounds.Left, bounds.Top, g) &&
                   region.IsVisible(bounds.Right - 1, bounds.Top, g) &&
                   region.IsVisible(bounds.Left, bounds.Bottom - 1, g) &&
                   region.IsVisible(bounds.Right - 1, bounds.Bottom - 1, g);
        }

        public Rectangle AdjustMovement(Rectangle oldBounds, Rectangle newBounds, GameWindow? currentParent, Action? onCeilingHit)
        {
            Rectangle adjustedBounds = oldBounds;

            using (Graphics g = Graphics.FromHwnd(IntPtr.Zero))
            {
                // X軸の移動を調整
                if (oldBounds.X != newBounds.X)
                {
                    int step = Math.Sign(newBounds.X - oldBounds.X);
                    while (adjustedBounds.X != newBounds.X)
                    {
                        Rectangle testBounds = new Rectangle(
                            adjustedBounds.X + step,
                            adjustedBounds.Y,
                            adjustedBounds.Width,
                            adjustedBounds.Height
                        );
                        if (IsValidMove(testBounds, currentParent))
                        {
                            adjustedBounds.X += step;
                        }
                        else
                        {
                            break;
                        }
                    }
                }

                // Y軸の移動を調整
                if (oldBounds.Y != newBounds.Y)
                {
                    int step = Math.Sign(newBounds.Y - oldBounds.Y);
                    while (adjustedBounds.Y != newBounds.Y)
                    {
                        Rectangle testBounds = new Rectangle(
                            adjustedBounds.X,
                            adjustedBounds.Y + step,
                            adjustedBounds.Width,
                            adjustedBounds.Height
                        );
                        if (IsValidMove(testBounds, currentParent))
                        {
                            adjustedBounds.Y += step;
                        }
                        else
                        {
                            // 移動がブロックされた場合、上方向への移動なら天井衝突コールバックを呼ぶ
                            if (step < 0) // 上方向への移動（ジャンプ中に天井にぶつかった）
                            {
                                onCeilingHit?.Invoke();
                            }
                            break;
                        }
                    }
                }
            }

            return adjustedBounds;
        }
    }
}