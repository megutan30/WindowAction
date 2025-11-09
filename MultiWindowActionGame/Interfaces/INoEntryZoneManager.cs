using System.Collections.Generic;
using System.Drawing;
using MultiWindowActionGame.Windows;

namespace MultiWindowActionGame.Interfaces
{
    public interface INoEntryZoneManager
    {
        IReadOnlyList<NoEntryZone> Zones { get; }

        void AddZone(Point location, Size size);
        void RemoveZone(NoEntryZone zone);
        void ClearZones();
        bool IntersectsWithAnyZone(Rectangle bounds, GameWindow? excludeWindow = null);
        Rectangle GetValidPosition(Rectangle currentBounds, Rectangle proposedBounds, GameWindow? excludeWindow = null);
        Size GetValidSize(Rectangle currentBounds, Size proposedSize, GameWindow? excludeWindow = null);
        void Draw(Graphics g);

        // 不可侵ウィンドウ管理用メソッド
        void RegisterNoEntryWindow(GameWindow window);
        void UnregisterNoEntryWindow(GameWindow window);
        void UpdateNoEntryZonesForWindow(GameWindow window);
        void RemoveNoEntryZonesForWindow(GameWindow window);
        bool IsNoEntryWindowVisible(GameWindow window, Rectangle checkBounds);

        // 不可侵ウィンドウ境界判定メソッド（Z-order + Region考慮）
        /// <summary>
        /// 不可侵ウィンドウの4辺境界Rectangleを取得
        /// </summary>
        /// <param name="window">対象の不可侵ウィンドウ</param>
        /// <returns>4つのRectangle（Top, Bottom, Left, Right）のリスト</returns>
        List<Rectangle> GetNoEntryBoundaryRectangles(GameWindow window);

        /// <summary>
        /// 単一の不可侵ウィンドウとの境界衝突をZ-order + Region考慮で判定
        /// </summary>
        /// <param name="window">判定対象の不可侵ウィンドウ</param>
        /// <param name="checkBounds">衝突判定する矩形</param>
        /// <param name="collisionRect">実際に衝突した境界Rectangle</param>
        /// <returns>衝突があればtrue</returns>
        bool CheckNoEntryBoundaryCollision(GameWindow window, Rectangle checkBounds, out Rectangle? collisionRect);

        /// <summary>
        /// すべての不可侵ウィンドウとの境界衝突をチェック
        /// </summary>
        /// <param name="checkBounds">衝突判定する矩形</param>
        /// <param name="collidingWindow">衝突した不可侵ウィンドウ</param>
        /// <param name="collisionRect">実際に衝突した境界Rectangle</param>
        /// <param name="excludeWindow">除外するウィンドウ（自分自身を除外する場合に使用）</param>
        /// <param name="excludeChildren">excludeWindowの子孫ウィンドウも除外するか</param>
        /// <returns>衝突があればtrue</returns>
        bool CheckAnyNoEntryBoundaryCollision(Rectangle checkBounds, out GameWindow? collidingWindow, out Rectangle? collisionRect, GameWindow? excludeWindow = null, bool excludeChildren = false);
    }
}