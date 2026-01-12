using MultiWindowActionGame;
using MultiWindowActionGame.Core;
using MultiWindowActionGame.Interfaces;
using MultiWindowActionGame.Managers;
using MultiWindowActionGame.Services;
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;

namespace MultiWindowActionGame.Windows
{
    public class NoEntryZoneManager : INoEntryZoneManager
    {
        // インスタンス統一化のためのstatic参照
        private static NoEntryZoneManager? _current;
        public static NoEntryZoneManager Current
        {
            get => _current ?? throw new InvalidOperationException("NoEntryZoneManager is not initialized");
            internal set => _current = value;
        }

        private List<NoEntryZone> zones = new List<NoEntryZone>();
        public IReadOnlyList<NoEntryZone> Zones => zones.AsReadOnly();

        private readonly NoEntryBoundaryCollider boundaryCollider;
        private readonly ZOrderVisibilityService visibilityService;
        private readonly IWindowManager? windowManager;

        // 後方互換性のためのパラメータなしコンストラクタ
        public NoEntryZoneManager()
        {
            // 後方互換性のため、nullを許容（ただし機能は制限される）
            this.boundaryCollider = null!;
            this.visibilityService = null!;
            this.windowManager = null;
        }

        public NoEntryZoneManager(
            IWindowManager? windowManager,
            NoEntryBoundaryCollider boundaryCollider,
            ZOrderVisibilityService visibilityService)
        {
            this.windowManager = windowManager;
            this.boundaryCollider = boundaryCollider ?? throw new ArgumentNullException(nameof(boundaryCollider));
            this.visibilityService = visibilityService ?? throw new ArgumentNullException(nameof(visibilityService));
        }

        public void AddZone(Point location, Size size)
        {
            var zone = new NoEntryZone(location, size);
            zones.Add(zone);
            zone.Show();
        }

        public void RemoveZone(NoEntryZone zone)
        {
            if (zones.Contains(zone))
            {
                zones.Remove(zone);
                zone.Close();
            }
        }

        public void ClearZones()
        {
            foreach (var zone in zones.ToList())
            {
                zone.Close();
            }
            zones.Clear();
        }

        // 指定された矩形が不可侵領域と重なるかチェック
        public bool IntersectsWithAnyZone(Rectangle bounds, GameWindow? excludeWindow = null)
        {
            // 既存のNoEntryZone判定
            if (zones.Any(zone => zone.Bounds.IntersectsWith(bounds)))
            {
                return true;
            }

            // 不可侵ウィンドウ境界判定（Z-order + Region考慮、除外ウィンドウを渡す）
            if (boundaryCollider != null && CheckAnyNoEntryBoundaryCollision(bounds, out _, out _, excludeWindow))
            {
                return true;
            }

            // 不可侵ウィンドウが移動する場合、通常ウィンドウとも衝突判定
            if (excludeWindow != null && excludeWindow.IsNoEntryWindow && windowManager != null)
            {
                var allWindows = windowManager.GetAllWindows();
                foreach (var window in allWindows)
                {
                    // 自分自身はスキップ
                    if (window == excludeWindow)
                    {
                        continue;
                    }

                    // 不可侵ウィンドウはスキップ（既に上でチェック済み）
                    if (window.IsNoEntryWindow)
                    {
                        continue;
                    }

                    // 最小化されているウィンドウはスキップ
                    if (window.WindowState == FormWindowState.Minimized || window.IsMinimized)
                    {
                        continue;
                    }

                    // 通常ウィンドウの可視性をチェック（Z-order考慮）
                    if (visibilityService != null && !visibilityService.IsWindowVisibleFromNoEntry(window, bounds, excludeWindow))
                    {
                        continue;  // 隠れている場合はスキップ
                    }

                    // 通常ウィンドウとの衝突判定
                    if (bounds.IntersectsWith(window.CollisionBounds))
                    {
                        return true;
                    }
                }
            }

            return false;
        }


        public Rectangle GetValidPosition(Rectangle currentBounds, Rectangle proposedBounds, GameWindow? excludeWindow = null)
        {
            Rectangle adjustedBounds = proposedBounds;

            // 調整候補を収集（最も制約が厳しい調整を採用）
            int? bestAdjustedX = null;
            int? bestAdjustedY = null;

            // 静的NoEntryZoneとの衝突判定
            foreach (var zone in zones)
            {
                // X軸方向の移動をスイープでチェック（移動経路全体をカバー）
                Rectangle xMovement = SweepBoundsHelper.CreateSweepBoundsXAxis(currentBounds, proposedBounds);

                if (xMovement.IntersectsWith(zone.Bounds))
                {
                    int candidateX;
                    // 不可侵領域との位置関係に基づいて調整
                    if (currentBounds.X + currentBounds.Width <= zone.Bounds.X)
                    {
                        // 左から右への移動時
                        candidateX = zone.Bounds.X - proposedBounds.Width;
                    }
                    else if (currentBounds.X >= zone.Bounds.X + zone.Bounds.Width)
                    {
                        // 右から左への移動時
                        candidateX = zone.Bounds.X + zone.Bounds.Width;
                    }
                    else
                    {
                        candidateX = currentBounds.X;
                    }

                    // 最も現在位置に近い調整を保持（移動距離が最小）
                    if (!bestAdjustedX.HasValue ||
                        Math.Abs(candidateX - currentBounds.X) < Math.Abs(bestAdjustedX.Value - currentBounds.X))
                    {
                        bestAdjustedX = candidateX;
                    }
                }

                // Y軸方向の移動をスイープでチェック（X軸調整後の位置から）
                Rectangle adjustedCurrentBounds = new Rectangle(
                    bestAdjustedX ?? currentBounds.X,
                    currentBounds.Y,
                    currentBounds.Width,
                    currentBounds.Height
                );
                Rectangle adjustedProposedBounds = new Rectangle(
                    bestAdjustedX ?? proposedBounds.X,
                    proposedBounds.Y,
                    proposedBounds.Width,
                    proposedBounds.Height
                );
                Rectangle yMovement = SweepBoundsHelper.CreateSweepBoundsYAxis(adjustedCurrentBounds, adjustedProposedBounds);

                if (yMovement.IntersectsWith(zone.Bounds))
                {
                    int candidateY;
                    // 不可侵領域との位置関係に基づいて調整
                    if (currentBounds.Y + currentBounds.Height <= zone.Bounds.Y)
                    {
                        // 上から下への移動時
                        candidateY = zone.Bounds.Y - proposedBounds.Height;
                    }
                    else if (currentBounds.Y >= zone.Bounds.Y + zone.Bounds.Height)
                    {
                        // 下から上への移動時
                        candidateY = zone.Bounds.Y + zone.Bounds.Height;
                    }
                    else
                    {
                        candidateY = currentBounds.Y;
                    }

                    // 最も現在位置に近い調整を保持（移動距離が最小）
                    if (!bestAdjustedY.HasValue ||
                        Math.Abs(candidateY - currentBounds.Y) < Math.Abs(bestAdjustedY.Value - currentBounds.Y))
                    {
                        bestAdjustedY = candidateY;
                    }
                }
            }

            // 不可侵ウィンドウ境界調整（Z-order + Region考慮） - boundaryCollider経由
            if (boundaryCollider != null)
            {
                foreach (var window in boundaryCollider.NoEntryWindows)
                {
                    // 除外ウィンドウはスキップ（自分自身の境界との衝突を防ぐ）
                    if (excludeWindow != null && window == excludeWindow)
                    {
                        continue;
                    }

                    if (window.WindowState == FormWindowState.Minimized || window.IsMinimized)
                    {
                        continue;
                    }

                    // Y軸方向の移動をスイープでチェック（Z-order + Region考慮、X軸調整後の位置から）
                    Rectangle adjustedCurrentForBoundary = new Rectangle(
                        bestAdjustedX ?? currentBounds.X,
                        currentBounds.Y,
                        currentBounds.Width,
                        currentBounds.Height
                    );
                    Rectangle adjustedProposedForBoundary = new Rectangle(
                        bestAdjustedX ?? proposedBounds.X,
                        proposedBounds.Y,
                        proposedBounds.Width,
                        proposedBounds.Height
                    );
                    Rectangle yMovement = SweepBoundsHelper.CreateSweepBoundsYAxis(adjustedCurrentForBoundary, adjustedProposedForBoundary);

                    if (boundaryCollider.CheckCollision(window, yMovement, out var yRect) && yRect.HasValue)
                    {
                        int candidateY;
                        // Z-order可視性を考慮した調整
                        if (currentBounds.Y + currentBounds.Height <= yRect.Value.Y)
                        {
                            candidateY = yRect.Value.Y - proposedBounds.Height;
                        }
                        else if (currentBounds.Y >= yRect.Value.Y + yRect.Value.Height)
                        {
                            candidateY = yRect.Value.Y + yRect.Value.Height;
                        }
                        else
                        {
                            candidateY = currentBounds.Y;
                        }

                        // 最も現在位置に近い調整を保持（移動距離が最小）
                        if (!bestAdjustedY.HasValue ||
                            Math.Abs(candidateY - currentBounds.Y) < Math.Abs(bestAdjustedY.Value - currentBounds.Y))
                        {
                            bestAdjustedY = candidateY;
                        }
                    }

                    // X軸方向の移動をスイープでチェック（Z-order + Region考慮）
                    Rectangle xMovement = SweepBoundsHelper.CreateSweepBoundsXAxis(currentBounds, proposedBounds);

                    if (boundaryCollider.CheckCollision(window, xMovement, out var xRect) && xRect.HasValue)
                    {
                        int candidateX;
                        // Z-order可視性を考慮した調整
                        if (currentBounds.X + currentBounds.Width <= xRect.Value.X)
                        {
                            candidateX = xRect.Value.X - proposedBounds.Width;
                        }
                        else if (currentBounds.X >= xRect.Value.X + xRect.Value.Width)
                        {
                            candidateX = xRect.Value.X + xRect.Value.Width;
                        }
                        else
                        {
                            candidateX = currentBounds.X;
                        }

                        // 最も現在位置に近い調整を保持（移動距離が最小）
                        if (!bestAdjustedX.HasValue ||
                            Math.Abs(candidateX - currentBounds.X) < Math.Abs(bestAdjustedX.Value - currentBounds.X))
                        {
                            bestAdjustedX = candidateX;
                        }
                    }
                }
            }

            // 最適な調整を適用（静的NoEntryZoneと不可侵ウィンドウ境界）
            if (bestAdjustedX.HasValue)
            {
                adjustedBounds.X = bestAdjustedX.Value;
            }
            if (bestAdjustedY.HasValue)
            {
                adjustedBounds.Y = bestAdjustedY.Value;
            }

            // 不可侵ウィンドウが移動する場合、通常ウィンドウとも衝突判定
            if (excludeWindow != null && excludeWindow.IsNoEntryWindow && windowManager != null)
            {
                var allWindows = windowManager.GetAllWindows();

                foreach (var window in allWindows)
                {
                    // 自分自身はスキップ
                    if (window == excludeWindow)
                    {
                        continue;
                    }

                    // 不可侵ウィンドウはスキップ（既に上でチェック済み）
                    if (window.IsNoEntryWindow)
                    {
                        continue;
                    }

                    // 最小化されているウィンドウはスキップ
                    if (window.WindowState == FormWindowState.Minimized || window.IsMinimized)
                    {
                        continue;
                    }

                    var windowBounds = window.CollisionBounds;

                    // Y軸方向の移動をスイープでチェック（X軸調整後の位置から）
                    Rectangle adjustedCurrentForNormal = new Rectangle(
                        bestAdjustedX ?? currentBounds.X,
                        currentBounds.Y,
                        currentBounds.Width,
                        currentBounds.Height
                    );
                    Rectangle adjustedProposedForNormal = new Rectangle(
                        bestAdjustedX ?? proposedBounds.X,
                        proposedBounds.Y,
                        proposedBounds.Width,
                        proposedBounds.Height
                    );
                    Rectangle yMovement = SweepBoundsHelper.CreateSweepBoundsYAxis(adjustedCurrentForNormal, adjustedProposedForNormal);

                    // 通常ウィンドウの可視性をチェック（Z-order考慮）
                    if (visibilityService != null && !visibilityService.IsWindowVisibleFromNoEntry(window, yMovement, excludeWindow))
                    {
                        continue;  // 隠れている場合はスキップ
                    }

                    if (yMovement.IntersectsWith(windowBounds))
                    {
                        int candidateY;
                        // 通常ウィンドウとの位置関係に基づいて調整
                        if (currentBounds.Y + currentBounds.Height <= windowBounds.Y)
                        {
                            // 上から下への移動時
                            candidateY = windowBounds.Y - proposedBounds.Height;
                        }
                        else if (currentBounds.Y >= windowBounds.Y + windowBounds.Height)
                        {
                            // 下から上への移動時
                            candidateY = windowBounds.Y + windowBounds.Height;
                        }
                        else
                        {
                            candidateY = currentBounds.Y;
                        }

                        // 最も現在位置に近い調整を保持（移動距離が最小）
                        if (!bestAdjustedY.HasValue ||
                            Math.Abs(candidateY - currentBounds.Y) < Math.Abs(bestAdjustedY.Value - currentBounds.Y))
                        {
                            bestAdjustedY = candidateY;
                        }
                    }

                    // X軸方向の移動をスイープでチェック
                    Rectangle xMovement = SweepBoundsHelper.CreateSweepBoundsXAxis(currentBounds, proposedBounds);

                    // 通常ウィンドウの可視性をチェック（Z-order考慮）
                    if (visibilityService != null && !visibilityService.IsWindowVisibleFromNoEntry(window, xMovement, excludeWindow))
                    {
                        continue;  // 隠れている場合はスキップ
                    }

                    if (xMovement.IntersectsWith(windowBounds))
                    {
                        int candidateX;
                        // 通常ウィンドウとの位置関係に基づいて調整
                        if (currentBounds.X + currentBounds.Width <= windowBounds.X)
                        {
                            // 左から右への移動時
                            candidateX = windowBounds.X - proposedBounds.Width;
                        }
                        else if (currentBounds.X >= windowBounds.X + windowBounds.Width)
                        {
                            // 右から左への移動時
                            candidateX = windowBounds.X + windowBounds.Width;
                        }
                        else
                        {
                            candidateX = currentBounds.X;
                        }

                        // 最も現在位置に近い調整を保持（移動距離が最小）
                        if (!bestAdjustedX.HasValue ||
                            Math.Abs(candidateX - currentBounds.X) < Math.Abs(bestAdjustedX.Value - currentBounds.X))
                        {
                            bestAdjustedX = candidateX;
                        }
                    }
                }

                // 最適な調整を適用
                if (bestAdjustedX.HasValue)
                {
                    adjustedBounds.X = bestAdjustedX.Value;
                }
                if (bestAdjustedY.HasValue)
                {
                    adjustedBounds.Y = bestAdjustedY.Value;
                }
            }

            return adjustedBounds;
        }

        public Size GetValidSize(Rectangle currentBounds, Size proposedSize, GameWindow? excludeWindow = null)
        {
            Size adjustedSize = proposedSize;

            // リサイズ方向を判定
            bool isGrowingWidth = proposedSize.Width > currentBounds.Width;
            bool isGrowingHeight = proposedSize.Height > currentBounds.Height;

            // 調整候補を収集（最も制約が厳しい = 最小のサイズを採用）
            int? minWidth = null;
            int? minHeight = null;

            // 静的NoEntryZoneとの衝突判定
            foreach (var zone in zones)
            {
                // X方向の拡大をチェック
                if (isGrowingWidth)
                {
                    Rectangle xResize = new Rectangle(
                        currentBounds.X,
                        currentBounds.Y,
                        proposedSize.Width,
                        currentBounds.Height
                    );

                    if (xResize.IntersectsWith(zone.Bounds))
                    {
                        if (currentBounds.X < zone.Bounds.X)
                        {
                            int candidateWidth = zone.Bounds.X - currentBounds.X;
                            // 最小の幅を保持
                            if (!minWidth.HasValue || candidateWidth < minWidth.Value)
                            {
                                minWidth = candidateWidth;
                            }
                        }
                    }
                }

                // Y方向の拡大をチェック
                if (isGrowingHeight)
                {
                    Rectangle yResize = new Rectangle(
                        currentBounds.X,
                        currentBounds.Y,
                        minWidth ?? proposedSize.Width,
                        proposedSize.Height
                    );

                    if (yResize.IntersectsWith(zone.Bounds))
                    {
                        if (currentBounds.Y < zone.Bounds.Y)
                        {
                            int candidateHeight = zone.Bounds.Y - currentBounds.Y;
                            // 最小の高さを保持
                            if (!minHeight.HasValue || candidateHeight < minHeight.Value)
                            {
                                minHeight = candidateHeight;
                            }
                        }
                    }
                }
            }

            // 不可侵ウィンドウ境界調整（Z-order + Region考慮） - boundaryCollider経由
            if (boundaryCollider != null)
            {
                foreach (var window in boundaryCollider.NoEntryWindows)
                {
                    // 除外ウィンドウはスキップ（自分自身の境界との衝突を防ぐ）
                    if (excludeWindow != null && window == excludeWindow)
                    {
                        continue;
                    }

                    if (window.WindowState == FormWindowState.Minimized || window.IsMinimized)
                    {
                        continue;
                    }

                    // X方向の拡大をチェック
                    if (isGrowingWidth)
                    {
                        Rectangle xResize = new Rectangle(
                            currentBounds.X,
                            currentBounds.Y,
                            proposedSize.Width,
                            currentBounds.Height
                        );

                        if (boundaryCollider.CheckCollision(window, xResize, out var xRect) && xRect.HasValue)
                        {
                            if (currentBounds.X < xRect.Value.X)
                            {
                                int candidateWidth = xRect.Value.X - currentBounds.X;
                                // 最小の幅を保持
                                if (!minWidth.HasValue || candidateWidth < minWidth.Value)
                                {
                                    minWidth = candidateWidth;
                                }
                            }
                        }
                    }

                    // Y方向の拡大をチェック
                    if (isGrowingHeight)
                    {
                        Rectangle yResize = new Rectangle(
                            currentBounds.X,
                            currentBounds.Y,
                            minWidth ?? proposedSize.Width,
                            proposedSize.Height
                        );

                        if (boundaryCollider.CheckCollision(window, yResize, out var yRect) && yRect.HasValue)
                        {
                            if (currentBounds.Y < yRect.Value.Y)
                            {
                                int candidateHeight = yRect.Value.Y - currentBounds.Y;
                                // 最小の高さを保持
                                if (!minHeight.HasValue || candidateHeight < minHeight.Value)
                                {
                                    minHeight = candidateHeight;
                                }
                            }
                        }
                    }
                }
            }

            // 不可侵ウィンドウがリサイズする場合、通常ウィンドウとも衝突判定
            if (excludeWindow != null && excludeWindow.IsNoEntryWindow && windowManager != null)
            {
                var allWindows = windowManager.GetAllWindows();
                foreach (var window in allWindows)
                {
                    // 自分自身はスキップ
                    if (window == excludeWindow)
                    {
                        continue;
                    }

                    // 不可侵ウィンドウはスキップ（既に上でチェック済み）
                    if (window.IsNoEntryWindow)
                    {
                        continue;
                    }

                    // 最小化されているウィンドウはスキップ
                    if (window.WindowState == FormWindowState.Minimized || window.IsMinimized)
                    {
                        continue;
                    }

                    var windowBounds = window.CollisionBounds;

                    // X方向の拡大をチェック
                    if (isGrowingWidth)
                    {
                        Rectangle xResize = new Rectangle(
                            currentBounds.X,
                            currentBounds.Y,
                            proposedSize.Width,
                            currentBounds.Height
                        );

                        if (xResize.IntersectsWith(windowBounds))
                        {
                            if (currentBounds.X < windowBounds.X)
                            {
                                int candidateWidth = windowBounds.X - currentBounds.X;
                                // 最小の幅を保持
                                if (!minWidth.HasValue || candidateWidth < minWidth.Value)
                                {
                                    minWidth = candidateWidth;
                                }
                            }
                        }
                    }

                    // Y方向の拡大をチェック
                    if (isGrowingHeight)
                    {
                        Rectangle yResize = new Rectangle(
                            currentBounds.X,
                            currentBounds.Y,
                            minWidth ?? proposedSize.Width,
                            proposedSize.Height
                        );

                        if (yResize.IntersectsWith(windowBounds))
                        {
                            if (currentBounds.Y < windowBounds.Y)
                            {
                                int candidateHeight = windowBounds.Y - currentBounds.Y;
                                // 最小の高さを保持
                                if (!minHeight.HasValue || candidateHeight < minHeight.Value)
                                {
                                    minHeight = candidateHeight;
                                }
                            }
                        }
                    }
                }
            }

            // 最終的なサイズを適用（すべての衝突判定を考慮）
            if (minWidth.HasValue && isGrowingWidth)
            {
                adjustedSize.Width = minWidth.Value;
            }
            if (minHeight.HasValue && isGrowingHeight)
            {
                adjustedSize.Height = minHeight.Value;
            }

            return adjustedSize;
        }

        public void Draw(Graphics g)
        {
            if (!MainGame.IsDebugMode) return;

            foreach (var zone in zones)
            {
                zone.Draw(g);
            }
        }

        #region 不可侵ウィンドウ管理メソッド（委譲パターン）

        /// <summary>
        /// 不可侵ウィンドウを登録し、境界の不可侵領域を生成
        /// </summary>
        public void RegisterNoEntryWindow(GameWindow window)
        {
            boundaryCollider?.RegisterWindow(window);
            UpdateNoEntryZonesForWindow(window);
        }

        /// <summary>
        /// 不可侵ウィンドウの登録を解除し、関連する不可侵領域を削除
        /// </summary>
        public void UnregisterNoEntryWindow(GameWindow window)
        {
            RemoveNoEntryZonesForWindow(window);
            boundaryCollider?.UnregisterWindow(window);
        }

        /// <summary>
        /// 不可侵ウィンドウの境界に沿って不可侵領域を更新
        /// 注: Z-order対応により、NoEntryZoneオブジェクトは生成せず、境界判定のみ使用
        /// </summary>
        public void UpdateNoEntryZonesForWindow(GameWindow window)
        {
            // ウィンドウが最小化されている場合は何もしない
            if (window.WindowState == FormWindowState.Minimized || window.IsMinimized)
            {
                return;
            }

            // 不可侵ウィンドウリストに含まれていることを確認するだけ
            // 実際の境界判定はboundaryCollider経由で行う
            // （NoEntryZoneオブジェクトは生成しない - パフォーマンス改善）
        }

        /// <summary>
        /// 不可侵ウィンドウに関連する不可侵領域を削除
        /// 注: Z-order対応により、NoEntryZoneオブジェクトを生成しないため、何もしない
        /// </summary>
        public void RemoveNoEntryZonesForWindow(GameWindow window)
        {
            // NoEntryZoneオブジェクトを生成しないため、削除処理も不要
            // 登録解除はUnregisterNoEntryWindowで行う
        }

        /// <summary>
        /// Z-orderを考慮して、不可侵ウィンドウの指定領域が見えているかチェック（委譲）
        /// </summary>
        public bool IsNoEntryWindowVisible(GameWindow window, Rectangle checkBounds)
        {
            if (boundaryCollider == null || visibilityService == null)
            {
                return true;
            }

            return visibilityService.IsNoEntryWindowVisible(window, checkBounds, boundaryCollider.NoEntryWindows.ToList());
        }

        /// <summary>
        /// 不可侵ウィンドウの4辺境界Rectangleを取得（委譲）
        /// </summary>
        public List<Rectangle> GetNoEntryBoundaryRectangles(GameWindow window)
        {
            if (boundaryCollider == null)
            {
                return new List<Rectangle>();
            }

            return boundaryCollider.GetBoundaryRectangles(window);
        }

        /// <summary>
        /// 単一の不可侵ウィンドウとの境界衝突をZ-order + Region考慮で判定（委譲）
        /// </summary>
        public bool CheckNoEntryBoundaryCollision(GameWindow window, Rectangle checkBounds, out Rectangle? collisionRect)
        {
            if (boundaryCollider == null)
            {
                collisionRect = null;
                return false;
            }

            return boundaryCollider.CheckCollision(window, checkBounds, out collisionRect);
        }

        /// <summary>
        /// すべての不可侵ウィンドウとの境界衝突をチェック（委譲）
        /// </summary>
        public bool CheckAnyNoEntryBoundaryCollision(
            Rectangle checkBounds,
            out GameWindow? collidingWindow,
            out Rectangle? collisionRect,
            GameWindow? excludeWindow = null,
            bool excludeChildren = false)
        {
            if (boundaryCollider == null)
            {
                collidingWindow = null;
                collisionRect = null;
                return false;
            }

            return boundaryCollider.CheckAnyCollision(checkBounds, out collidingWindow, out collisionRect, excludeWindow, excludeChildren);
        }

        #endregion
    }
}
