using MultiWindowActionGame;
using MultiWindowActionGame.Core;
using System.Runtime.InteropServices;

namespace MultiWindowActionGame.Windows
{
    public class NoEntryZone : Form
{
    private const int GWL_EXSTYLE = -20;
    private const int WS_EX_LAYERED = 0x80000;
    private const int WS_EX_TRANSPARENT = 0x20;
    private const int WS_EX_TOPMOST = 0x8;
    private static readonly IntPtr HWND_TOPMOST = new IntPtr(-1);
    private const uint SWP_NOMOVE = 0x0002;
    private const uint SWP_NOSIZE = 0x0001;

    [DllImport("user32.dll")]
    private static extern int SetWindowLong(IntPtr hWnd, int nIndex, int dwNewLong);

    [DllImport("user32.dll")]
    private static extern int GetWindowLong(IntPtr hWnd, int nIndex);

    [DllImport("user32.dll")]
    private static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);

    private Rectangle bounds;
    private readonly System.Windows.Forms.Timer animationTimer;
    private int animationOffset = 0;
    private const int STRIPE_WIDTH = 20;  // 縞模様の幅
    private const int TOTAL_PATTERN_HEIGHT = STRIPE_WIDTH * 2;// 赤と黒の組み合わせの高さ

    public new Rectangle Bounds => bounds;
    public new GameWindow? Parent { get; private set; }
    public ICollection<IEffectTarget> Children { get; } = new HashSet<IEffectTarget>();
    public bool IsMinimized { get; private set; }

    public NoEntryZone(Point location, Size size)
    {
        bounds = new Rectangle(location, size);
        this.MinimumSize = new Size(10, 10);
        InitializeZone();

        // アニメーション用タイマー設定
        animationTimer = new System.Windows.Forms.Timer();
        animationTimer.Interval = 50;  // 20fps
        animationTimer.Tick += AnimationTimer_Tick;
        animationTimer.Start();
    }

    private void InitializeZone()
    {
        this.FormBorderStyle = FormBorderStyle.None;
        this.StartPosition = FormStartPosition.Manual;
        this.Location = bounds.Location;
        this.Size = bounds.Size;
        this.ShowInTaskbar = false;
        this.Load += NoEntryZone_Load;
        this.Paint += NoEntryZone_Paint;
        this.BackColor = Color.Magenta;
        this.TransparencyKey = Color.Magenta;
        // ダブルバッファリングを有効にする
        this.SetStyle(
            ControlStyles.OptimizedDoubleBuffer |
            ControlStyles.AllPaintingInWmPaint |
            ControlStyles.UserPaint,
            true
        );
    }

    private void NoEntryZone_Load(object? sender, EventArgs e)
    {
        SetWindowProperties();
    }

    private void SetWindowProperties()
    {
        int exStyle = GetWindowLong(this.Handle, GWL_EXSTYLE);
        exStyle |= WS_EX_LAYERED;
        exStyle |= WS_EX_TRANSPARENT;
        exStyle |= WS_EX_TOPMOST;
        SetWindowLong(this.Handle, GWL_EXSTYLE, exStyle);
        SetWindowPos(this.Handle, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }

    private void AnimationTimer_Tick(object? sender, EventArgs e)
    {
        animationOffset = (animationOffset + 2) % TOTAL_PATTERN_HEIGHT;
        this.Invalidate();
    }

    private void NoEntryZone_Paint(object? sender, PaintEventArgs e)
    {
        // アンチエイリアシングを有効にする
        e.Graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;

        using (Brush redBrush = new SolidBrush(Color.FromArgb(180, Color.Red)))
        using (Brush blackBrush = new SolidBrush(Color.FromArgb(180, Color.Black)))
        using (BufferedGraphics buffered = BufferedGraphicsManager.Current.Allocate(
            e.Graphics, this.ClientRectangle))
        {
            Graphics g = buffered.Graphics;
            g.Clear(Color.Transparent);

            // パターンの開始位置を調整（ウィンドウの高さを考慮）
            int startY = -(animationOffset % TOTAL_PATTERN_HEIGHT);
            // 画面外から開始して、画面外まで描画
            for (int y = startY; y < this.Height + TOTAL_PATTERN_HEIGHT; y += TOTAL_PATTERN_HEIGHT)
            {
                g.FillRectangle(redBrush, 0, y, this.Width, STRIPE_WIDTH);
                g.FillRectangle(blackBrush, 0, y + STRIPE_WIDTH, this.Width, STRIPE_WIDTH);
            }

            buffered.Render();
        }
    }
    // IEffectTarget の実装
    public void UpdateTargetPosition(Point newPosition)
    {
        bounds.Location = newPosition;
        this.Location = newPosition;
    }

    public void UpdateTargetSize(Size newSize)
    {
        bounds.Size = newSize;
        this.Size = newSize;
    }

    public void AddChild(IEffectTarget child) => Children.Add(child);
    public void RemoveChild(IEffectTarget child) => Children.Remove(child);
    public bool CanReceivedEffect(IWindowEffect effect) => false;
    public void ApplyEffect(IWindowEffect effect) { }
    public void OnMinimize() { }
    public void OnRestore() { }

    public Task UpdateAsync(float deltaTime)
    {
        // 必要に応じて更新ロジックを実装
        return Task.CompletedTask;
    }

    public void Draw(Graphics g)
    {
        if (MainGame.IsDebugMode)
        {
            // デバッグ情報の表示
            g.DrawRectangle(new Pen(Color.Yellow, 2), bounds);
        }
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing)
        {
            animationTimer.Stop();
            animationTimer.Dispose();
        }
        base.Dispose(disposing);
    }
    protected override CreateParams CreateParams
    {
        get
        {
            CreateParams cp = base.CreateParams;
            cp.ExStyle |= 0x00080000; // WS_EX_LAYERED
            cp.ExStyle |= 0x00000020; // WS_EX_TRANSPARENT
            return cp;
        }
    }
}
}