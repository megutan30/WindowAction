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
    }
}
