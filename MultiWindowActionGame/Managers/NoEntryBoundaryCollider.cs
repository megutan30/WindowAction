using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Linq;
using System.Windows.Forms;
using MultiWindowActionGame.Windows;
using MultiWindowActionGame.Interfaces;
using MultiWindowActionGame.Services;

namespace MultiWindowActionGame.Managers
{
    /// <summary>
    /// 不可侵ウィンドウの境界衝突判定を担当するクラス
    /// Z-order + Region方式で正確な可視性判定を行う
    /// </summary>
    public class NoEntryBoundaryCollider
    {
        private readonly IWindowManager windowManager;
        private readonly ZOrderVisibilityService visibilityService;
        private readonly List<GameWindow> noEntryWindows = new();

        public NoEntryBoundaryCollider(
            IWindowManager windowManager,
            ZOrderVisibilityService visibilityService)
        {
            this.windowManager = windowManager ?? throw new ArgumentNullException(nameof(windowManager));
            this.visibilityService = visibilityService ?? throw new ArgumentNullException(nameof(visibilityService));
        }

        /// <summary>
        /// 不可侵ウィンドウの4辺境界Rectangleを取得
        /// </summary>
        /// <param name="window">対象の不可侵ウィンドウ</param>
        /// <returns>4つのRectangle（Top, Bottom, Left, Right）のリスト</returns>
        public List<Rectangle> GetBoundaryRectangles(GameWindow window)
        {
            var bounds = window.CollisionBounds;
            const int borderThickness = 5;
            var rectangles = new List<Rectangle>(4);

            // 上辺
            rectangles.Add(new Rectangle(
                bounds.X, bounds.Y,
                bounds.Width, borderThickness
            ));

            // 下辺
            rectangles.Add(new Rectangle(
                bounds.X, bounds.Bottom - borderThickness,
                bounds.Width, borderThickness
            ));

            // 左辺
            rectangles.Add(new Rectangle(
                bounds.X, bounds.Y,
                borderThickness, bounds.Height
            ));

            // 右辺
            rectangles.Add(new Rectangle(
                bounds.Right - borderThickness, bounds.Y,
                borderThickness, bounds.Height
            ));

            return rectangles;
        }

        /// <summary>
        /// 単一の不可侵ウィンドウとの境界衝突をZ-order + Region考慮で判定
        /// </summary>
        /// <param name="window">判定対象の不可侵ウィンドウ</param>
        /// <param name="checkBounds">衝突判定する矩形</param>
        /// <param name="collisionRect">実際に衝突した境界Rectangle</param>
        /// <returns>衝突があればtrue</returns>
        public bool CheckCollision(GameWindow window, Rectangle checkBounds, out Rectangle? collisionRect)
        {
            collisionRect = null;

            // 不可侵ウィンドウでない、または最小化されている場合は判定なし
            if (!noEntryWindows.Contains(window) ||
                window.WindowState == FormWindowState.Minimized ||
                window.IsMinimized)
            {
                return false;
            }

            // 4辺の境界を取得
            var boundaryRects = GetBoundaryRectangles(window);

            // 各境界との衝突判定とZ-order可視性チェック
            foreach (var boundary in boundaryRects)
            {
                // checkBoundsとboundaryの交差を確認
                if (!checkBounds.IntersectsWith(boundary))
                {
                    continue;
                }

                // 交差部分を計算
                Rectangle intersection = Rectangle.Intersect(checkBounds, boundary);

                // 交差部分のRegionを作成
                using (Region visibleRegion = new Region(intersection))
                {
                    var allWindows = windowManager.GetAllWindows();
                    var windowsList = allWindows.ToList();
                    int windowIndex = windowsList.IndexOf(window);

                    if (windowIndex != -1)
                    {
                        // より前面のウィンドウで交差部分を除外
                        for (int i = windowIndex + 1; i < windowsList.Count; i++)
                        {
                            var coveringWindow = windowsList[i];

                            // 最小化されているウィンドウは無視
                            if (coveringWindow.WindowState == FormWindowState.Minimized ||
                                coveringWindow.IsMinimized)
                            {
                                continue;
                            }

                            // 前面ウィンドウの領域を除外
                            visibleRegion.Exclude(coveringWindow.CollisionBounds);
                        }
                    }

                    // visibleRegionが空でなければ、見えている部分がある = 衝突判定有効
                    using (var dummyGraphics = Graphics.FromHwnd(IntPtr.Zero))
                    {
                        if (!visibleRegion.IsEmpty(dummyGraphics))
                        {
                            collisionRect = boundary;
                            return true;
                        }
                    }
                }
            }

            return false;
        }

        /// <summary>
        /// すべての不可侵ウィンドウとの境界衝突をチェック
        /// </summary>
        /// <param name="checkBounds">衝突判定する矩形</param>
        /// <param name="collidingWindow">衝突した不可侵ウィンドウ</param>
        /// <param name="collisionRect">実際に衝突した境界Rectangle</param>
        /// <param name="excludeWindow">除外するウィンドウ（自分自身を除外する場合に使用）</param>
        /// <param name="excludeChildren">excludeWindowの子孫ウィンドウも除外するか</param>
        /// <returns>衝突があればtrue</returns>
        public bool CheckAnyCollision(
            Rectangle checkBounds,
            out GameWindow? collidingWindow,
            out Rectangle? collisionRect,
            GameWindow? excludeWindow = null,
            bool excludeChildren = false)
        {
            collidingWindow = null;
            collisionRect = null;

            // 除外すべきウィンドウのセットを作成
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

            // すべての不可侵ウィンドウをチェック
            foreach (var window in noEntryWindows)
            {
                // 除外ウィンドウはスキップ（自分自身と子孫の境界との衝突を防ぐ）
                if (excludedWindows.Contains(window))
                {
                    continue;
                }

                if (CheckCollision(window, checkBounds, out var rect))
                {
                    collidingWindow = window;
                    collisionRect = rect;
                    return true;
                }
            }

            return false;
        }

        /// <summary>
        /// 不可侵ウィンドウを登録
        /// </summary>
        public void RegisterWindow(GameWindow window)
        {
            if (!noEntryWindows.Contains(window))
            {
                noEntryWindows.Add(window);
            }
        }

        /// <summary>
        /// 不可侵ウィンドウの登録を解除
        /// </summary>
        public void UnregisterWindow(GameWindow window)
        {
            noEntryWindows.Remove(window);
        }

        /// <summary>
        /// 登録されている不可侵ウィンドウのリスト（読み取り専用）
        /// </summary>
        public IReadOnlyList<GameWindow> NoEntryWindows => noEntryWindows.AsReadOnly();
    }
}
