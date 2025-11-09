using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using MultiWindowActionGame.Core;
using MultiWindowActionGame.Debug;
using MultiWindowActionGame.Managers;

namespace MultiWindowActionGame.UI
{
    public class OverlayForm : Form
    {
        private readonly WindowManager windowManager;
        private PlayerDebugInfo? playerDebugInfo;
        private WindowDebugInfo? windowDebugInfo;
        private PerformanceDebugInfo performanceDebugInfo;
        private DesktopIconDebugInfo? desktopIconDebugInfo;
        private readonly IDesktopIconManager? desktopIconManager;

        public OverlayForm(WindowManager windowManager, IDesktopIconManager? desktopIconManager = null)
        {
            this.windowManager = windowManager ?? throw new ArgumentNullException(nameof(windowManager));
            this.desktopIconManager = desktopIconManager;

            this.FormBorderStyle = FormBorderStyle.None;
            this.ShowInTaskbar = false;
            this.TopMost = true;
            this.BackColor = Color.Magenta;
            this.TransparencyKey = Color.Magenta;

            this.SetStyle(
                ControlStyles.OptimizedDoubleBuffer |
                ControlStyles.AllPaintingInWmPaint |
                ControlStyles.UserPaint,
                true
            );

            // スクリーン全体を覆うように設定（mainFormの状態に関係なく）
            var screenBounds = Screen.PrimaryScreen.Bounds;
            this.Location = new Point(0, 0);
            this.Size = screenBounds.Size;
            
            if (Program.mainForm != null)
            {
                // デバッグ情報をログ出力
            }

            // インスタンスを必ず初期化
            this.windowDebugInfo = new WindowDebugInfo(windowManager);
            this.performanceDebugInfo = new PerformanceDebugInfo();
            
            // DesktopIconManagerが利用可能な場合のみ初期化
            if (this.desktopIconManager != null)
            {
                this.desktopIconDebugInfo = new DesktopIconDebugInfo(this.desktopIconManager);
            }

            this.Paint += OverlayForm_Paint;
        }
        private void OverlayForm_Paint(object? sender, PaintEventArgs e)
        {
            windowManager.DrawMarks(e.Graphics);
            if (!MainGame.IsDebugMode) return;

            var player = MainGame.GetPlayer();
            if (player != null)
            {
                playerDebugInfo ??= new PlayerDebugInfo(player);
                playerDebugInfo.Draw(e.Graphics);
            }
            windowDebugInfo?.Draw(e.Graphics);
            performanceDebugInfo.Draw(e.Graphics);
            
            // デスクトップアイコン情報を描画
            desktopIconDebugInfo?.Draw(e.Graphics);
        }

        public void UpdateOverlay()
        {
            this.Invalidate();
        }
    }
}
