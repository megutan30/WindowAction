using System;
using System.Drawing;
using System.Linq;
using MultiWindowActionGame.Collision;
using MultiWindowActionGame.Interfaces;
using MultiWindowActionGame.Windows;

namespace MultiWindowActionGame.Services
{
    /// <summary>
    /// 衝突判定の統一的なAPIを提供
    /// NoEntryZoneManager、ZOrderCollisionHelper、CollisionValidatorを統合
    /// </summary>
    public class CollisionService : ICollisionService
    {
        private readonly INoEntryZoneManager noEntryZoneManager;
        private readonly IWindowManager windowManager;
        private readonly ZOrderCollisionHelper zOrderHelper;
        private readonly CollisionValidator validator;

        public CollisionService(
            INoEntryZoneManager noEntryZoneManager,
            IWindowManager windowManager,
            ZOrderCollisionHelper zOrderHelper,
            CollisionValidator validator)
        {
            this.noEntryZoneManager = noEntryZoneManager ?? throw new ArgumentNullException(nameof(noEntryZoneManager));
            this.windowManager = windowManager ?? throw new ArgumentNullException(nameof(windowManager));
            this.zOrderHelper = zOrderHelper ?? throw new ArgumentNullException(nameof(zOrderHelper));
            this.validator = validator ?? throw new ArgumentNullException(nameof(validator));
        }

        /// <summary>
        /// 指定された境界が衝突するかチェック
        /// </summary>
        public bool CheckCollision(Rectangle bounds, CollisionOptions options)
        {
            // 1. 静的NoEntryZone判定
            if (options.CheckNoEntryZones)
            {
                if (noEntryZoneManager.Zones.Any(z => z.Bounds.IntersectsWith(bounds)))
                {
                    return true;
                }
            }

            // 2. 不可侵境界判定（Z-order考慮）
            if (options.CheckNoEntryBoundaries)
            {
                if (zOrderHelper.CheckNoEntryBoundaryCollision(bounds, options.ExcludeWindow, options.UseZOrderFiltering, options.ExcludeChildren))
                {
                    return true;
                }
            }

            // 3. 通常ウィンドウ判定（不可侵が移動する場合）
            if (options.CheckNormalWindows && options.ExcludeWindow?.IsNoEntryWindow == true)
            {
                if (zOrderHelper.CheckNormalWindowCollision(bounds, options.ExcludeWindow, options.UseZOrderFiltering, options.ExcludeChildren))
                {
                    return true;
                }
            }

            // 4. ボタン判定（不可侵が移動する場合）
            if (options.CheckButtons)
            {
                var allButtons = windowManager.GetAllButtons();
                if (allButtons.Any(button => button.Visible && button.Bounds.IntersectsWith(bounds)))
                {
                    return true;
                }
            }

            return false;
        }

        /// <summary>
        /// 移動可能な位置に調整した境界を返す
        /// </summary>
        public Rectangle ValidatePosition(Rectangle currentBounds, Rectangle proposedBounds, CollisionOptions options)
        {
            return validator.ValidatePosition(currentBounds, proposedBounds, options);
        }

        /// <summary>
        /// リサイズ可能なサイズに調整したサイズを返す
        /// </summary>
        public Size ValidateSize(Rectangle currentBounds, Size proposedSize, CollisionOptions options)
        {
            return validator.ValidateSize(currentBounds, proposedSize, options);
        }

        /// <summary>
        /// 親が不可侵ウィンドウの場合、境界を親の範囲内に制限
        /// </summary>
        public Rectangle ConstrainToParentBoundary(GameWindow window, Rectangle proposedBounds)
        {
            if (window.Parent is GameWindow parentWindow && parentWindow.IsNoEntryWindow)
            {
                Rectangle parentBounds = parentWindow.CollisionBounds;
                int adjustedX = Math.Max(parentBounds.Left,
                    Math.Min(parentBounds.Right - proposedBounds.Width, proposedBounds.X));
                int adjustedY = Math.Max(parentBounds.Top,
                    Math.Min(parentBounds.Bottom - proposedBounds.Height, proposedBounds.Y));

                return new Rectangle(adjustedX, adjustedY, proposedBounds.Width, proposedBounds.Height);
            }
            return proposedBounds;
        }
    }
}
