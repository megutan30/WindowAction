using System;
using System.Drawing;
using System.Windows.Forms;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Diagnostics;
using MultiWindowActionGame.Effects;
using MultiWindowActionGame.Extensions;
using MultiWindowActionGame.Player;
using MultiWindowActionGame.Utilities;
using MultiWindowActionGame.Core;
using MultiWindowActionGame.Interfaces;

namespace MultiWindowActionGame.Windows
{
    public class GameWindow : BaseEffectTarget, IWindowSubject
    {
        private readonly IWindowManager? windowManager;
        private readonly IGameSettings? gameSettings;
        public Rectangle ClientBounds { get; private set; }
        public Rectangle AdjustedBounds { get; private set; }
        public bool CanEnter { get; set; } = true;
        public bool CanExit { get; set; } = true;
        public Size OriginalSize { get; private set; }
        public IWindowStrategy Strategy { get; private set; }
        public bool IsNoEntryWindow { get; set; } = false;
        public override Rectangle Bounds => AdjustedBounds;
        public Rectangle FullBounds => new Rectangle(Location, Size);
        public bool IsInitializing { get; private set; } = true;
        private new const int Margin = 0;
        protected IWindowStrategy strategy;
        private List<IWindowObserver> observers = new List<IWindowObserver>();
        private readonly List<IWindowEffect> effects = new();
        public Guid Id { get; } = Guid.NewGuid();
        public event EventHandler<EventArgs> WindowMoved = delegate { };
        public event EventHandler<SizeChangedEventArgs> WindowResized = delegate { };
        public bool HasActiveEffects => effects.Any(e => e.IsActive);
        private TaskCompletionSource<bool> initializationTcs = new TaskCompletionSource<bool>();
        public Task InitializationTask => initializationTcs.Task;
        public Rectangle CollisionBounds => new(
            AdjustedBounds.X,
            Location.Y,
            AdjustedBounds.Size.Width,
            AdjustedBounds.Size.Height + (RectangleToScreen(ClientRectangle).Y - Location.Y)
        );

        protected override void OnMouseMove(MouseEventArgs e)
        {
            base.OnMouseMove(e);
            Strategy.UpdateCursor(this, e.Location);
        }
        public GameWindow(Point location, Size size, IWindowStrategy strategy, bool showImmediately = true)
            : this(location, size, strategy, null, showImmediately)
        {
        }

        public GameWindow(Point location, Size size, IWindowStrategy strategy, IWindowManager? windowManager, bool showImmediately = true)
            : this(location, size, strategy, windowManager, null, showImmediately)
        {
        }

        public GameWindow(Point location, Size size, IWindowStrategy strategy, IWindowManager? windowManager, IGameSettings? gameSettings, bool showImmediately = true)
        {
            this.windowManager = windowManager;
            this.gameSettings = gameSettings;
            IsInitializing = true;
            this.strategy = strategy;
            this.Strategy = strategy;
            this.OriginalSize = size;
            this.MinimumSize = (gameSettings ?? GameSettings.Current).Window.MinimumSize;

            InitializeWindow(location, size);
            InitializeEvents();

            (windowManager ?? WindowManager.Current).RegisterFormOrder(this, MultiWindowActionGame.Managers.ZOrderPriority.Window);


            if (showImmediately)
            {
                this.Show();
            }
            else
            {
                // showImmediately: falseの場合はHandleCreatedイベントを待たずに初期化完了をマーク
                if (!initializationTcs.Task.IsCompleted)
                {
                    initializationTcs.SetResult(true);
                }
            }

            IsInitializing = false;
        }
        private void InitializeWindow(Point location, Size size)
        {
            this.FormBorderStyle = FormBorderStyle.FixedSingle;
            this.StartPosition = FormStartPosition.Manual;
            this.Location = location;
            this.Size = size;
            this.TopMost = true;
            this.ControlBox = true;
            this.MaximizeBox = false;
            this.MinimizeBox = false;
        }
        private void InitializeEvents()
        {
            this.Load += GameWindow_Load;
            this.Move += GameWindow_Move;
            this.Resize += GameWindow_Resize;
            UpdateBounds();
        }

        #region IEffectTarget Implementation
        public IEnumerable<IWindowEffect> GetActiveEffects()
        {
            return effects.Where(e => e.IsActive).ToList();
        }
        public override Size GetOriginalSize()
        {
            return CollisionBounds.Size;
        }
        public override void SetParent(GameWindow? newParent)
        {
            if (base.Parent != null)
            {
                base.Parent.RemoveChild(this);
            }
            base.Parent = newParent;
            newParent?.AddChild(this);
        }
        public override void OnMinimize()
        {
            IsMinimized = true;

            // 不可侵ウィンドウの場合、不可侵領域を削除
            if (IsNoEntryWindow)
            {
                var noEntryZoneManager = NoEntryZoneManager.Current;
                noEntryZoneManager.RemoveNoEntryZonesForWindow(this);
            }

            foreach (var child in Children.ToList())
            {
                child.OnMinimize();
                RemoveChild(child);
            }

            if (base.Parent != null)
            {
                base.Parent.RemoveChild(this);
            }

            // アニメーション表示のために一時的にTopMostを設定
            bool wasTopMost = this.TopMost;
            this.TopMost = true;
            this.BringToFront();

            // Win32 APIを使用した確実な最小化実行
            WindowMessages.ShowWindow(this.Handle, WindowMessages.SW_SHOWMINIMIZED);
            WindowState = FormWindowState.Minimized;

            // アニメーション後にTopMostを元に戻す（非同期で実行）
            Task.Run(async () =>
            {
                await Task.Delay(300); // アニメーション時間を待機
                if (!this.IsDisposed && this.WindowState == FormWindowState.Minimized)
                {
                    this.BeginInvoke(new Action(() =>
                    {
                        if (!this.IsDisposed)
                        {
                            this.TopMost = wasTopMost;
                        }
                    }));
                }
            });
        }
        public override void OnRestore()
        {
            IsMinimized = false;

            // Win32 APIを使用した確実な復元
            WindowMessages.ShowWindow(this.Handle, WindowMessages.SW_RESTORE);
            WindowState = FormWindowState.Normal;
            Show();

            // 親子判定はOSの復元処理後に実行されるため、ここでは呼ばない

            // 不可侵ウィンドウの場合、不可侵領域を再作成
            if (IsNoEntryWindow)
            {
                var noEntryZoneManager = NoEntryZoneManager.Current;
                noEntryZoneManager.UpdateNoEntryZonesForWindow(this);
            }
        }
        public override void AddChild(IEffectTarget child)
        {
            Children.Add(child);
            if (child is GameWindow window && window.Parent != this)
            {
                window.SetParent(this);
            }
        }
        public override void RemoveChild(IEffectTarget child)
        {
            base.RemoveChild(child);
            if (child is GameWindow window)
            {
                window.Parent = null;
            }
        }
        public override void UpdateTargetSize(Size newSize)
        {
            // フォームの実際のサイズを、CollisionBoundsのサイズと同じになるように調整
            Rectangle currentCollision = this.CollisionBounds;
            Rectangle currentForm = new Rectangle(this.Location, this.Size);

            // フォームの装飾部分のサイズを計算
            int widthDiff = currentForm.Width - currentCollision.Width;
            int heightDiff = currentForm.Height - currentCollision.Height;

            this.Size = new Size(
                newSize.Width + widthDiff,
                newSize.Height + heightDiff
            );
        }
        public override void UpdateTargetPosition(Point newPosition)
        {
            this.Location = newPosition;
        }
        public override bool CanReceiveEffect(IWindowEffect effect)
        {
            // 親からのエフェクトは常に受け入れる
            if (Parent != null)
            {
                return true;
            }

            // 自身が直接エフェクトを受け取る場合のチェック
            switch (effect.Type)
            {
                case EffectType.Movement:
                    return Strategy is MovableWindowStrategy;
                case EffectType.Resize:
                    return Strategy is ResizableWindowStrategy;
                case EffectType.Minimize:
                    return Strategy is MinimizableWindowStrategy;
                default:
                    return false;
            }
        }
        protected override void OnHandleCreated(EventArgs e)
        {
            base.OnHandleCreated(e);
            if (!initializationTcs.Task.IsCompleted)
            {
                initializationTcs.SetResult(true);
            }
        }
        protected override void OnHandleDestroyed(EventArgs e)
        {
            WindowEffectManager.Current.ClearEffects();
            base.OnHandleDestroyed(e);
        }
        public override void ApplyEffect(IWindowEffect effect)
        {
            if (!CanReceiveEffect(effect)) return;
            var effectManager = WindowEffectManager.Current;
            effectManager.AddEffect(effect);
            effectManager.ApplyEffects(this);
        }
        public bool IsChildOf(GameWindow potentialParent)
        {
            var current = this.Parent;
            while (current != null)
            {
                if (current == potentialParent) return true;
                current = current.Parent;
            }
            return false;
        }
        public IEnumerable<GameWindow> GetAllDescendants()
        {
            var descendants = new List<GameWindow>();
            foreach (var child in Children.OfType<GameWindow>())
            {
                descendants.Add(child);
                descendants.AddRange(child.GetAllDescendants());
            }
            return descendants;
        }
        #endregion

        #region IUpdatable and IDrawable Implementation
        public override Task UpdateAsync(float deltaTime)
        {
            strategy.Update(this, deltaTime);
            strategy.HandleInput(this);
            UpdateBounds();
            return Task.CompletedTask;
        }
        public override void Draw(Graphics g)
        {

        }
        #endregion

        #region Window Event Handlers
        private void GameWindow_Load(object? sender, EventArgs e)
        {
            IntPtr hMenu = WindowMessages.GetSystemMenu(this.Handle, false);
            WindowMessages.EnableMenuItem(hMenu, WindowMessages.SC_CLOSE, WindowMessages.MF_BYCOMMAND | WindowMessages.MF_GRAYED);
        }
        private void GameWindow_Move(object? sender, EventArgs e)
        {
            UpdateBounds();
            WindowMoved?.Invoke(this, EventArgs.Empty);
            NotifyObservers(WindowChangeType.Moved);
        }
        private void GameWindow_Activated(object? sender, EventArgs e)
        {
            // 最小化状態からの復元ではない通常のアクティブ化の場合
            if (!IsMinimized && WindowState != FormWindowState.Minimized)
            {
                //new WindowManager().HandleWindowActivation(this);
                (windowManager ?? WindowManager.Current).CheckPotentialParentWindow(this);
            }
        }
        private void GameWindow_Resize(object? sender, EventArgs e)
        {
            UpdateBounds();
            WindowResized?.Invoke(this, new SizeChangedEventArgs(this.Size));
            NotifyObservers(WindowChangeType.Resized);
            strategy.HandleResize(this);
        }
        private new void UpdateBounds()
        {
            Rectangle clientRect = GetClientRectangle();
            ClientBounds = clientRect;
            AdjustedBounds = new Rectangle(
                clientRect.X + Margin,
                clientRect.Y + Margin,
                clientRect.Width - (2 * Margin),
                clientRect.Height - (2 * Margin)
            );
        }
        public Point Center()
        {
            return new Point(
                Bounds.X + Bounds.Width / 2,
                Bounds.Y + Bounds.Height / 2
            );
        }
        private Rectangle GetClientRectangle()
        {
            WindowMessages.RECT rect;
            WindowMessages.GetClientRect(this.Handle, out rect);
            WindowMessages.POINT point = new WindowMessages.POINT { X = rect.Left, Y = rect.Top };
            WindowMessages.ClientToScreen(this.Handle, ref point);
            return new Rectangle(point.X, point.Y, rect.Right - rect.Left, rect.Bottom - rect.Top);
        }
        #endregion

        #region Observer Pattern Implementation
        public void AddObserver(IWindowObserver observer) => observers.Add(observer);
        public void RemoveObserver(IWindowObserver observer) => observers.Remove(observer);
        public void NotifyObservers(WindowChangeType changeType)
        {
            foreach (var observer in observers)
            {
                observer.OnWindowChanged(this, changeType);
            }
        }
        #endregion

        #region Window Message Processing
        protected override void WndProc(ref Message m)
        {
            switch (m.Msg)
            {
                case WindowMessages.WM_NCHITTEST:
                    base.WndProc(ref m);
                    if (m.Result.ToInt32() == WindowMessages.HTCAPTION)
                    {
                        // マウスの位置を取得してカーソル更新とマークの処理を行う
                        Point screenPoint = new Point(
                            (int)(m.LParam.ToInt64() & 0xFFFF),
                            (int)((m.LParam.ToInt64() >> 16) & 0xFFFF)
                        );
                        Point clientPoint = this.PointToClient(screenPoint);
                        this.Invalidate(); // マークの再描画を要求

                        m.Result = (IntPtr)WindowMessages.HTCLIENT;
                    }
                    return;

                case WindowMessages.WM_NCLBUTTONDOWN:
                case WindowMessages.WM_NCLBUTTONDBLCLK:  // タイトルバーダブルクリックも同じ処理
                    if (m.WParam.ToInt32() == WindowMessages.HTCAPTION)
                    {
                        Point screenPoint = new Point(
                            (int)(m.LParam.ToInt64() & 0xFFFF),
                            (int)((m.LParam.ToInt64() >> 16) & 0xFFFF)
                        );
                        Point clientPoint = this.PointToClient(screenPoint);

                        // 親子関係の更新のためにWindowManagerに通知
                        (windowManager ?? WindowManager.Current).BringWindowToFront(this);
                        (windowManager ?? WindowManager.Current).CheckPotentialParentWindow(this);

                        // クリックイベントをシミュレート
                        Message newMsg = new Message
                        {
                            Msg = WindowMessages.WM_LBUTTONDOWN,
                            WParam = m.WParam,
                            LParam = (IntPtr)((clientPoint.Y << 16) | clientPoint.X)
                        };
                        this.Strategy.HandleWindowMessage(this, newMsg);
                        return;
                    }
                    break;

                case WindowMessages.WM_NCMOUSEMOVE:
                    // タイトルバー上でのマウス移動も処理
                    Point mousePoint = new Point(
                        (int)(m.LParam.ToInt64() & 0xFFFF),
                        (int)((m.LParam.ToInt64() >> 16) & 0xFFFF)
                    );
                    Point clientMousePoint = this.PointToClient(mousePoint);
                    Strategy.UpdateCursor(this, clientMousePoint);
                    this.Invalidate(); // マークの再描画を要求
                    break;
            }

            var result = WindowMessageHandler.HandleWindowMessage(this, m);
            if (!result.Handled)
            {
                base.WndProc(ref m);
            }
            else
            {
                m.Result = result.Result;
            }
        }

        #endregion
        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                // 不可侵ウィンドウの場合、登録を解除
                if (IsNoEntryWindow)
                {
                    var noEntryZoneManager = NoEntryZoneManager.Current;
                    noEntryZoneManager.UnregisterNoEntryWindow(this);
                }

                (windowManager ?? WindowManager.Current).UnregisterFormOrder(this);
                foreach (var observer in observers.ToList())
                {
                    RemoveObserver(observer);
                }
            }
            base.Dispose(disposing);
        }
        public class SizeChangedEventArgs : EventArgs
        {
            public Size NewSize { get; }
            public SizeChangedEventArgs(Size newSize) => NewSize = newSize;
        }
        protected override CreateParams CreateParams
        {
            get
            {
                var cp = base.CreateParams;
                cp.ClassStyle |= 0x8; // CS_DBLCLKS
                return cp;
            }
        }
        public bool IsResizable() => strategy is ResizableWindowStrategy;

        // ゲームウィンドウ専用リサイズ制約実装
        public override Size GetMinimumSize()
        {
            // ゲームウィンドウの最小サイズは設定ファイルから取得
            return (gameSettings ?? GameSettings.Current).Window.MinimumSize;
        }

        public override Size GetMaximumSize()
        {
            // ゲームウィンドウの最大サイズを事実上無制限に設定
            return new Size(int.MaxValue, int.MaxValue);
        }

        public override bool ShouldInheritParentConstraints
        {
            get
            {
                // ゲームウィンドウは親の制約を部分的に継承（リサイズ可能ウィンドウの場合）
                return IsResizable();
            }
        }
    }
}