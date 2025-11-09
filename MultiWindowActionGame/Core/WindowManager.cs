using MultiWindowActionGame.Player;
using MultiWindowActionGame.Utilities;
using MultiWindowActionGame.Windows;
using MultiWindowActionGame.UI;
using MultiWindowActionGame.Rendering;
using MultiWindowActionGame.Interfaces;
using MultiWindowActionGame.Managers;
using System.Drawing.Drawing2D;
using System.Numerics;
using System.Runtime.InteropServices;
using static MultiWindowActionGame.Windows.GameWindow;
using ZOrderPriority = MultiWindowActionGame.Managers.ZOrderPriority;

namespace MultiWindowActionGame.Core
{
    public class WindowManager : IWindowManager, IWindowObserver
    {
        // インスタンス統一化のためのstatic参照
        private static WindowManager? _current;
        public static WindowManager Current
        {
            get => _current ?? throw new InvalidOperationException("WindowManager is not initialized");
            internal set => _current = value;
        }

        // DI対応のインスタンス用フィールド
        private readonly IWindowHierarchyManager hierarchyManager;
        private readonly IWindowZOrderManager zOrderManager;
        private readonly IWindowCollisionDetector collisionDetector;
        private readonly IWindowRenderingManager renderingManager;
        private readonly IDesktopIconManager desktopIconManager;
        private readonly IGameSettings gameSettings;

        private List<GameWindow> windows = new List<GameWindow>();
        private object windowLock = new object();
        private PlayerForm? player;
        private bool isInitialized = false;

        private OverlayForm? overlayForm;

        // DI constructor
        public WindowManager(
            IWindowHierarchyManager hierarchyManager,
            IWindowZOrderManager zOrderManager,
            IWindowCollisionDetector collisionDetector,
            IWindowRenderingManager renderingManager,
            IDesktopIconManager desktopIconManager,
            IGameSettings gameSettings)
        {
            this.hierarchyManager = hierarchyManager ?? throw new ArgumentNullException(nameof(hierarchyManager));
            this.zOrderManager = zOrderManager ?? throw new ArgumentNullException(nameof(zOrderManager));
            this.collisionDetector = collisionDetector ?? throw new ArgumentNullException(nameof(collisionDetector));
            this.renderingManager = renderingManager ?? throw new ArgumentNullException(nameof(renderingManager));
            this.desktopIconManager = desktopIconManager ?? throw new ArgumentNullException(nameof(desktopIconManager));
            this.gameSettings = gameSettings ?? throw new ArgumentNullException(nameof(gameSettings));
        }

        public void Initialize()
        {
            if (isInitialized) return;


            overlayForm = new OverlayForm(this, desktopIconManager);
            RegisterFormOrder(overlayForm, ZOrderPriority.DebugLayer);
            overlayForm.Show();

            isInitialized = true;
        }
        public async Task InitializeWindowsAsync(IEnumerable<GameWindow> windows)
        {
            var tasks = windows.Select(w => w.InitializationTask);

            try
            {
                // 5秒のタイムアウトを設定
                await Task.WhenAll(tasks).WaitAsync(TimeSpan.FromSeconds(5));
            }
            catch (TimeoutException)
            {
            }

            UpdateWindowGroupZOrder();
        }
        public void SetPlayer(PlayerForm player)
        {
            this.player = player;
        }

        public PlayerForm? GetPlayer()
        {
            return player;
        }
        public GameWindow? GetParentWindow(IEffectTarget child)
        {
            return hierarchyManager.GetParentWindow(child);
        }

        public IReadOnlyList<GameWindow> GetAllWindows()
        {
            lock (windowLock)
            {
                return windows.ToList();
            }
        }
        public void CheckPotentialParentWindow(GameWindow operatedWindow)
        {
            hierarchyManager.CheckPotentialParentWindow(operatedWindow, windows);
        }
        public void RegisterFormOrder(Form form, ZOrderPriority priority)
        {
            zOrderManager.RegisterFormOrder(form, priority);
        }
        public void UnregisterFormOrder(Form form)
        {
            zOrderManager.UnregisterFormOrder(form);
        }


        public void RegisterWindow(GameWindow window)
        {
            lock (windowLock)
            {
                window.AddObserver(this);
                windows.Add(window);

                // 親子関係のチェックと更新
                CheckPotentialParentWindow(window);
            }
        }

        public void ClearWindows()
        {
            lock (windowLock)
            {
                // すべてのウィンドウを閉じる
                foreach (var window in windows.ToList())
                {
                    window.RemoveObserver(this);  // オブザーバーの解除を追加
                    window.Close();
                }
                windows.Clear();
            }
        }
        public HashSet<IEffectTarget> GetContainedTargets(GameWindow window)
        {
            return hierarchyManager.GetContainedTargets(window);
        }

        public IReadOnlyList<GameButton> GetAllButtons()
        {
            return zOrderManager.GetAllButtons();
        }
        public int GetWindowZIndex(GameWindow window)
        {
            lock (windowLock)
            {
                return windows.IndexOf(window);
            }
        }
        public List<GameWindow> GetIntersectingWindows(Rectangle bounds)
        {
            return collisionDetector.GetIntersectingWindows(bounds, windows);
        }

        public async Task UpdateAsync(float deltaTime)
        {
            List<GameWindow> windowsCopy;
            lock (windowLock)
            {
                windowsCopy = new List<GameWindow>(windows);
            }

            foreach (var window in windowsCopy)
            {
                await window.UpdateAsync(deltaTime);
            }
        }
        public void Draw(Graphics g)
        {
            var formsByPriority = zOrderManager.GetFormsByPriority();
            var allComponents = renderingManager.GetAllComponents(windows, player, formsByPriority);
            renderingManager.Draw(g, allComponents);
        }
        public void DrawMarks(Graphics g)
        {
            renderingManager.DrawMarks(g, windows);
        }
        public GameWindow? GetWindowAt(Rectangle bounds, GameWindow? currentWindow = null)
        {
            return collisionDetector.GetWindowAt(bounds, windows, currentWindow);
        }

        public GameWindow? GetTopWindowAt(Rectangle bounds, GameWindow? currentWindow)
        {
            return collisionDetector.GetTopWindowAt(bounds, windows, currentWindow);
        }
        public GameWindow? GetWindowFullyContaining(Rectangle bounds)
        {
            return collisionDetector.GetWindowFullyContaining(bounds, windows);
        }
        public void BringWindowToFront(GameWindow window)
        {
            var updatedWindows = zOrderManager.BringWindowToFront(window, windows);
            lock (windowLock)
            {
                windows.Clear();
                windows.AddRange(updatedWindows);
            }
        }
        public void UpdateDisplay()
        {
            overlayForm?.UpdateOverlay();
        }
        public Region CalculateMovableRegion(GameWindow? currentWindow)
        {
            // currentWindowがnullの場合（プレイヤーがウィンドウの外にいる場合）、
            // メインフォームの領域を返す
            if (currentWindow == null && Program.mainForm != null)
            {
                return new Region(new Rectangle(0, 0,
                    Program.mainForm.ClientSize.Width,
                    Program.mainForm.ClientSize.Height));
            }

            // currentWindowが有効な場合は既存の処理を実行
            if (currentWindow != null)
            {
                Region movableRegion = new Region(currentWindow.AdjustedBounds);

                lock (windowLock)
                {
                    var topLevelWindows = windows.Where(w => w.Parent == null && w != currentWindow);
                    foreach (var window in topLevelWindows)
                    {
                        if (window.AdjustedBounds.IntersectsWith(currentWindow.AdjustedBounds) ||
                            IsAdjacentTo(window.AdjustedBounds, currentWindow.AdjustedBounds))
                        {
                            movableRegion.Union(window.AdjustedBounds);
                            foreach (var child in window.GetAllDescendants())
                            {
                                movableRegion.Union(child.AdjustedBounds);
                            }
                        }
                    }
                }

                return movableRegion;
            }

            // どちらの条件も満たさない場合は空のリージョンを返す
            return new Region();
        }
        private bool IsAdjacentTo(Rectangle rect1, Rectangle rect2)
        {
            var settings = gameSettings.Gameplay;
            return (Math.Abs(rect1.Right - rect2.Left) <= settings.WindowSnapDistance ||
                    Math.Abs(rect1.Left - rect2.Right) <= settings.WindowSnapDistance ||
                    Math.Abs(rect1.Bottom - rect2.Top) <= settings.WindowSnapDistance ||
                    Math.Abs(rect1.Top - rect2.Bottom) <= settings.WindowSnapDistance) &&
                   (rect1.Left <= rect2.Right && rect2.Left <= rect1.Right &&
                    rect1.Top <= rect2.Bottom && rect2.Top <= rect1.Bottom);
        }
        public GameWindow? GetNearestWindow(Rectangle bounds)
        {
            return collisionDetector.GetNearestWindow(bounds, windows);
        }
        public void UpdateWindowGroupZOrder()
        {
            zOrderManager.UpdateWindowGroupZOrder();
        }

        public void UpdateFormZOrder(Form form, ZOrderPriority priority)
        {
            zOrderManager.UpdateFormZOrder(form, priority);
        }
        void IWindowObserver.OnWindowChanged(GameWindow window, WindowChangeType changeType)
        {
            if (changeType == WindowChangeType.Deleted)
            {
                lock (windowLock)
                {
                    windows.Remove(window);
                }
            }
        }

        public void OnWindowChanged(GameWindow window, WindowChangeType changeType)
        {
            ((IWindowObserver)this).OnWindowChanged(window, changeType);
        }
    }
}