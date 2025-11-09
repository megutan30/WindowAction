using System;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;
using MultiWindowActionGame.Managers;
using MultiWindowActionGame.Windows;
using MultiWindowActionGame.Interfaces;

namespace MultiWindowActionGame.Services
{
    /// <summary>
    /// Z-order考慮の衝突判定ヘルパー
    /// 不可侵ウィンドウと通常ウィンドウの衝突判定を提供
    /// </summary>
    public class ZOrderCollisionHelper
    {
        private readonly NoEntryBoundaryCollider boundaryCollider;
        private readonly ZOrderVisibilityService visibilityService;
        private readonly IWindowManager windowManager;

        public ZOrderCollisionHelper(
            NoEntryBoundaryCollider boundaryCollider,
            ZOrderVisibilityService visibilityService,
            IWindowManager windowManager)
        {
            this.boundaryCollider = boundaryCollider ?? throw new ArgumentNullException(nameof(boundaryCollider));
            this.visibilityService = visibilityService ?? throw new ArgumentNullException(nameof(visibilityService));
            this.windowManager = windowManager ?? throw new ArgumentNullException(nameof(windowManager));
        }

        /// <summary>
        /// 不可侵ウィンドウの境界との衝突をチェック（委譲）
        /// </summary>
        /// <param name="bounds">チェックする矩形</param>
        /// <param name="excludeWindow">除外するウィンドウ（自分自身を除外する場合に使用）</param>
        /// <param name="useZOrder">Z-orderを考慮するか（現在は常にtrueで動作）</param>
        /// <param name="excludeChildren">excludeWindowの子孫ウィンドウも除外するか</param>
        /// <returns>衝突がある場合true</returns>
        public bool CheckNoEntryBoundaryCollision(Rectangle bounds, GameWindow? excludeWindow, bool useZOrder, bool excludeChildren = false)
        {
            return boundaryCollider.CheckAnyCollision(bounds, out _, out _, excludeWindow, excludeChildren);
        }

        /// <summary>
        /// 通常ウィンドウとの衝突をチェック（不可侵ウィンドウが移動する場合に使用）
        /// </summary>
        /// <param name="bounds">チェックする矩形</param>
        /// <param name="excludeWindow">除外するウィンドウ（自分自身を除外する場合に使用）</param>
        /// <param name="useZOrder">Z-orderを考慮した可視性フィルタリングを行うか</param>
        /// <returns>衝突がある場合true</returns>
        public bool CheckNormalWindowCollision(Rectangle bounds, GameWindow? excludeWindow, bool useZOrder)
        {
            var allWindows = windowManager.GetAllWindows();

            foreach (var window in allWindows)
            {
                // 除外ウィンドウはスキップ
                if (window == excludeWindow) continue;

                // 親ウィンドウもスキップ（子が親の内部で移動する場合、親自体は障害物にならない）
                if (excludeWindow != null && window == excludeWindow.Parent) continue;

                // 不可侵ウィンドウはスキップ（境界判定で処理済み）
                if (window.IsNoEntryWindow) continue;

                // 最小化されているウィンドウはスキップ
                if (window.WindowState == FormWindowState.Minimized || window.IsMinimized) continue;

                // Z-order考慮の可視性チェック
                if (useZOrder && !visibilityService.IsWindowVisibleFromNoEntry(window, bounds, excludeWindow))
                {
                    continue;
                }

                // 衝突判定
                if (bounds.IntersectsWith(window.CollisionBounds))
                {
                    return true;
                }
            }

            return false;
        }
    }
}
