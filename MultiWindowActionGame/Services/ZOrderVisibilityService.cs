using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Linq;
using System.Windows.Forms;
using MultiWindowActionGame.Windows;
using MultiWindowActionGame.Interfaces;

namespace MultiWindowActionGame.Services
{
    /// <summary>
    /// Z-order + Region方式の可視性判定サービス
    /// ウィンドウの重なり順を考慮した正確な可視性判定を提供
    /// </summary>
    public class ZOrderVisibilityService
    {
        private readonly IWindowManager windowManager;

        public ZOrderVisibilityService(IWindowManager windowManager)
        {
            this.windowManager = windowManager ?? throw new ArgumentNullException(nameof(windowManager));
        }

        /// <summary>
        /// 不可侵ウィンドウの指定領域が見えているかチェック
        /// </summary>
        /// <param name="window">チェック対象の不可侵ウィンドウ</param>
        /// <param name="checkBounds">チェックする矩形</param>
        /// <param name="noEntryWindows">登録されている不可侵ウィンドウのリスト</param>
        /// <returns>見えている場合true、隠れている場合false</returns>
        public bool IsNoEntryWindowVisible(GameWindow window, Rectangle checkBounds, List<GameWindow> noEntryWindows)
        {
            // 不可侵ウィンドウでない場合はfalse
            if (!noEntryWindows.Contains(window))
            {
                return false;
            }

            var allWindows = windowManager.GetAllWindows();
            var windowsList = allWindows.ToList();
            int windowIndex = windowsList.IndexOf(window);

            if (windowIndex == -1)
            {
                return true; // リストにない場合は見えているとみなす
            }

            // より前面のウィンドウ（インデックスが大きい）をチェック
            for (int i = windowIndex + 1; i < windowsList.Count; i++)
            {
                var coveringWindow = windowsList[i];

                // 最小化されているウィンドウは無視
                if (coveringWindow.WindowState == FormWindowState.Minimized ||
                    coveringWindow.IsMinimized)
                {
                    continue;
                }

                // 重なっている場合、その部分は見えていない
                if (coveringWindow.CollisionBounds.IntersectsWith(checkBounds))
                {
                    return false;
                }
            }

            return true; // どのウィンドウにも隠されていない
        }

        /// <summary>
        /// 通常ウィンドウが不可侵ウィンドウの視点から見えているかチェック（Z-order考慮）
        /// </summary>
        /// <param name="normalWindow">チェック対象の通常ウィンドウ</param>
        /// <param name="checkBounds">衝突判定する矩形</param>
        /// <param name="excludeWindow">除外するウィンドウ（動かしている不可侵ウィンドウ）</param>
        /// <returns>見えている場合true、隠れている場合false</returns>
        public bool IsWindowVisibleFromNoEntry(
            GameWindow normalWindow,
            Rectangle checkBounds,
            GameWindow? excludeWindow)
        {
            // 通常ウィンドウとcheckBoundsの交差部分を計算
            var windowBounds = normalWindow.CollisionBounds;
            if (!checkBounds.IntersectsWith(windowBounds))
            {
                return false;  // そもそも交差していない
            }

            Rectangle intersection = Rectangle.Intersect(checkBounds, windowBounds);

            // 交差部分のRegionを作成
            using (Region visibleRegion = new Region(intersection))
            {
                var allWindows = windowManager.GetAllWindows();
                var windowsList = allWindows.ToList();
                int normalWindowIndex = windowsList.IndexOf(normalWindow);

                if (normalWindowIndex != -1)
                {
                    // より前面の不可侵ウィンドウで交差部分を除外
                    for (int i = normalWindowIndex + 1; i < windowsList.Count; i++)
                    {
                        var coveringWindow = windowsList[i];

                        // 自分自身（動かしている不可侵ウィンドウ）はスキップ
                        if (excludeWindow != null && coveringWindow == excludeWindow)
                        {
                            continue;
                        }

                        // 不可侵ウィンドウのみチェック（通常ウィンドウは透明扱い）
                        if (!coveringWindow.IsNoEntryWindow)
                        {
                            continue;
                        }

                        // 最小化されているウィンドウは無視
                        if (coveringWindow.WindowState == FormWindowState.Minimized ||
                            coveringWindow.IsMinimized)
                        {
                            continue;
                        }

                        // 前面不可侵ウィンドウの領域を除外
                        visibleRegion.Exclude(coveringWindow.CollisionBounds);
                    }
                }

                // visibleRegionが空でなければ、見えている部分がある = 衝突判定有効
                using (var dummyGraphics = Graphics.FromHwnd(IntPtr.Zero))
                {
                    return !visibleRegion.IsEmpty(dummyGraphics);
                }
            }
        }

        /// <summary>
        /// 汎用的な可視性判定（Region方式）
        /// </summary>
        /// <param name="rect">判定対象の矩形</param>
        /// <param name="referenceWindow">基準となるウィンドウ</param>
        /// <param name="excludeWindows">除外するウィンドウのリスト</param>
        /// <returns>見えている場合true、隠れている場合false</returns>
        public bool IsRectangleVisible(
            Rectangle rect,
            GameWindow referenceWindow,
            List<GameWindow>? excludeWindows = null)
        {
            // RectangleのRegionを作成
            using (Region visibleRegion = new Region(rect))
            {
                var allWindows = windowManager.GetAllWindows();
                var windowsList = allWindows.ToList();
                int refIndex = windowsList.IndexOf(referenceWindow);

                if (refIndex != -1)
                {
                    // より前面のウィンドウで矩形を除外
                    for (int i = refIndex + 1; i < windowsList.Count; i++)
                    {
                        var coveringWindow = windowsList[i];

                        // 除外リストに含まれている場合はスキップ
                        if (excludeWindows?.Contains(coveringWindow) == true)
                        {
                            continue;
                        }

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

                // visibleRegionが空でなければ、見えている部分がある
                using (var dummyGraphics = Graphics.FromHwnd(IntPtr.Zero))
                {
                    return !visibleRegion.IsEmpty(dummyGraphics);
                }
            }
        }
    }
}
