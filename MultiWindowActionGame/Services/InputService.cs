using System.Drawing;
using System.Runtime.InteropServices;
using System.Windows.Forms;
using MultiWindowActionGame.Interfaces;

namespace MultiWindowActionGame.Services
{
    public class InputService : IInputService
    {
        [DllImport("user32.dll")]
        private static extern short GetAsyncKeyState(Keys vKey);

        // 放置検出用のマウス位置追跡
        private Point lastMousePosition = Point.Empty;

        public bool IsKeyDown(Keys key)
        {
            return (GetAsyncKeyState(key) & 0x8000) != 0;
        }

        /// <summary>
        /// マウスが移動したかどうかを検出
        /// </summary>
        public bool HasMouseMoved()
        {
            Point currentPos = Cursor.Position;

            // 初回は現在位置を記録して終了
            if (lastMousePosition == Point.Empty)
            {
                lastMousePosition = currentPos;
                return false;
            }

            // 位置が変わったかチェック
            if (currentPos != lastMousePosition)
            {
                lastMousePosition = currentPos;
                return true;
            }

            return false;
        }

        /// <summary>
        /// ゲーム操作キーが押されているかどうかを検出
        /// </summary>
        public bool IsAnyGameKeyPressed()
        {
            // 移動キー（WASD、矢印キー）
            if (IsKeyDown(Keys.W) || IsKeyDown(Keys.A) || IsKeyDown(Keys.S) || IsKeyDown(Keys.D))
                return true;

            if (IsKeyDown(Keys.Up) || IsKeyDown(Keys.Down) || IsKeyDown(Keys.Left) || IsKeyDown(Keys.Right))
                return true;

            // ジャンプキー
            if (IsKeyDown(Keys.Space))
                return true;

            return false;
        }
    }
}