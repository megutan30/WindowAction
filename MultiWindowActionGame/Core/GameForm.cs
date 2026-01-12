using System;
using System.Drawing;
using System.Threading.Tasks;
using System.Windows.Forms;
using MultiWindowActionGame.Utilities;

namespace MultiWindowActionGame.Core
{
    /// <summary>
    /// ゲームのメインフォーム（Shell通知受信機能付き）
    /// </summary>
    public class GameForm : Form
    {
        private const uint WM_SHNOTIFY = 0x0401;

        public GameForm()
        {
            // Program.mainFormと同じ設定
            var screenBounds = Screen.PrimaryScreen.Bounds;
            FormBorderStyle = FormBorderStyle.None;
            WindowState = FormWindowState.Normal;
            Location = new Point(0, 0);
            Size = screenBounds.Size;
            TopMost = true;
            ShowInTaskbar = false;
            BackColor = Color.Black;
            TransparencyKey = Color.Black;
            Text = "Game";
        }

        protected override void WndProc(ref Message m)
        {
            if (m.Msg == WM_SHNOTIFY)
            {
                // Shell通知受信時に非同期でRefresh
                if (DesktopIconHelper.Current != null)
                {
                    Task.Run(() => DesktopIconHelper.Current.RefreshIconsAsync());
                }
            }

            base.WndProc(ref m);
        }
    }
}
