using System.Drawing;
using MultiWindowActionGame.Collision;
using MultiWindowActionGame.Windows;

namespace MultiWindowActionGame.Interfaces
{
    /// <summary>
    /// 衝突判定サービスのインターフェース
    /// </summary>
    public interface ICollisionService
    {
        /// <summary>
        /// 指定された境界が衝突するかチェック
        /// </summary>
        bool CheckCollision(Rectangle bounds, CollisionOptions options);

        /// <summary>
        /// 移動可能な位置に調整した境界を返す
        /// </summary>
        Rectangle ValidatePosition(Rectangle currentBounds, Rectangle proposedBounds, CollisionOptions options);

        /// <summary>
        /// リサイズ可能なサイズに調整したサイズを返す
        /// </summary>
        Size ValidateSize(Rectangle currentBounds, Size proposedSize, CollisionOptions options);

        /// <summary>
        /// 親が不可侵ウィンドウの場合、境界を親の範囲内に制限
        /// </summary>
        Rectangle ConstrainToParentBoundary(GameWindow window, Rectangle proposedBounds);
    }
}
