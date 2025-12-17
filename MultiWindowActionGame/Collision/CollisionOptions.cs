using MultiWindowActionGame.Windows;

namespace MultiWindowActionGame.Collision
{
    /// <summary>
    /// 衝突判定のオプション設定
    /// </summary>
    public class CollisionOptions
    {
        /// <summary>
        /// 衝突判定から除外するウィンドウ（自分自身を除外する場合に使用）
        /// </summary>
        public GameWindow? ExcludeWindow { get; set; }

        /// <summary>
        /// ExcludeWindowの子孫ウィンドウも除外するか（親ウィンドウが子を持つ場合に使用）
        /// </summary>
        public bool ExcludeChildren { get; set; } = false;

        /// <summary>
        /// 静的NoEntryZoneとの衝突判定を行うか
        /// </summary>
        public bool CheckNoEntryZones { get; set; } = true;

        /// <summary>
        /// 不可侵ウィンドウの境界との衝突判定を行うか
        /// </summary>
        public bool CheckNoEntryBoundaries { get; set; } = true;

        /// <summary>
        /// 通常ウィンドウとの衝突判定を行うか（不可侵ウィンドウが移動する場合に使用）
        /// </summary>
        public bool CheckNormalWindows { get; set; } = false;

        /// <summary>
        /// ボタンとの衝突判定を行うか（不可侵ウィンドウが移動する場合に使用）
        /// </summary>
        public bool CheckButtons { get; set; } = false;

        /// <summary>
        /// プレイヤーとの衝突判定を行うか（不可侵ウィンドウが移動する場合に使用）
        /// </summary>
        public bool CheckPlayer { get; set; } = false;

        /// <summary>
        /// Z-orderを考慮したフィルタリングを行うか
        /// </summary>
        public bool UseZOrderFiltering { get; set; } = true;
    }
}
