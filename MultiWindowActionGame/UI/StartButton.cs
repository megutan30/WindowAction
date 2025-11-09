using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using MultiWindowActionGame.Core;
using MultiWindowActionGame.Interfaces;

namespace MultiWindowActionGame.UI
{
    // StartButtonの実装
    public class StartButton : GameButton
    {
        private readonly IStageManager? stageManager;
        
        // DI対応コンストラクタ
        public StartButton(Point location, IStageManager? stageManager = null, IWindowManager? windowManager = null) 
            : base(location, new Size(150, 40), windowManager)
        {
            this.stageManager = stageManager;
        }
        
        // ヘルパーメソッド：DIとLegacyの両方をサポート
        private IStageManager GetStageManagerSafely()
        {
            return stageManager ?? StageManager.Current;
        }

        protected override void OnButtonClick()
        {
            GetStageManagerSafely().StartNextStage();  // 次のステージへ
        }

        protected override void DrawButtonContent(Graphics g)
        {
            using (Font font = new Font(CustomFonts.PressStart.FontFamily, 14, FontStyle.Bold))  // フォントサイズを少し大きく
            {
                string text = "Start";
                SizeF textSize = g.MeasureString(text, font);
                g.DrawString(text, font, Brushes.Black,
                    (Width - textSize.Width) / 2,
                    (Height - textSize.Height) / 2);
            }
        }
    }
}
