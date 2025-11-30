using System;
using System.Drawing;

namespace MultiWindowActionGame.Services
{
    /// <summary>
    /// Sweep Bounds（移動経路全体をカバーする境界）の計算を提供するヘルパークラス
    /// 高速移動時の衝突判定漏れを防ぐために使用
    /// </summary>
    public static class SweepBoundsHelper
    {
        /// <summary>
        /// 両軸のSweep Boundsを作成
        /// </summary>
        /// <param name="currentBounds">現在の境界</param>
        /// <param name="proposedBounds">移動後の境界</param>
        /// <returns>現在位置と移動後位置を包含する矩形</returns>
        public static Rectangle CreateSweepBounds(Rectangle currentBounds, Rectangle proposedBounds)
        {
            int minX = Math.Min(currentBounds.X, proposedBounds.X);
            int maxX = Math.Max(currentBounds.Right, proposedBounds.Right);
            int minY = Math.Min(currentBounds.Y, proposedBounds.Y);
            int maxY = Math.Max(currentBounds.Bottom, proposedBounds.Bottom);

            return new Rectangle(minX, minY, maxX - minX, maxY - minY);
        }

        /// <summary>
        /// X軸のみのSweep Boundsを作成（Y座標は現在位置を維持）
        /// </summary>
        /// <param name="currentBounds">現在の境界</param>
        /// <param name="proposedBounds">移動後の境界</param>
        /// <returns>X軸方向の移動をカバーする矩形</returns>
        public static Rectangle CreateSweepBoundsXAxis(Rectangle currentBounds, Rectangle proposedBounds)
        {
            int minX = Math.Min(currentBounds.X, proposedBounds.X);
            int maxX = Math.Max(currentBounds.Right, proposedBounds.Right);

            return new Rectangle(minX, currentBounds.Y, maxX - minX, currentBounds.Height);
        }

        /// <summary>
        /// Y軸のみのSweep Boundsを作成（X座標は提案位置を使用）
        /// </summary>
        /// <param name="currentBounds">現在の境界</param>
        /// <param name="proposedBounds">移動後の境界</param>
        /// <returns>Y軸方向の移動をカバーする矩形</returns>
        public static Rectangle CreateSweepBoundsYAxis(Rectangle currentBounds, Rectangle proposedBounds)
        {
            int minY = Math.Min(currentBounds.Y, proposedBounds.Y);
            int maxY = Math.Max(currentBounds.Bottom, proposedBounds.Bottom);

            return new Rectangle(proposedBounds.X, minY, proposedBounds.Width, maxY - minY);
        }

        /// <summary>
        /// 垂直方向の足元Sweep Boundsを作成（PlayerPhysics接地判定用）
        /// </summary>
        /// <param name="feetBounds">足元の境界</param>
        /// <param name="verticalMovement">垂直方向の移動量（ピクセル）</param>
        /// <param name="maxStep">最大ステップ距離</param>
        /// <param name="margin">追加のマージン（上下）</param>
        /// <returns>垂直方向の移動をカバーする矩形（マージン付き）</returns>
        public static Rectangle CreateVerticalSweepBounds(
            Rectangle feetBounds,
            float verticalMovement,
            float maxStep,
            int margin = 5)
        {
            int minY = Math.Min(feetBounds.Y, feetBounds.Y + (int)verticalMovement) - margin;
            int height = (int)maxStep + feetBounds.Height + margin * 2;

            return new Rectangle(feetBounds.X, minY, feetBounds.Width, height);
        }
    }
}
