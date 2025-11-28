using System.Collections.Generic;
using System.Linq;
using System.Windows.Forms;
using MultiWindowActionGame.Windows;
using MultiWindowActionGame.Collision;

namespace MultiWindowActionGame.Services
{
    /// <summary>
    /// 衝突判定時のウィンドウフィルタリング処理を提供
    /// 重複コードを削減し、統一的なフィルタリングロジックを実現
    /// </summary>
    public static class CollisionFilter
    {
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
        /// <returns>不可侵ウィンドウは3px、通常ウィンドウは0px</returns>
        public static int GetBoundaryWidth(GameWindow window)
        {
            return window.IsNoEntryWindow ? 5 : 0;
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
                UseZOrderFiltering = true
            };
        }

        /// <summary>
        /// 親が不可侵ウィンドウの場合、境界内に位置を制限
        /// </summary>
        /// <param name="window">対象ウィンドウ</param>
        /// <param name="proposedBounds">提案された境界</param>
        /// <returns>制約適用後の境界</returns>
        public static Rectangle ConstrainToParentBounds(GameWindow window, Rectangle proposedBounds)
        {
            if (window.Parent is not GameWindow parentWindow || !parentWindow.IsNoEntryWindow)
            {
                return proposedBounds;
            }

            Rectangle parentBounds = parentWindow.CollisionBounds;

            // 親の境界を超えないように制限
            int adjustedX = Math.Max(parentBounds.Left,
                Math.Min(parentBounds.Right - proposedBounds.Width, proposedBounds.X));
            int adjustedY = Math.Max(parentBounds.Top,
                Math.Min(parentBounds.Bottom - proposedBounds.Height, proposedBounds.Y));

            return new Rectangle(adjustedX, adjustedY, proposedBounds.Width, proposedBounds.Height);
        }

        /// <summary>
        /// 親が不可侵ウィンドウの場合、境界内にサイズを制限
        /// </summary>
        /// <param name="window">対象ウィンドウ</param>
        /// <param name="proposedSize">提案されたサイズ</param>
        /// <returns>制約適用後のサイズ</returns>
        public static Size ConstrainSizeToParentBounds(GameWindow window, Size proposedSize)
        {
            if (window.Parent is not GameWindow parentWindow || !parentWindow.IsNoEntryWindow)
            {
                return proposedSize;
            }

            Rectangle parentBounds = parentWindow.CollisionBounds;
            Rectangle windowBounds = window.CollisionBounds;

            // 親の境界を超えないように最大サイズを制限
            int maxWidth = parentBounds.Right - windowBounds.Left;
            int maxHeight = parentBounds.Bottom - windowBounds.Top;

            return new Size(
                Math.Min(proposedSize.Width, maxWidth),
                Math.Min(proposedSize.Height, maxHeight)
            );
        }

        /// <summary>
        /// 不可侵ウィンドウの親境界バッファ制約を適用（移動用）
        /// </summary>
        /// <param name="window">対象ウィンドウ</param>
        /// <param name="validBounds">検証済みの境界</param>
        /// <param name="bufferSize">バッファサイズ（px）</param>
        /// <returns>バッファ制約適用後の境界</returns>
        public static Rectangle ApplyParentBoundaryBuffer(GameWindow window, Rectangle validBounds, int bufferSize = 5)
        {
            if (window.Parent is not GameWindow parentWindow || !window.IsNoEntryWindow)
            {
                return validBounds;
            }

            Rectangle parentBounds = parentWindow.CollisionBounds;
            int constrainedX = validBounds.X;
            int constrainedY = validBounds.Y;

            if (validBounds.Left < parentBounds.Left + bufferSize)
                constrainedX = parentBounds.Left + bufferSize;
            if (validBounds.Right > parentBounds.Right - bufferSize)
                constrainedX = parentBounds.Right - bufferSize - validBounds.Width;
            if (validBounds.Top < parentBounds.Top + bufferSize)
                constrainedY = parentBounds.Top + bufferSize;
            if (validBounds.Bottom > parentBounds.Bottom - bufferSize)
                constrainedY = parentBounds.Bottom - bufferSize - validBounds.Height;

            return new Rectangle(constrainedX, constrainedY, validBounds.Width, validBounds.Height);
        }

        /// <summary>
        /// 不可侵ウィンドウの親境界バッファ制約を適用（リサイズ用）
        /// </summary>
        /// <param name="window">対象ウィンドウ</param>
        /// <param name="newSize">新しいサイズ</param>
        /// <param name="bufferSize">バッファサイズ（px）</param>
        /// <returns>バッファ制約適用後のサイズ</returns>
        public static Size ApplyParentBoundaryBufferForResize(GameWindow window, Size newSize, int bufferSize = 5)
        {
            if (window.Parent is not GameWindow parentWindow || !window.IsNoEntryWindow)
            {
                return newSize;
            }

            Rectangle parentBounds = parentWindow.CollisionBounds;
            Rectangle currentBounds = window.CollisionBounds;

            // 親の境界を超えないように幅を制限（バッファ考慮）
            int maxWidth = parentBounds.Right - currentBounds.X - bufferSize;
            int constrainedWidth = newSize.Width;
            if (newSize.Width > maxWidth)
            {
                constrainedWidth = Math.Max(100, maxWidth); // 最小サイズ100pxを保証
            }

            // 親の境界を超えないように高さを制限（バッファ考慮）
            int maxHeight = parentBounds.Bottom - currentBounds.Y - bufferSize;
            int constrainedHeight = newSize.Height;
            if (newSize.Height > maxHeight)
            {
                constrainedHeight = Math.Max(100, maxHeight); // 最小サイズ100pxを保証
            }

            return new Size(constrainedWidth, constrainedHeight);
        }
    }
}
