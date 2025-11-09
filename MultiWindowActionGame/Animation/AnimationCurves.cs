using System;

namespace MultiWindowActionGame.Animation
{
    /// <summary>
    /// アニメーション用のイージング関数を提供するクラス
    /// </summary>
    public static class AnimationCurves
    {
        /// <summary>
        /// ジャンプの伸び縮みに使うカーブ
        /// 1 → 1.5 → 1 の曲線を返す
        /// </summary>
        /// <param name="x">進行度 (0-1)</param>
        /// <returns>スケール値</returns>
        public static float StretchAndBack(float x)
        {
            // エッジケース処理
            if (x <= 0) return 1.0f;
            if (x >= 1) return 1.0f;

            return 1.0f + (float)Math.Sin(x * Math.PI) * 0.5f;
        }

        /// <summary>
        /// 着地の弾性アニメーションに使うカーブ
        /// easeOutElastic - 弾性のある減衰曲線
        /// https://easings.net/#easeOutElastic
        /// </summary>
        /// <param name="x">進行度 (0-1)</param>
        /// <returns>スケール値</returns>
        public static float EaseOutElastic(float x)
        {
            // エッジケース処理
            if (x <= 0) return 0.0f;
            if (x >= 1) return 1.0f;

            const float c4 = (2.0f * (float)Math.PI) / 3.0f;

            return (float)(Math.Pow(2, -10 * x) * Math.Sin((x * 10 - 0.75) * c4) + 1);
        }
    }
}
