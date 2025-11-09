namespace MultiWindowActionGame.Animation
{
    /// <summary>
    /// プレイヤーのアニメーションステート
    /// </summary>
    public enum PlayerAnimationState
    {
        /// <summary>待機</summary>
        Idle,

        /// <summary>移動</summary>
        Running,

        /// <summary>ジャンプ</summary>
        Jumping,

        /// <summary>落下</summary>
        Falling,

        /// <summary>着地</summary>
        Landing
    }
}
