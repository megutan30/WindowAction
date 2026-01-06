using System.Collections.Generic;
using System.Linq;
using System.Windows.Forms;
using MultiWindowActionGame.Windows;
using MultiWindowActionGame.Collision;
using MultiWindowActionGame.Interfaces;
using MultiWindowActionGame.Managers;

namespace MultiWindowActionGame.Services
{
    /// <summary>
    /// 衝突判定時のウィンドウフィルタリング処理を提供
    /// 重複コードを削減し、統一的なフィルタリングロジックを実現
    /// </summary>
    public static class CollisionFilter
    {
        /// <summary>
        /// 不可侵ウィンドウの境界幅（px）
        /// NoEntryBoundaryColliderで生成される境界矩形の幅
        /// </summary>
        public const int NOENTRY_BOUNDARY_WIDTH = 5;

        /// <summary>
        /// 親ウィンドウの境界バッファ（子が親の端から保つべき距離、px）
        /// 不可侵ウィンドウが親ウィンドウ内で移動・リサイズする際の余白
        /// </summary>
        public const int PARENT_BOUNDARY_BUFFER = 5;

        /// <summary>
        /// 指定されたウィンドウをスキップすべきかチェック
        /// </summary>
        /// <param name="window">チェック対象のウィンドウ</param>
        /// <param name="excludeWindow">除外するウィンドウ（null可）</param>
        /// <param name="excludeChildren">excludeWindowの子孫も除外するか</param>
        /// <param name="excludeParent">excludeWindowの親も除外するか</param>
        /// <returns>スキップすべき場合true</returns>
        public static bool ShouldSkipWindow(
            GameWindow window,
            GameWindow? excludeWindow,
            bool excludeChildren = false,
            bool excludeParent = true)
        {
            // 除外ウィンドウ自身はスキップ
            if (window == excludeWindow)
                return true;

            // 子孫ウィンドウのスキップ
            if (excludeChildren && excludeWindow != null &&
                excludeWindow.GetAllDescendants().Contains(window))
                return true;

            // 親ウィンドウのスキップ（子が親の内部で移動する場合、親自体は障害物にならない）
            if (excludeParent && excludeWindow != null && window == excludeWindow.Parent)
                return true;

            // 最小化されているウィンドウはスキップ
            if (window.WindowState == FormWindowState.Minimized || window.IsMinimized)
                return true;

            return false;
        }

        /// <summary>
        /// 除外すべきウィンドウのセットを作成（HashSet方式）
        /// NoEntryBoundaryCollider.CheckAnyCollision用
        /// </summary>
        /// <param name="excludeWindow">除外するウィンドウ（null可）</param>
        /// <param name="excludeChildren">子孫も除外するか</param>
        /// <returns>除外すべきウィンドウのHashSet</returns>
        public static HashSet<GameWindow> CreateExcludedSet(
            GameWindow? excludeWindow,
            bool excludeChildren = false)
        {
            var excludedWindows = new HashSet<GameWindow>();

            if (excludeWindow != null)
            {
                excludedWindows.Add(excludeWindow);

                if (excludeChildren)
                {
                    // 子孫ウィンドウもすべて除外
                    foreach (var descendant in excludeWindow.GetAllDescendants().OfType<GameWindow>())
                    {
                        excludedWindows.Add(descendant);
                    }
                }
            }

            return excludedWindows;
        }

        /// <summary>
        /// ウィンドウの境界幅を取得
        /// </summary>
        /// <param name="window">対象ウィンドウ</param>
        /// <returns>不可侵ウィンドウはNOENTRY_BOUNDARY_WIDTH、通常ウィンドウは0px</returns>
        public static int GetBoundaryWidth(GameWindow window)
        {
            return window.IsNoEntryWindow ? NOENTRY_BOUNDARY_WIDTH : 0;
        }

        /// <summary>
        /// 標準的なCollisionOptionsを生成
        /// WindowStrategiesで使用される共通のオプション設定
        /// </summary>
        /// <param name="excludeWindow">除外するウィンドウ</param>
        /// <param name="excludeChildren">子孫も除外するか</param>
        /// <returns>標準的なCollisionOptions</returns>
        public static CollisionOptions CreateStandardOptions(GameWindow excludeWindow, bool excludeChildren = true)
        {
            return new CollisionOptions
            {
                ExcludeWindow = excludeWindow,
                ExcludeChildren = excludeChildren,
                CheckNoEntryZones = true,
                CheckNoEntryBoundaries = true,
                CheckNormalWindows = excludeWindow.IsNoEntryWindow,  // 不可侵ウィンドウは通常のウィンドウともぶつかる
                CheckButtons = excludeWindow.IsNoEntryWindow,  // 不可侵ウィンドウはボタンともぶつかる
                CheckPlayer = excludeWindow.IsNoEntryWindow,  // 不可侵ウィンドウはプレイヤーともぶつかる
                UseZOrderFiltering = true
            };
        }

        /// <summary>
        /// 親がある場合、境界内に位置を制限
        /// 子が不可侵ウィンドウの場合は境界線（5px）を考慮してバッファを適用
        /// </summary>
        /// <param name="window">対象ウィンドウ</param>
        /// <param name="proposedBounds">提案された境界</param>
        /// <returns>制約適用後の境界</returns>
        public static Rectangle ConstrainToParentBounds(GameWindow window, Rectangle proposedBounds)
        {
            if (window.Parent is not GameWindow parentWindow)
            {
                return proposedBounds;
            }

            Rectangle parentBounds = parentWindow.CollisionBounds;

            // 子が不可侵ウィンドウの場合は境界線（5px）を考慮
            int bufferSize = window.IsNoEntryWindow ? PARENT_BOUNDARY_BUFFER : 0;

            // 親の境界を超えないように制限（不可侵ウィンドウの場合はバッファ付き）
            int adjustedX = Math.Max(parentBounds.Left + bufferSize,
                Math.Min(parentBounds.Right - bufferSize - proposedBounds.Width, proposedBounds.X));
            int adjustedY = Math.Max(parentBounds.Top + bufferSize,
                Math.Min(parentBounds.Bottom - bufferSize - proposedBounds.Height, proposedBounds.Y));

            return new Rectangle(adjustedX, adjustedY, proposedBounds.Width, proposedBounds.Height);
        }

        /// <summary>
        /// 親がある場合、境界内にサイズを制限
        /// 子が不可侵ウィンドウの場合は境界線（5px）を考慮してバッファを適用
        /// </summary>
        /// <param name="window">対象ウィンドウ</param>
        /// <param name="proposedSize">提案されたサイズ</param>
        /// <returns>制約適用後のサイズ</returns>
        public static Size ConstrainSizeToParentBounds(GameWindow window, Size proposedSize)
        {
            if (window.Parent is not GameWindow parentWindow)
            {
                return proposedSize;
            }

            Rectangle parentBounds = parentWindow.CollisionBounds;
            Rectangle windowBounds = window.CollisionBounds;

            // 子が不可侵ウィンドウの場合は境界線（5px）を考慮
            int bufferSize = window.IsNoEntryWindow ? PARENT_BOUNDARY_BUFFER : 0;

            // 親の境界を超えないように最大サイズを制限（不可侵ウィンドウの場合はバッファ付き）
            int maxWidth = parentBounds.Right - bufferSize - windowBounds.Left;
            int maxHeight = parentBounds.Bottom - bufferSize - windowBounds.Top;

            return new Size(
                Math.Min(proposedSize.Width, maxWidth),
                Math.Min(proposedSize.Height, maxHeight)
            );
        }

        /// <summary>
        /// 親が不可侵ウィンドウの場合、Z-order可視性を考慮して境界内に位置を制限
        /// 見えている境界のみを制約として適用し、隠れている境界は無視
        /// </summary>
        /// <param name="window">対象ウィンドウ</param>
        /// <param name="proposedBounds">提案された境界</param>
        /// <param name="windowManager">ウィンドウマネージャー</param>
        /// <param name="boundaryCollider">境界衝突判定</param>
        /// <returns>制約適用後の境界</returns>
        public static Rectangle ConstrainToVisibleParentBoundaries(
            GameWindow window,
            Rectangle proposedBounds,
            IWindowManager windowManager,
            NoEntryBoundaryCollider boundaryCollider)
        {
            if (window.Parent is not GameWindow parentWindow)
            {
                return proposedBounds;
            }

            // 親が不可侵ウィンドウでない場合は通常の制約
            if (!parentWindow.IsNoEntryWindow)
            {
                return ConstrainToParentBounds(window, proposedBounds);
            }

            // 親が不可侵ウィンドウの場合: Z-order可視性を考慮

            Rectangle parentBounds = parentWindow.CollisionBounds;
            int bufferSize = window.IsNoEntryWindow ? PARENT_BOUNDARY_BUFFER : 0;

            // 親の4辺の境界を取得
            var boundaries = boundaryCollider.GetBoundaryRectangles(parentWindow);

            // 各境界を位置で分類
            Rectangle? topBoundary = null;
            Rectangle? bottomBoundary = null;
            Rectangle? leftBoundary = null;
            Rectangle? rightBoundary = null;

            foreach (var boundary in boundaries)
            {
                // 上辺判定: Y座標が親の上端付近
                if (Math.Abs(boundary.Y - parentBounds.Y) < 3)
                {
                    topBoundary = boundary;
                }
                // 下辺判定: Y座標が親の下端付近
                else if (Math.Abs(boundary.Bottom - parentBounds.Bottom) < 3)
                {
                    bottomBoundary = boundary;
                }
                // 左辺判定: X座標が親の左端付近
                else if (Math.Abs(boundary.X - parentBounds.X) < 3)
                {
                    leftBoundary = boundary;
                }
                // 右辺判定: X座標が親の右端付近
                else if (Math.Abs(boundary.Right - parentBounds.Right) < 3)
                {
                    rightBoundary = boundary;
                }
            }

            // 各境界の可視性をチェック
            bool topVisible = false;
            bool bottomVisible = false;
            bool leftVisible = false;
            bool rightVisible = false;

            if (topBoundary.HasValue)
            {
                topVisible = boundaryCollider.CheckCollision(parentWindow, topBoundary.Value, out _);
            }
            if (bottomBoundary.HasValue)
            {
                bottomVisible = boundaryCollider.CheckCollision(parentWindow, bottomBoundary.Value, out _);
            }
            if (leftBoundary.HasValue)
            {
                leftVisible = boundaryCollider.CheckCollision(parentWindow, leftBoundary.Value, out _);
            }
            if (rightBoundary.HasValue)
            {
                rightVisible = boundaryCollider.CheckCollision(parentWindow, rightBoundary.Value, out _);
            }

            // 見えている境界のみに対して制約を適用
            int adjustedX = proposedBounds.X;
            int adjustedY = proposedBounds.Y;

            // 左境界が見えている場合のみ制約
            if (leftVisible)
            {
                adjustedX = Math.Max(parentBounds.Left + bufferSize, adjustedX);
            }

            // 右境界が見えている場合のみ制約
            if (rightVisible)
            {
                adjustedX = Math.Min(parentBounds.Right - bufferSize - proposedBounds.Width, adjustedX);
            }

            // 上境界が見えている場合のみ制約
            if (topVisible)
            {
                adjustedY = Math.Max(parentBounds.Top + bufferSize, adjustedY);
            }

            // 下境界が見えている場合のみ制約
            if (bottomVisible)
            {
                adjustedY = Math.Min(parentBounds.Bottom - bufferSize - proposedBounds.Height, adjustedY);
            }

            return new Rectangle(adjustedX, adjustedY, proposedBounds.Width, proposedBounds.Height);
        }

        /// <summary>
        /// 親が不可侵ウィンドウの場合、Z-order可視性を考慮して境界内にサイズを制限
        /// 見えている境界のみを制約として適用し、隠れている境界は無視
        /// </summary>
        /// <param name="window">対象ウィンドウ</param>
        /// <param name="proposedSize">提案されたサイズ</param>
        /// <param name="windowManager">ウィンドウマネージャー</param>
        /// <param name="boundaryCollider">境界衝突判定</param>
        /// <returns>制約適用後のサイズ</returns>
        public static Size ConstrainSizeToVisibleParentBoundaries(
            GameWindow window,
            Size proposedSize,
            IWindowManager windowManager,
            NoEntryBoundaryCollider boundaryCollider)
        {
            if (window.Parent is not GameWindow parentWindow)
            {
                return proposedSize;
            }

            // 親が不可侵ウィンドウでない場合は通常の制約
            if (!parentWindow.IsNoEntryWindow)
            {
                return ConstrainSizeToParentBounds(window, proposedSize);
            }

            // 親が不可侵ウィンドウの場合: Z-order可視性を考慮

            Rectangle parentBounds = parentWindow.CollisionBounds;
            Rectangle windowBounds = window.CollisionBounds;
            int bufferSize = window.IsNoEntryWindow ? PARENT_BOUNDARY_BUFFER : 0;

            // 親の4辺の境界を取得
            var boundaries = boundaryCollider.GetBoundaryRectangles(parentWindow);

            // 右辺・下辺のみをチェック（サイズ制約は拡大方向のみ）
            Rectangle? rightBoundary = null;
            Rectangle? bottomBoundary = null;

            foreach (var boundary in boundaries)
            {
                // 右辺判定: X座標が親の右端付近
                if (Math.Abs(boundary.Right - parentBounds.Right) < 3)
                {
                    rightBoundary = boundary;
                }
                // 下辺判定: Y座標が親の下端付近
                else if (Math.Abs(boundary.Bottom - parentBounds.Bottom) < 3)
                {
                    bottomBoundary = boundary;
                }
            }

            // 各境界の可視性をチェック
            bool rightVisible = false;
            bool bottomVisible = false;

            if (rightBoundary.HasValue)
            {
                rightVisible = boundaryCollider.CheckCollision(parentWindow, rightBoundary.Value, out _);
            }
            if (bottomBoundary.HasValue)
            {
                bottomVisible = boundaryCollider.CheckCollision(parentWindow, bottomBoundary.Value, out _);
            }

            // 見えている境界のみに対して制約を適用
            int maxWidth = proposedSize.Width;
            int maxHeight = proposedSize.Height;

            // 右境界が見えている場合のみ幅を制約
            if (rightVisible)
            {
                maxWidth = Math.Min(maxWidth, parentBounds.Right - bufferSize - windowBounds.Left);
            }

            // 下境界が見えている場合のみ高さを制約
            if (bottomVisible)
            {
                maxHeight = Math.Min(maxHeight, parentBounds.Bottom - bufferSize - windowBounds.Top);
            }

            return new Size(maxWidth, maxHeight);
        }
    }
}
