using System;
using System.Drawing;
using System.Linq;
using MultiWindowActionGame.Interfaces;
using MultiWindowActionGame.Windows;
using MultiWindowActionGame.Collision;

namespace MultiWindowActionGame.Services
{
    /// <summary>
    /// CollisionServiceの拡張メソッド集
    /// PlayerPhysics.CheckGrounded()のロジックをCollisionService APIに統合
    /// </summary>
    public static class CollisionServiceExtensions
    {
        /// <summary>
        /// 接地判定を実行（PlayerPhysics.CheckGrounded()の統一化版）
        /// </summary>
        /// <param name="service">CollisionServiceインスタンス</param>
        /// <param name="bounds">プレイヤーの境界</param>
        /// <param name="sweepBounds">Sweep Bounds（垂直方向の移動経路全体）</param>
        /// <param name="parentWindow">親ウィンドウ（nullable）</param>
        /// <param name="options">衝突判定オプション</param>
        /// <returns>(接地している, 地面のY座標)</returns>
        public static (bool isGrounded, int groundY) CheckGrounded(
            this ICollisionService service,
            Rectangle bounds,
            Rectangle sweepBounds,
            GameWindow? parentWindow,
            CollisionOptions options)
        {
            // TODO: Phase 2.2で実装
            // 現在は未実装、Phase 2.2-2.3で段階的に実装する
            throw new NotImplementedException("Phase 2.2で実装予定");
        }
    }
}
