using MultiWindowActionGame;
using MultiWindowActionGame.Effects;
using MultiWindowActionGame.Core;
using MultiWindowActionGame.Debug;
using System.Drawing.Drawing2D;
using System.Runtime.InteropServices;

namespace MultiWindowActionGame.Windows
{
    public class Goal : BaseEffectTarget
{
    private bool isInFront;
    private GameWindow? lastValidParent;
    private GoalDebugInfo? debugInfo;

    // 領域の定義
    private Rectangle collisionBounds;  // 当たり判定領域（ゴール判定用）
    private Rectangle renderBounds;     // 描画領域（"G"文字の描画範囲）
    private Rectangle displayBounds;    // 表示領域（Form領域）

    // 当たり判定領域を基準とした各領域の比率
    private const float RENDER_RATIO = 1.5f;   // 描画領域は当たり判定の1.5倍
    private const float DISPLAY_RATIO = 2.0f;  // 表示領域は当たり判定の2.0倍

    // 当たり判定領域を公開（読み取り専用）
    public Rectangle CollisionBounds => collisionBounds;
    // 描画領域を公開（読み取り専用）
    public Rectangle RenderBounds => renderBounds;
    // 表示領域を公開（読み取り専用）
    public Rectangle DisplayBounds => displayBounds;

    // Boundsプロパティをオーバーライドして当たり判定領域を返す
    public override Rectangle Bounds => collisionBounds;

    public Goal(Point location, bool isInFront)
    {
        this.isInFront = isInFront;

        // 当たり判定領域を初期化（64x64）
        collisionBounds = new Rectangle(location, new Size(64, 64));
        // 描画領域を当たり判定の1.5倍で計算
        renderBounds = CalculateRenderBounds(collisionBounds);
        // 表示領域を当たり判定の2.0倍で計算
        displayBounds = CalculateDisplayBounds(collisionBounds);
        // bounds（継承元のフィールド）に表示領域を設定
        bounds = displayBounds;

        InitializeForm();
        this.Load += Goal_Load;

        debugInfo = new GoalDebugInfo(this);

        WindowManager.Current.RegisterFormOrder(this,
            isInFront ? MultiWindowActionGame.Managers.ZOrderPriority.Goal : MultiWindowActionGame.Managers.ZOrderPriority.Bottom);

        var parentWindow = WindowManager.Current.GetTopWindowAt(collisionBounds, null);
        if (parentWindow != null)
        {
            SetParent(parentWindow);
        }
    }
    private void Goal_Load(object? sender, EventArgs e)
    {
        SetWindowProperties();
        WindowManager.Current.UpdateFormZOrder(this,
            isInFront ? MultiWindowActionGame.Managers.ZOrderPriority.Goal : MultiWindowActionGame.Managers.ZOrderPriority.Bottom);
    }
    private void SetWindowProperties()
    {
        int exStyle = WindowMessages.GetWindowLong(this.Handle, WindowMessages.GWL_EXSTYLE);
        exStyle |= WindowMessages.WS_EX_LAYERED;
        // WS_EX_TRANSPARENTを削除: サイズ変更時の黒いチラつき問題を解決
        // TransparencyKeyによる透明領域は自動的にマウスイベントを透過する
        WindowMessages.SetWindowLong(this.Handle, WindowMessages.GWL_EXSTYLE, exStyle);
    }

    private void InitializeForm()
    {
        this.FormBorderStyle = FormBorderStyle.None;
        this.StartPosition = FormStartPosition.Manual;
        this.Location = bounds.Location;
        this.Size = bounds.Size;
        this.TopMost = true;
        this.BackColor = Color.Magenta;
        this.TransparencyKey = Color.Magenta;

        this.SetStyle(
            ControlStyles.OptimizedDoubleBuffer |
            ControlStyles.AllPaintingInWmPaint |
            ControlStyles.UserPaint |
            ControlStyles.ResizeRedraw,
            true
        );

        this.Paint += Goal_Paint;
    }

    // 当たり判定領域から描画領域を計算（当たり判定の1.5倍）
    private Rectangle CalculateRenderBounds(Rectangle collision)
    {
        int renderWidth = (int)(collision.Width * RENDER_RATIO);
        int renderHeight = (int)(collision.Height * RENDER_RATIO);
        int offsetX = (renderWidth - collision.Width) / 2;
        int offsetY = (renderHeight - collision.Height) / 2;

        return new Rectangle(
            collision.X - offsetX,
            collision.Y - offsetY,
            renderWidth,
            renderHeight
        );
    }

    // 当たり判定領域から表示領域を計算（当たり判定の2.0倍）
    private Rectangle CalculateDisplayBounds(Rectangle collision)
    {
        int displayWidth = (int)(collision.Width * DISPLAY_RATIO);
        int displayHeight = (int)(collision.Height * DISPLAY_RATIO);
        int offsetX = (displayWidth - collision.Width) / 2;
        int offsetY = (displayHeight - collision.Height) / 2;

        return new Rectangle(
            collision.X - offsetX,
            collision.Y - offsetY,
            displayWidth,
            displayHeight
        );
    }
    public override void SetParent(GameWindow? newParent)
    {
        base.SetParent(newParent);

        // 親が変更されたら再描画を要求
        this.Invalidate();
    }
    private void Goal_Paint(object? sender, PaintEventArgs e)
    {
        e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;

        // フォーム全体を透明色で塗りつぶし（パディング部分を透明化）
        e.Graphics.Clear(Color.Magenta);

        // 描画領域のローカル座標を計算（Form座標系での位置）
        // Form.LocationはdisplayBounds.Locationと同じなので、displayBounds基準で計算
        Rectangle localRenderRect = new Rectangle(
            renderBounds.X - this.Location.X,
            renderBounds.Y - this.Location.Y,
            renderBounds.Width,
            renderBounds.Height
        );

        // 描画領域に基づいてフォントサイズを計算
        float baseFontSize = Math.Min(localRenderRect.Width, localRenderRect.Height) * 1f;
        using (var font = new Font("Arial", baseFontSize, FontStyle.Bold))
        {
            var text = "G";
            var size = e.Graphics.MeasureString(text, font);

            // 描画領域いっぱいに表示するためのスケーリング
            float scaleX = localRenderRect.Width / size.Width;
            float scaleY = localRenderRect.Height / size.Height;

            // 変換行列を設定（描画領域の中央に配置）
            e.Graphics.TranslateTransform(
                localRenderRect.X + localRenderRect.Width / 2,
                localRenderRect.Y + localRenderRect.Height / 2);
            e.Graphics.ScaleTransform(scaleX, scaleY);
            e.Graphics.TranslateTransform(-size.Width / 2, -size.Height / 2);

            // 親ウィンドウに基づいてアウトライン色を設定
            Color outlineColor;
            if (Parent != null)
            {
                // 親の背景色の明るさを計算
                float brightness = (Parent.BackColor.R * 0.299f +
                                  Parent.BackColor.G * 0.587f +
                                  Parent.BackColor.B * 0.114f) / 255f;

                if (brightness < 0.5f)
                {
                    // 暗い背景の場合は明るい色のアウトライン
                    outlineColor = Color.FromArgb(
                        Math.Min(255, Parent.BackColor.R + 100),
                        Math.Min(255, Parent.BackColor.G + 100),
                        Math.Min(255, Parent.BackColor.B + 100)
                    );
                }
                else
                {
                    // 明るい背景の場合は暗い色のアウトライン
                    outlineColor = Color.FromArgb(
                        Math.Max(0, Parent.BackColor.R - 50),
                        Math.Max(0, Parent.BackColor.G - 50),
                        Math.Max(0, Parent.BackColor.B - 50)
                    );
                }
            }
            else
            {
                outlineColor = Color.Black;
            }

            // アウトラインの太さを調整（スケールを考慮）
            float offset = baseFontSize * 0.04f / Math.Max(scaleX, scaleY);

            // アウトラインを描画（8方向）
            for (int x = -1; x <= 1; x++)
            {
                for (int y = -1; y <= 1; y++)
                {
                    if (x != 0 || y != 0)
                    {
                        e.Graphics.DrawString(text, font, new SolidBrush(outlineColor),
                            x * offset,
                            y * offset);
                    }
                }
            }

            // メインの文字を描画
            e.Graphics.DrawString(text, font, Brushes.Gold, 0, 0);

            // 変換をリセット
            e.Graphics.ResetTransform();
        }
    }
    private void UpdateParentIfNeeded()
    {
        // 当たり判定領域に変更があった場合のみチェック
        if (lastCheckedBounds != collisionBounds)
        {
            try
            {
                var potentialParent = WindowManager.Current.GetWindowFullyContaining(collisionBounds);
                if (potentialParent != Parent)
                {
                    SetParent(potentialParent);
                }
                lastCheckedBounds = collisionBounds;
            }
            catch (Exception ex)
            {
                // 親変更でエラーが発生した場合はログに記録し、処理を続行
                System.Diagnostics.Debug.WriteLine($"Goal: 親変更エラー - {ex.Message}");
                // エラー時も最後にチェックした境界を更新して無限ループを防ぐ
                lastCheckedBounds = collisionBounds;
            }
        }
    }
    private Rectangle lastCheckedBounds;
    public override Size GetOriginalSize() => collisionBounds.Size;
    public override void UpdateTargetPosition(Point newPosition)
    {
        collisionBounds.Location = newPosition;
        renderBounds = CalculateRenderBounds(collisionBounds);
        displayBounds = CalculateDisplayBounds(collisionBounds);
        bounds = displayBounds;
        this.Location = bounds.Location;
        UpdateParentIfNeeded();
    }

    public override void UpdateTargetSize(Size newSize)
    {
        // 最小サイズを設定
        var validSize = new Size(
            Math.Max(newSize.Width, 20),  // 最小幅20px
            Math.Max(newSize.Height, 20)  // 最小高さ20px
        );

        collisionBounds.Size = validSize;
        renderBounds = CalculateRenderBounds(collisionBounds);
        displayBounds = CalculateDisplayBounds(collisionBounds);
        bounds = displayBounds;
        this.Size = bounds.Size;
        UpdateParentIfNeeded();

        // サイズ変更後に即座に再描画
        this.Invalidate();
        this.Update();
    }

    public override void OnMinimize()
    {
        IsMinimized = true;
        this.WindowState = FormWindowState.Minimized;

        if (Parent != null)
        {
            lastValidParent = Parent;
            Parent.RemoveChild(this);
            Parent = null;
        }
    }
    public override void OnRestore()
    {
        IsMinimized = false;
        this.WindowState = FormWindowState.Normal;
        this.BringToFront();

        if (lastValidParent != null &&
            !lastValidParent.IsMinimized &&
            lastValidParent.CollisionBounds.IntersectsWith(collisionBounds))
        {
            SetParent(lastValidParent);
        }
        else
        {
            var newParent = WindowManager.Current.GetTopWindowAt(collisionBounds, null);
            SetParent(newParent);
        }
    }
    public override Task UpdateAsync(float deltaTime)
    {
        CheckParentWindow();
        return Task.CompletedTask;
    }

    private void CheckParentWindow()
    {
        // 当たり判定領域を完全に含むウィンドウを探す
        var potentialParent = WindowManager.Current.GetWindowFullyContaining(collisionBounds);
        if (potentialParent != Parent)
        {
            SetParent(potentialParent);
        }
    }
    public override void Draw(Graphics g)
    {
        if (MainGame.IsDebugMode)
        {
            debugInfo?.Draw(g);
        }
    }
    protected override void Dispose(bool disposing)
    {
        if (disposing)
        {
            WindowManager.Current.UnregisterFormOrder(this);
        }
        base.Dispose(disposing);
    }
}
}