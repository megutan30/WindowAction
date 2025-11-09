using System;

namespace MultiWindowActionGame.Animation
{
    /// <summary>
    /// プレイヤーのアニメーションを管理するクラス
    /// </summary>
    public class PlayerAnimation
    {
        // スケール値
        private float scaleX = 1.0f;
        private float scaleY = 1.0f;
        private float targetScaleX = 1.0f;
        private float targetScaleY = 1.0f;

        // アニメーション設定
        private const float AnimationLerpSpeed = 0.25f;
        private const int AnimationDuration = 30; // フレーム数

        // ステート管理
        private PlayerAnimationState currentState = PlayerAnimationState.Idle;
        private int stateTimer = 0;
        private int globalTimer = 0;

        // プロパティ
        public float ScaleX => scaleX;
        public float ScaleY => scaleY;
        public PlayerAnimationState CurrentState => currentState;

        /// <summary>
        /// アニメーションステートを更新
        /// </summary>
        public void UpdateAnimationState(bool isGrounded, bool wasGrounded, float dx)
        {
            // 優先順位付きステート遷移ロジック

            // 1. 着地した瞬間
            if (isGrounded && !wasGrounded)
            {
                currentState = PlayerAnimationState.Landing;
                stateTimer = 0;
                return;
            }

            // 2. 着地アニメーション中
            if (currentState == PlayerAnimationState.Landing)
            {
                if (stateTimer >= AnimationDuration)
                {
                    // 着地アニメーション終了
                    stateTimer = 0;
                    currentState = (dx == 0) ? PlayerAnimationState.Idle : PlayerAnimationState.Running;
                }
                else if (dx != 0)
                {
                    // 着地アニメーション中に移動開始
                    currentState = PlayerAnimationState.Running;
                    stateTimer = 0;
                }
                return;
            }

            // 3. ジャンプアニメーション中
            if (currentState == PlayerAnimationState.Jumping)
            {
                if (isGrounded)
                {
                    // ジャンプ中に着地
                    currentState = PlayerAnimationState.Landing;
                    stateTimer = 0;
                }
                else if (stateTimer >= AnimationDuration)
                {
                    // ジャンプアニメーション完了後も空中
                    currentState = PlayerAnimationState.Falling;
                }
                return;
            }

            // 4. 地面にいて一時的なアニメーション中でない
            if (isGrounded)
            {
                if (dx == 0)
                {
                    currentState = PlayerAnimationState.Idle;
                }
                else
                {
                    currentState = PlayerAnimationState.Running;
                }
                return;
            }

            // 5. 空中にいて一時的なアニメーション中でない
            currentState = PlayerAnimationState.Jumping;
            stateTimer = 0;
        }

        /// <summary>
        /// ジャンプアニメーションを開始
        /// </summary>
        public void StartJump()
        {
            currentState = PlayerAnimationState.Jumping;
            stateTimer = 0;
        }

        /// <summary>
        /// アニメーションを更新
        /// </summary>
        /// <param name="deltaTime">デルタタイム</param>
        /// <param name="distanceToTop">移動可能領域の上端までの距離（制限なしの場合はfloat.MaxValue）</param>
        /// <param name="originalHeight">プレイヤーの元の高さ</param>
        public void UpdateAnimation(float deltaTime, float distanceToTop = float.MaxValue, float originalHeight = 60f)
        {
            // タイマー更新
            globalTimer++;
            if (currentState == PlayerAnimationState.Jumping || currentState == PlayerAnimationState.Landing)
            {
                stateTimer++;
            }

            // 現在のステートに基づいて目標スケールを計算
            float progress = (float)stateTimer / AnimationDuration;

            switch (currentState)
            {
                case PlayerAnimationState.Idle:
                    targetScaleY = 1.0f + (float)Math.Sin(globalTimer * 0.05f) * 0.05f;
                    targetScaleX = 1.0f / targetScaleY;
                    break;

                case PlayerAnimationState.Running:
                    targetScaleY = 1.0f + Math.Abs((float)Math.Sin(globalTimer * 0.25f)) * 0.2f;
                    targetScaleX = 1.0f / targetScaleY;
                    break;

                case PlayerAnimationState.Jumping:
                    targetScaleY = AnimationCurves.StretchAndBack(progress);

                    // 上端に近い場合はスケールを制限
                    float scaleLimit = CalculateScaleLimit(distanceToTop, originalHeight);
                    targetScaleY = Math.Min(targetScaleY, scaleLimit);

                    targetScaleX = 1.0f / targetScaleY;
                    break;

                case PlayerAnimationState.Falling:
                    // ジャンプステートを継続（伸びた状態維持）
                    targetScaleY = AnimationCurves.StretchAndBack(1.0f);

                    // 上端に近い場合はスケールを制限
                    scaleLimit = CalculateScaleLimit(distanceToTop, originalHeight);
                    targetScaleY = Math.Min(targetScaleY, scaleLimit);

                    targetScaleX = 1.0f / targetScaleY;
                    break;

                case PlayerAnimationState.Landing:
                    float landingProgress = AnimationCurves.EaseOutElastic(progress);
                    targetScaleY = 1.0f - 0.6f * (1.0f - landingProgress); // 40%まで潰れる
                    targetScaleX = 1.0f / targetScaleY;
                    break;
            }

            // Lerp補間で現在のスケールを目標スケールに近づける
            scaleX += (targetScaleX - scaleX) * AnimationLerpSpeed;
            scaleY += (targetScaleY - scaleY) * AnimationLerpSpeed;
        }

        /// <summary>
        /// 上端との距離に基づいてスケールの上限を計算
        /// </summary>
        private float CalculateScaleLimit(float distanceToTop, float originalHeight)
        {
            // 上端との距離がoriginalHeightの50%未満の場合に制限
            float threshold = originalHeight * 0.5f;

            if (distanceToTop >= threshold)
            {
                return float.MaxValue; // 制限なし
            }

            // 距離に応じて1.0〜1.5の範囲で制限（線形補間）
            float ratio = distanceToTop / threshold;
            return 1.0f + 0.5f * ratio;
        }

        /// <summary>
        /// アニメーション適用後の当たり判定領域を計算
        /// </summary>
        public Rectangle GetAnimatedCollisionBounds(Rectangle originalBounds)
        {
            // X方向は変更なし
            int x = originalBounds.X;
            int width = originalBounds.Width;

            // Y方向の高さをscaleYで調整
            int animatedHeight = (int)(originalBounds.Height * scaleY);

            // 足元固定で上端を調整
            int bottomY = originalBounds.Bottom;
            int topY = bottomY - animatedHeight;

            return new Rectangle(x, topY, width, animatedHeight);
        }

        /// <summary>
        /// スケールを強制的にリセット（天井接触時用）
        /// </summary>
        public void ResetScale()
        {
            scaleX = 1.0f;
            scaleY = 1.0f;
            targetScaleX = 1.0f;
            targetScaleY = 1.0f;
        }
    }
}
