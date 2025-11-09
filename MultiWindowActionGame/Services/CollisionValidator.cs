using System;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;
using MultiWindowActionGame.Collision;
using MultiWindowActionGame.Interfaces;
using MultiWindowActionGame.Managers;
using MultiWindowActionGame.Windows;

namespace MultiWindowActionGame.Services
{
    /// <summary>
    /// 位置とサイズの検証ロジックを提供
    /// NoEntryZoneManagerのGetValidPosition/GetValidSizeロジックを移植
    /// </summary>
    public class CollisionValidator
    {
        private readonly INoEntryZoneManager noEntryZoneManager;
        private readonly NoEntryBoundaryCollider boundaryCollider;
        private readonly ZOrderCollisionHelper zOrderHelper;
        private readonly IWindowManager windowManager;

        public CollisionValidator(
            INoEntryZoneManager noEntryZoneManager,
            NoEntryBoundaryCollider boundaryCollider,
            ZOrderCollisionHelper zOrderHelper,
            IWindowManager windowManager)
        {
            this.noEntryZoneManager = noEntryZoneManager ?? throw new ArgumentNullException(nameof(noEntryZoneManager));
            this.boundaryCollider = boundaryCollider ?? throw new ArgumentNullException(nameof(boundaryCollider));
            this.zOrderHelper = zOrderHelper ?? throw new ArgumentNullException(nameof(zOrderHelper));
            this.windowManager = windowManager ?? throw new ArgumentNullException(nameof(windowManager));
        }

        /// <summary>
        /// Sweep Boundsを作成（現在位置から目標位置までの移動経路全体をカバー）
        /// </summary>
        private Rectangle CreateSweepBounds(Rectangle currentBounds, Rectangle proposedBounds, bool xAxisOnly, bool yAxisOnly)
        {
            int minX = Math.Min(currentBounds.X, proposedBounds.X);
            int maxX = Math.Max(currentBounds.Right, proposedBounds.Right);
            int minY = Math.Min(currentBounds.Y, proposedBounds.Y);
            int maxY = Math.Max(currentBounds.Bottom, proposedBounds.Bottom);

            // X軸のみの場合、Y座標は現在位置を使用
            if (xAxisOnly)
            {
                return new Rectangle(minX, currentBounds.Y, maxX - minX, currentBounds.Height);
            }
            // Y軸のみの場合、X座標は現在位置または調整後のX位置を使用
            else if (yAxisOnly)
            {
                return new Rectangle(proposedBounds.X, minY, proposedBounds.Width, maxY - minY);
            }
            // 両軸の場合
            else
            {
                return new Rectangle(minX, minY, maxX - minX, maxY - minY);
            }
        }

        /// <summary>
        /// 移動可能な位置に調整した境界を返す（Sweep方式で移動経路全体をチェック）
        /// NoEntryZoneManager.GetValidPositionから移植 + Sweep機能追加
        /// </summary>
        public Rectangle ValidatePosition(
            Rectangle currentBounds,
            Rectangle proposedBounds,
            CollisionOptions options)
        {
            Rectangle adjustedBounds = proposedBounds;
            int? bestAdjustedX = null;
            int? bestAdjustedY = null;

            // 静的NoEntryZoneとの衝突判定（Sweep方式）
            if (options.CheckNoEntryZones)
            {
                foreach (var zone in noEntryZoneManager.Zones)
                {
                    // X軸方向の移動をチェック（移動経路全体をSweep）
                    Rectangle xMovement = CreateSweepBounds(
                        new Rectangle(currentBounds.X, currentBounds.Y, currentBounds.Width, currentBounds.Height),
                        new Rectangle(proposedBounds.X, currentBounds.Y, proposedBounds.Width, currentBounds.Height),
                        xAxisOnly: true,
                        yAxisOnly: false
                    );

                    if (xMovement.IntersectsWith(zone.Bounds))
                    {
                        int candidateX;
                        if (currentBounds.X + currentBounds.Width <= zone.Bounds.X)
                        {
                            candidateX = zone.Bounds.X - proposedBounds.Width;
                        }
                        else if (currentBounds.X >= zone.Bounds.X + zone.Bounds.Width)
                        {
                            candidateX = zone.Bounds.X + zone.Bounds.Width;
                        }
                        else
                        {
                            candidateX = currentBounds.X;
                        }

                        if (!bestAdjustedX.HasValue ||
                            Math.Abs(candidateX - currentBounds.X) < Math.Abs(bestAdjustedX.Value - currentBounds.X))
                        {
                            bestAdjustedX = candidateX;
                        }
                    }

                    // Y軸方向の移動をチェック（移動経路全体をSweep）
                    int adjustedX = bestAdjustedX ?? proposedBounds.X;
                    Rectangle yMovement = CreateSweepBounds(
                        new Rectangle(adjustedX, currentBounds.Y, proposedBounds.Width, currentBounds.Height),
                        new Rectangle(adjustedX, proposedBounds.Y, proposedBounds.Width, proposedBounds.Height),
                        xAxisOnly: false,
                        yAxisOnly: true
                    );

                    if (yMovement.IntersectsWith(zone.Bounds))
                    {
                        int candidateY;
                        if (currentBounds.Y + currentBounds.Height <= zone.Bounds.Y)
                        {
                            candidateY = zone.Bounds.Y - proposedBounds.Height;
                        }
                        else if (currentBounds.Y >= zone.Bounds.Y + zone.Bounds.Height)
                        {
                            candidateY = zone.Bounds.Y + zone.Bounds.Height;
                        }
                        else
                        {
                            candidateY = currentBounds.Y;
                        }

                        if (!bestAdjustedY.HasValue ||
                            Math.Abs(candidateY - currentBounds.Y) < Math.Abs(bestAdjustedY.Value - currentBounds.Y))
                        {
                            bestAdjustedY = candidateY;
                        }
                    }
                }
            }

            // 不可侵ウィンドウ境界調整（ExcludeChildren対応）
            if (options.CheckNoEntryBoundaries)
            {
                // Y軸方向の移動をチェック（移動経路全体をSweep）
                int adjustedXForBoundary = bestAdjustedX ?? proposedBounds.X;
                Rectangle yMovement = CreateSweepBounds(
                    new Rectangle(adjustedXForBoundary, currentBounds.Y, proposedBounds.Width, currentBounds.Height),
                    new Rectangle(adjustedXForBoundary, proposedBounds.Y, proposedBounds.Width, proposedBounds.Height),
                    xAxisOnly: false,
                    yAxisOnly: true
                );

                // CheckAnyCollisionを使用（ExcludeChildren対応）
                if (boundaryCollider.CheckAnyCollision(
                    yMovement,
                    out var collidingWindowY,
                    out var yRect,
                    options.ExcludeWindow,
                    options.ExcludeChildren) && yRect.HasValue)
                {
                    int candidateY;
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

                    if (!bestAdjustedY.HasValue ||
                        Math.Abs(candidateY - currentBounds.Y) < Math.Abs(bestAdjustedY.Value - currentBounds.Y))
                    {
                        bestAdjustedY = candidateY;
                    }
                }

                // X軸方向の移動をチェック（移動経路全体をSweep）
                Rectangle xMovement = CreateSweepBounds(
                    new Rectangle(currentBounds.X, currentBounds.Y, currentBounds.Width, currentBounds.Height),
                    new Rectangle(proposedBounds.X, currentBounds.Y, proposedBounds.Width, currentBounds.Height),
                    xAxisOnly: true,
                    yAxisOnly: false
                );

                // CheckAnyCollisionを使用（ExcludeChildren対応）
                if (boundaryCollider.CheckAnyCollision(
                    xMovement,
                    out var collidingWindowX,
                    out var xRect,
                    options.ExcludeWindow,
                    options.ExcludeChildren) && xRect.HasValue)
                {
                    int candidateX;
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

                    if (!bestAdjustedX.HasValue ||
                        Math.Abs(candidateX - currentBounds.X) < Math.Abs(bestAdjustedX.Value - currentBounds.X))
                    {
                        bestAdjustedX = candidateX;
                    }
                }
            }

            // 通常ウィンドウとの衝突判定（不可侵ウィンドウが移動する場合）
            if (options.CheckNormalWindows)
            {
                var allWindows = windowManager.GetAllWindows();

                foreach (var window in allWindows)
                {
                    // 除外ウィンドウはスキップ
                    if (window == options.ExcludeWindow) continue;

                    // 親ウィンドウもスキップ（子が親の内部で移動する場合、親自体は障害物にならない）
                    if (options.ExcludeWindow != null && window == options.ExcludeWindow.Parent) continue;

                    // 不可侵ウィンドウはスキップ（境界判定で処理済み）
                    if (window.IsNoEntryWindow) continue;

                    // 最小化されているウィンドウはスキップ
                    if (window.WindowState == FormWindowState.Minimized || window.IsMinimized) continue;

                    Rectangle windowBounds = window.CollisionBounds;

                    // X軸方向の移動をチェック（移動経路全体をSweep）
                    Rectangle xMovement = CreateSweepBounds(
                        new Rectangle(currentBounds.X, currentBounds.Y, currentBounds.Width, currentBounds.Height),
                        new Rectangle(proposedBounds.X, currentBounds.Y, proposedBounds.Width, currentBounds.Height),
                        xAxisOnly: true,
                        yAxisOnly: false
                    );

                    if (xMovement.IntersectsWith(windowBounds))
                    {
                        int candidateX;
                        if (currentBounds.X + currentBounds.Width <= windowBounds.X)
                        {
                            candidateX = windowBounds.X - proposedBounds.Width;
                        }
                        else if (currentBounds.X >= windowBounds.X + windowBounds.Width)
                        {
                            candidateX = windowBounds.X + windowBounds.Width;
                        }
                        else
                        {
                            candidateX = currentBounds.X;
                        }

                        if (!bestAdjustedX.HasValue ||
                            Math.Abs(candidateX - currentBounds.X) < Math.Abs(bestAdjustedX.Value - currentBounds.X))
                        {
                            bestAdjustedX = candidateX;
                        }
                    }

                    // Y軸方向の移動をチェック（移動経路全体をSweep）
                    int adjustedXForNormalWindow = bestAdjustedX ?? proposedBounds.X;
                    Rectangle yMovement = CreateSweepBounds(
                        new Rectangle(adjustedXForNormalWindow, currentBounds.Y, proposedBounds.Width, currentBounds.Height),
                        new Rectangle(adjustedXForNormalWindow, proposedBounds.Y, proposedBounds.Width, proposedBounds.Height),
                        xAxisOnly: false,
                        yAxisOnly: true
                    );

                    if (yMovement.IntersectsWith(windowBounds))
                    {
                        int candidateY;
                        if (currentBounds.Y + currentBounds.Height <= windowBounds.Y)
                        {
                            candidateY = windowBounds.Y - proposedBounds.Height;
                        }
                        else if (currentBounds.Y >= windowBounds.Y + windowBounds.Height)
                        {
                            candidateY = windowBounds.Y + windowBounds.Height;
                        }
                        else
                        {
                            candidateY = currentBounds.Y;
                        }

                        if (!bestAdjustedY.HasValue ||
                            Math.Abs(candidateY - currentBounds.Y) < Math.Abs(bestAdjustedY.Value - currentBounds.Y))
                        {
                            bestAdjustedY = candidateY;
                        }
                    }
                }
            }

            // ボタンとの衝突判定（不可侵ウィンドウが移動する場合）
            if (options.CheckButtons)
            {
                var allButtons = windowManager.GetAllButtons();

                foreach (var button in allButtons)
                {
                    // 最小化されているボタンはスキップ
                    if (!button.Visible) continue;

                    Rectangle buttonBounds = button.Bounds;

                    // X軸方向の移動をチェック（移動経路全体をSweep）
                    Rectangle xMovement = CreateSweepBounds(
                        new Rectangle(currentBounds.X, currentBounds.Y, currentBounds.Width, currentBounds.Height),
                        new Rectangle(proposedBounds.X, currentBounds.Y, proposedBounds.Width, currentBounds.Height),
                        xAxisOnly: true,
                        yAxisOnly: false
                    );

                    if (xMovement.IntersectsWith(buttonBounds))
                    {
                        int candidateX;
                        if (currentBounds.X + currentBounds.Width <= buttonBounds.X)
                        {
                            candidateX = buttonBounds.X - proposedBounds.Width;
                        }
                        else if (currentBounds.X >= buttonBounds.X + buttonBounds.Width)
                        {
                            candidateX = buttonBounds.X + buttonBounds.Width;
                        }
                        else
                        {
                            candidateX = currentBounds.X;
                        }

                        if (!bestAdjustedX.HasValue ||
                            Math.Abs(candidateX - currentBounds.X) < Math.Abs(bestAdjustedX.Value - currentBounds.X))
                        {
                            bestAdjustedX = candidateX;
                        }
                    }

                    // Y軸方向の移動をチェック（移動経路全体をSweep）
                    int adjustedXForButton = bestAdjustedX ?? proposedBounds.X;
                    Rectangle yMovement = CreateSweepBounds(
                        new Rectangle(adjustedXForButton, currentBounds.Y, proposedBounds.Width, currentBounds.Height),
                        new Rectangle(adjustedXForButton, proposedBounds.Y, proposedBounds.Width, proposedBounds.Height),
                        xAxisOnly: false,
                        yAxisOnly: true
                    );

                    if (yMovement.IntersectsWith(buttonBounds))
                    {
                        int candidateY;
                        if (currentBounds.Y + currentBounds.Height <= buttonBounds.Y)
                        {
                            candidateY = buttonBounds.Y - proposedBounds.Height;
                        }
                        else if (currentBounds.Y >= buttonBounds.Y + buttonBounds.Height)
                        {
                            candidateY = buttonBounds.Y + buttonBounds.Height;
                        }
                        else
                        {
                            candidateY = currentBounds.Y;
                        }

                        if (!bestAdjustedY.HasValue ||
                            Math.Abs(candidateY - currentBounds.Y) < Math.Abs(bestAdjustedY.Value - currentBounds.Y))
                        {
                            bestAdjustedY = candidateY;
                        }
                    }
                }
            }

            // 最終的な調整を適用
            return new Rectangle(
                bestAdjustedX ?? proposedBounds.X,
                bestAdjustedY ?? proposedBounds.Y,
                proposedBounds.Width,
                proposedBounds.Height
            );
        }

        /// <summary>
        /// リサイズ可能なサイズに調整したサイズを返す
        /// NoEntryZoneManager.GetValidSizeから移植
        /// </summary>
        public Size ValidateSize(
            Rectangle currentBounds,
            Size proposedSize,
            CollisionOptions options)
        {
            Size adjustedSize = proposedSize;
            bool isGrowingWidth = proposedSize.Width > currentBounds.Width;
            bool isGrowingHeight = proposedSize.Height > currentBounds.Height;
            int? minWidth = null;
            int? minHeight = null;

            // 静的NoEntryZoneとの衝突判定
            if (options.CheckNoEntryZones)
            {
                foreach (var zone in noEntryZoneManager.Zones)
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
                                if (!minHeight.HasValue || candidateHeight < minHeight.Value)
                                {
                                    minHeight = candidateHeight;
                                }
                            }
                        }
                    }
                }
            }

            // 不可侵ウィンドウ境界調整（ExcludeChildren対応）
            if (options.CheckNoEntryBoundaries)
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

                    // CheckAnyCollisionを使用（ExcludeChildren対応）
                    if (boundaryCollider.CheckAnyCollision(
                        xResize,
                        out var collidingWindow,
                        out var xRect,
                        options.ExcludeWindow,
                        options.ExcludeChildren) && xRect.HasValue)
                    {
                        if (currentBounds.X < xRect.Value.X)
                        {
                            int candidateWidth = xRect.Value.X - currentBounds.X;
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

                    // CheckAnyCollisionを使用（ExcludeChildren対応）
                    if (boundaryCollider.CheckAnyCollision(
                        yResize,
                        out var collidingWindow,
                        out var yRect,
                        options.ExcludeWindow,
                        options.ExcludeChildren) && yRect.HasValue)
                    {
                        if (currentBounds.Y < yRect.Value.Y)
                        {
                            int candidateHeight = yRect.Value.Y - currentBounds.Y;
                            if (!minHeight.HasValue || candidateHeight < minHeight.Value)
                            {
                                minHeight = candidateHeight;
                            }
                        }
                    }
                }
            }

            // 通常ウィンドウとの衝突判定（不可侵ウィンドウがリサイズする場合）
            if (options.CheckNormalWindows)
            {
                var allWindows = windowManager.GetAllWindows();

                foreach (var window in allWindows)
                {
                    // 除外ウィンドウとその子孫をスキップ
                    if (window == options.ExcludeWindow) continue;
                    if (options.ExcludeChildren && options.ExcludeWindow != null &&
                        options.ExcludeWindow.GetAllDescendants().Contains(window)) continue;

                    // 不可侵ウィンドウはスキップ（境界判定で処理済み）
                    if (window.IsNoEntryWindow) continue;

                    // 最小化されているウィンドウはスキップ
                    if (window.WindowState == FormWindowState.Minimized || window.IsMinimized) continue;

                    // X方向の拡大をチェック
                    if (isGrowingWidth)
                    {
                        Rectangle xResize = new Rectangle(
                            currentBounds.X,
                            currentBounds.Y,
                            proposedSize.Width,
                            currentBounds.Height
                        );

                        if (xResize.IntersectsWith(window.CollisionBounds))
                        {
                            // 左から右に拡大している場合
                            if (currentBounds.X < window.CollisionBounds.X)
                            {
                                int candidateWidth = window.CollisionBounds.X - currentBounds.X;
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
                            minWidth ?? proposedSize.Width,  // X軸で調整された幅を使用
                            proposedSize.Height
                        );

                        if (yResize.IntersectsWith(window.CollisionBounds))
                        {
                            // 上から下に拡大している場合
                            if (currentBounds.Y < window.CollisionBounds.Y)
                            {
                                int candidateHeight = window.CollisionBounds.Y - currentBounds.Y;
                                if (!minHeight.HasValue || candidateHeight < minHeight.Value)
                                {
                                    minHeight = candidateHeight;
                                }
                            }
                        }
                    }
                }
            }

            // 最終的なサイズを適用
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
    }
}
