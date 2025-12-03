// ボタンの基底クラス
using MultiWindowActionGame;
using MultiWindowActionGame.Effects;
using MultiWindowActionGame.Windows;
using System.Runtime.InteropServices;
using MultiWindowActionGame.Core;
using MultiWindowActionGame.Interfaces;

namespace MultiWindowActionGame.UI
{
    public abstract class GameButton : BaseEffectTarget
{
    protected bool isHovered;
    private GameWindow? lastValidParent;
    private readonly IWindowManager? windowManager;
    
    /// <summary>
    /// フォームの実際の位置とサイズを返す境界
    /// </summary>
    public override Rectangle Bounds => new Rectangle(this.Location, this.Size);
    
    /// <summary>
    /// プレイヤーとの当たり判定に使用する境界
    /// フォームの実際の位置とサイズを使用
    /// </summary>
    public Rectangle CollisionBounds => Bounds;
    
    // DI対応コンストラクタ
    protected GameButton(Point location, Size size, IWindowManager? windowManager = null)
    {
        this.windowManager = windowManager;
        bounds = new Rectangle(location, size);
        InitializeButton();
        (windowManager ?? WindowManager.Current).RegisterFormOrder(this, MultiWindowActionGame.Managers.ZOrderPriority.Button);
    }
    
    private void GameButton_Load(object? sender, EventArgs e)
    {
        SetWindowProperties();
    }
    private void SetWindowProperties()
    {
        int exStyle = WindowMessages.GetWindowLong(this.Handle, WindowMessages.GWL_EXSTYLE);
        exStyle |= WindowMessages.WS_EX_TRANSPARENT;
        exStyle |= WindowMessages.WS_EX_TOPMOST;
        WindowMessages.SetWindowLong(this.Handle, WindowMessages.GWL_EXSTYLE, exStyle);
        WindowMessages.SetWindowPos(this.Handle, WindowMessages.HWND_TOPMOST, 0, 0, 0, 0, WindowMessages.SWP_NOMOVE | WindowMessages.SWP_NOSIZE);
    }
    private void InitializeButton()
    {
        this.FormBorderStyle = FormBorderStyle.None;
        this.StartPosition = FormStartPosition.Manual;
        this.Location = bounds.Location;
        this.Size = bounds.Size;
        this.ShowInTaskbar = true;

        // マウスイベントの設定
        this.MouseEnter += (s, e) => { isHovered = true; this.Invalidate(); };
        this.MouseLeave += (s, e) => { isHovered = false; this.Invalidate(); };
        this.MouseClick += OnMouseClick;

        this.Paint += Button_Paint;
    }

    protected abstract void OnButtonClick();
    protected abstract void DrawButtonContent(Graphics g);

    private void OnMouseClick(object? sender, MouseEventArgs e)
    {
        if (e.Button == MouseButtons.Left)
        {
            OnButtonClick();
        }
    }
    private void Button_Paint(object? sender, PaintEventArgs e)
    {
        e.Graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;

        // ボタンの背景
        using (SolidBrush brush = new SolidBrush(
            isHovered ? Color.FromArgb(230, 230, 230): Color.FromArgb(200, 200, 200)))
        {
            e.Graphics.FillRectangle(brush, ClientRectangle);
        }

        // ボタンの枠
        using (Pen pen = new Pen(Color.FromArgb(100, 100, 100), 2))
        {
            e.Graphics.DrawRectangle(pen, 1, 1, Width - 2, Height - 2);
        }

        DrawButtonContent(e.Graphics);
    }

    private Rectangle lastCheckedBounds;
    public override void UpdateTargetPosition(Point newPosition)
    {
        this.Location = newPosition;
        bounds.Location = newPosition;
        UpdateParentIfNeeded();
    }

    public override void UpdateTargetSize(Size newSize)
    {
        // 最小サイズを設定
        var validSize = new Size(
            Math.Max(newSize.Width, 150),  // 最小幅20px
            Math.Max(newSize.Height, 40)  // 最小高さ20px
        );

        this.Size = validSize;
        bounds.Size = validSize;
        UpdateParentIfNeeded();

        this.Invalidate();
    }
    public override bool CanReceiveEffect(IWindowEffect effect)
    {
        if (Parent == null) return false;
        return true;
    }

    private void UpdateParentIfNeeded()
    {
        // 自身の位置とサイズに変更があった場合のみチェック
        if (lastCheckedBounds != Bounds)
        {
            // リサイズ処理中は親変更を延期してフリーズを防止
            if (IsAnyWindowResizing())
            {
                // リサイズ完了後に親変更を再チェックするためのフラグ
                Task.Run(async () =>
                {
                    await Task.Delay(100); // リサイズ処理の完了を待つ
                    if (InvokeRequired)
                    {
                        Invoke(new Action(UpdateParentIfNeeded));
                    }
                    else
                    {
                        UpdateParentIfNeeded();
                    }
                });
                return;
            }

            try
            {
                var potentialParent = (windowManager ?? WindowManager.Current).GetWindowFullyContaining(Bounds);
                if (potentialParent != Parent)
                {
                    SetParent(potentialParent);
                }
                lastCheckedBounds = Bounds;
            }
            catch (Exception ex)
            {
                // 親変更でエラーが発生した場合はログに記録し、処理を続行
                System.Diagnostics.Debug.WriteLine($"GameButton: 親変更エラー - {ex.Message}");
            }
        }
    }

    private bool IsAnyWindowResizing()
    {
        try
        {
            var allWindows = (windowManager ?? WindowManager.Current).GetAllWindows();
            return allWindows.Any(w => w.IsResizable() && IsWindowCurrentlyResizing(w));
        }
        catch
        {
            return false; // エラー時は安全側に倒してfalseを返す
        }
    }

    private bool IsWindowCurrentlyResizing(GameWindow window)
    {
        // WindowStrategiesのResizableWindowStrategyがアクティブかチェック
        // 実装は保守的にマウスキャプチャ状態で判定
        return window.Capture;
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
            lastValidParent.CollisionBounds.IntersectsWith(Bounds))
        {
            SetParent(lastValidParent);
        }
        else
        {
            var newParent = (windowManager ?? WindowManager.Current).GetTopWindowAt(Bounds, null);
            SetParent(newParent);
        }
    }
    public override void ApplyEffect(IWindowEffect effect)
    {
        if (!CanReceiveEffect(effect)) return;

        if (effect is MovementEffect moveEffect)
        {
            var newPos = new Point(
                Bounds.X + (int)moveEffect.CurrentMovement.X,
                Bounds.Y + (int)moveEffect.CurrentMovement.Y
            );
            UpdateTargetPosition(newPos);
        }
        else if (effect is ResizeEffect resizeEffect)
        {
            var scale = resizeEffect.GetCurrentScale(this);
            var newSize = new Size(
                (int)(Bounds.Width * scale.Width),
                (int)(Bounds.Height * scale.Height)
            );
            UpdateTargetSize(newSize);
        }
    }
    public override Task UpdateAsync(float deltaTime)
    {
        CheckParentWindow();
        return Task.CompletedTask;
    }
    private void CheckParentWindow()
    {
        // 自身の領域を完全に含むウィンドウを探す
        var potentialParent = (windowManager ?? WindowManager.Current).GetWindowFullyContaining(Bounds);
        if (potentialParent != Parent)
        {
            SetParent(potentialParent);
        }
    }
    public override void Draw(Graphics g) { }
    public override Size GetOriginalSize() => Bounds.Size;
    protected override void Dispose(bool disposing)
    {
        if (disposing)
        {
            // フォームが破棄される前にWindowManagerから登録解除
            if (!IsDisposed)
            {
                (windowManager ?? WindowManager.Current).UnregisterFormOrder(this);
            }
        }
        base.Dispose(disposing);
    }
}
}
