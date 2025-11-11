using System;
using System.Drawing;
using System.Numerics;
using System.Threading.Tasks;
using MultiWindowActionGame.Windows;

namespace MultiWindowActionGame.Player
{
    public interface IPlayerPhysics
    {
        bool IsGrounded { get; }
        float VerticalVelocity { get; }
        Action<int>? OnGrounded { get; set; }

        Vector2 CalculateMovement(float deltaTime);
        void ApplyGravity(float deltaTime);
        void Jump();
        void CheckGrounded(Rectangle bounds, GameWindow? parentWindow, float deltaTime);
        void ResetPhysics();
        void SetGrounded(bool grounded, int? groundY = null);
        void SetVerticalVelocity(float velocity);

        Rectangle GetGroundCheckArea(Rectangle bounds);

        /// <summary>
        /// 横方向の移動に対するスイープ衝突判定
        /// プレイヤーが細い場合に不可侵ウィンドウを貫通するのを防ぐ
        /// </summary>
        Vector2 CheckHorizontalCollision(Rectangle bounds, Vector2 movement);
    }
}