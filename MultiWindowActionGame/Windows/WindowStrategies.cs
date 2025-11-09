using MultiWindowActionGame;
using MultiWindowActionGame.Utilities;
using MultiWindowActionGame.Effects;
using MultiWindowActionGame.Core;
using MultiWindowActionGame.Interfaces;
using MultiWindowActionGame.Collision;
using System;
using System.Drawing;
using System.Numerics;
using System.Windows.Forms;
using static MultiWindowActionGame.Utilities.GameSettings;

namespace MultiWindowActionGame.Windows
{
    public interface IWindowStrategy
    {
        void Update(GameWindow window, float deltaTime);
        void HandleInput(GameWindow window);
        void HandleResize(GameWindow window);
        void HandleWindowMessage(GameWindow window, Message m);
        void UpdateCursor(GameWindow window, Point clientMousePos);
        void DrawStrategyMark(Graphics g, Rectangle bounds, bool isHovered);
    }
    public abstract class BaseWindowStrategy : IWindowStrategy
    {
        // 共通のフィールド
        protected bool isActive = false;
        protected readonly WindowSettings settings;
        protected readonly IInputService? inputService;
        protected readonly IGameSettings? gameSettings;
        protected readonly INoEntryZoneManager? noEntryZoneManager;
        protected readonly ICollisionService? collisionService;

        // 不可侵ウィンドウフラグ（デフォルトはfalse）
        public virtual bool IsNoEntry => false;

        // NoEntryZoneManagerへのアクセスを共通化（Fallbackパターン）
        protected INoEntryZoneManager ZoneManager => noEntryZoneManager ?? NoEntryZoneManager.Current;

        protected BaseWindowStrategy(
            IInputService? inputService = null,
            IGameSettings? gameSettings = null,
            INoEntryZoneManager? noEntryZoneManager = null,
            ICollisionService? collisionService = null)
        {
            this.inputService = inputService;
            this.gameSettings = gameSettings;
            this.noEntryZoneManager = noEntryZoneManager;
            this.collisionService = collisionService;

            // Fallback to static reference for backward compatibility
            settings = gameSettings?.Window ?? GameSettings.Current.Window;
        }

        // 基本実装を提供するメソッド
        public virtual void Update(GameWindow window, float deltaTime) { }
        public virtual void HandleInput(GameWindow window) { }
        public virtual void HandleResize(GameWindow window)
        {
            window.Invalidate();
        }
        public virtual void HandleWindowMessage(GameWindow window, Message m)
        {
            switch (m.Msg)
            {
                case WindowMessages.WM_LBUTTONDOWN:
                    OnMouseDown(window);
                    break;
                case WindowMessages.WM_LBUTTONUP:
                    OnMouseUp(window);
                    break;
                case WindowMessages.WM_MOUSEMOVE:
                    OnMouseMove(window);
                    break;
                case WindowMessages.WM_SYSCOMMAND:
                    int command = m.WParam.ToInt32() & 0xFFF0;
                    if (command == WindowMessages.SC_RESTORE)
                    {
                        window.OnRestore();
                        // 親子判定はOSの復元処理後に実行されるため、ここでは呼ばない
                    }
                    break;
            }
        }
        // 新しい共通メソッド
        protected virtual void OnMouseDown(GameWindow window) { }
        protected virtual void OnMouseUp(GameWindow window) { }
        protected virtual void OnMouseMove(GameWindow window) { }

        // カーソル管理の共通実装
        public virtual void UpdateCursor(GameWindow window, Point clientMousePos)
        {
            window.Cursor = GetStrategyCursor();
        }

        // 各ストラテジーで実装が必要なメソッド
        public abstract void DrawStrategyMark(Graphics g, Rectangle bounds, bool isHovered);
        protected abstract Cursor GetStrategyCursor();
    }
    public static class StrategyMarkUtility
    {
        public const int DEFAULT_MARK_SIZE = 60;

        public static void DrawMarkBackground(Graphics g, Rectangle bounds, int markSize, Color color)
        {
            int x = bounds.X + (bounds.Width - markSize) / 2;
            int y = bounds.Y + (bounds.Height - markSize) / 2;

            using (var brush = new SolidBrush(Color.FromArgb(50, color)))
            {
                g.FillRectangle(brush, x, y, markSize, markSize);
            }
        }

        public static Point GetMarkCenter(Rectangle bounds, int markSize)
        {
            return new Point(
                bounds.X + (bounds.Width - markSize) / 2,
                bounds.Y + (bounds.Height - markSize) / 2
            );
        }

        public static Color GetMarkColor(bool isHovered)
        {
            return isHovered ? Color.White : Color.FromArgb(128, 128, 128);
        }

        public static void DrawArrowHead(Graphics g, Pen pen, Point start, Point end, int headSize = 10)
        {
            float angle = (float)Math.Atan2(end.Y - start.Y, end.X - start.X);
            float arrowAngle = (float)(Math.PI / 6); // 30度

            PointF p1 = new PointF(
                end.X - headSize * (float)Math.Cos(angle + arrowAngle),
                end.Y - headSize * (float)Math.Sin(angle + arrowAngle)
            );

            PointF p2 = new PointF(
                end.X - headSize * (float)Math.Cos(angle - arrowAngle),
                end.Y - headSize * (float)Math.Sin(angle - arrowAngle)
            );

            g.DrawLine(pen, end, p1);
            g.DrawLine(pen, end, p2);
        }
    }
    public class NormalWindowStrategy : BaseWindowStrategy
    {
        public NormalWindowStrategy(
            IInputService? inputService = null,
            IGameSettings? gameSettings = null,
            INoEntryZoneManager? noEntryZoneManager = null,
            ICollisionService? collisionService = null)
            : base(inputService, gameSettings, noEntryZoneManager, collisionService)
        {
        }

        public override void DrawStrategyMark(Graphics g, Rectangle bounds, bool isHovered) { }
        protected override Cursor GetStrategyCursor() => Cursors.Default;
    }
    public class ResizableWindowStrategy : BaseWindowStrategy
    {
        private readonly ResizeEffect resizeEffect;
        private bool isResizing = false;
        private Point lastMousePos;
        private Size originalSize ;
        private readonly Dictionary<IEffectTarget, Size> originalSizes = new();

        public ResizableWindowStrategy(
            IInputService? inputService = null,
            IGameSettings? gameSettings = null,
            INoEntryZoneManager? noEntryZoneManager = null,
            ICollisionService? collisionService = null)
            : base(inputService, gameSettings, noEntryZoneManager, collisionService)
        {
            resizeEffect = new ResizeEffect();
            WindowEffectManager.Current.AddEffect(resizeEffect);
        }

        protected override void OnMouseDown(GameWindow window)
        {
            StartResizing(window);
            window.Capture = true;
        }

        protected override void OnMouseUp(GameWindow window)
        {
            window.Capture = false;
            StopResizing();
        }

        public override void Update(GameWindow window, float deltaTime)
        {
            if (isResizing)
            {
                UpdateResize(window);
            }
        }
        private void UpdateResize(GameWindow window)
        {
            Point currentMousePos = window.PointToClient(Cursor.Position);
            Size newSize = CalculateNewSize(window, currentMousePos);

            // 早期リターン条件を強化（パフォーマンス最適化）
            var currentSize = window.CollisionBounds.Size;
            if (newSize.Width == currentSize.Width && newSize.Height == currentSize.Height) return;
            
            // 最小変更闾値を設定（微小な変更での不要な処理を回避）
            int deltaWidth = Math.Abs(newSize.Width - currentSize.Width);
            int deltaHeight = Math.Abs(newSize.Height - currentSize.Height);
            if (deltaWidth < 2 && deltaHeight < 2) return;

            SizeF scale = new(
                (float)newSize.Width / originalSize.Width,
                (float)newSize.Height / originalSize.Height
            );

            // CollisionBoundsを一貫して使用
            Rectangle proposedBounds = new(
                window.CollisionBounds.Location,
                newSize
            );

            // CollisionService or fallback to ZoneManager
            bool hasCollision;
            if (collisionService != null)
            {
                var options = new CollisionOptions
                {
                    ExcludeWindow = window,
                    ExcludeChildren = true,  // 親ウィンドウが子を持つ場合、子も除外
                    CheckNoEntryZones = true,
                    CheckNoEntryBoundaries = true,
                    CheckNormalWindows = window.IsNoEntryWindow,  // 不可侵ウィンドウは通常のウィンドウともぶつかる
                    CheckButtons = window.IsNoEntryWindow,  // 不可侵ウィンドウはボタンともぶつかる
                    UseZOrderFiltering = true
                };
                hasCollision = collisionService.CheckCollision(proposedBounds, options);
            }
            else
            {
                hasCollision = ZoneManager.IntersectsWithAnyZone(proposedBounds, window);
            }

            // Z-order + Region考慮の不可侵ウィンドウ境界判定を含む（自分自身を除外）
            if (!hasCollision)
            {
                // 子の不可侵境界との接触をチェックしてサイズを調整
                newSize = CheckChildBoundaryContact(window, originalSize, newSize);

                // 親がある場合かつ自分自身が不可侵ウィンドウの場合、親の境界内に収まるようにサイズを制約（3pxバッファ）
                if (window.Parent is GameWindow resizeParentWindow && window.IsNoEntryWindow)
                {
                    const int PARENT_BOUNDARY_BUFFER = 5; // 親境界とのバッファ（px）
                    Rectangle parentBounds = resizeParentWindow.CollisionBounds;
                    Rectangle currentBounds = window.CollisionBounds;

                    // 親の境界を超えないように幅を制限（3pxバッファ）
                    int maxWidth = parentBounds.Right - currentBounds.X - PARENT_BOUNDARY_BUFFER;
                    if (newSize.Width > maxWidth)
                    {
                        newSize.Width = Math.Max(settings.MinimumSize.Width, maxWidth);
                    }

                    // 親の境界を超えないように高さを制限（3pxバッファ）
                    int maxHeight = parentBounds.Bottom - currentBounds.Y - PARENT_BOUNDARY_BUFFER;
                    if (newSize.Height > maxHeight)
                    {
                        newSize.Height = Math.Max(settings.MinimumSize.Height, maxHeight);
                    }
                }

                // スケールを再計算（調整されたサイズに基づく）
                scale = new SizeF((float)newSize.Width / originalSize.Width, (float)newSize.Height / originalSize.Height);

                // リファクタリング修正: movableRegionは毎回計算されるため、更新不要

                // サイズ更新とスケール適用をバッチ処理
                window.UpdateTargetSize(newSize);
                ApplyScaleToHierarchy(window, scale);

                // エフェクト適用は最後に一度だけ
                WindowEffectManager.Current.ApplyEffects(window);
            }
        }
        private void ApplyScaleToHierarchy(GameWindow window, SizeF scale)
        {
            // 現在のウィンドウにスケールを適用
            resizeEffect.UpdateScale(window, scale, originalSize);

            // 子要素のスケーリング処理（精度向上版）
            foreach (var child in window.Children.ToList()) // ToList()でコレクション変更時の例外を防止
            {
                if (!originalSizes.ContainsKey(child))
                {
                    // より精密な元サイズ計算（浮動小数点演算の精度向上）
                    var currentSize = child.GetOriginalSize();
                    originalSizes[child] = new Size(
                        Math.Max(1, (int)Math.Round(currentSize.Width / scale.Width, MidpointRounding.AwayFromZero)),
                        Math.Max(1, (int)Math.Round(currentSize.Height / scale.Height, MidpointRounding.AwayFromZero))
                    );
                }

                var childOriginalSize = originalSizes[child];

                // 子要素の新サイズを計算（各要素の最小サイズ制約を使用）
                var childMinSize = child.GetMinimumSize();
                var childMaxSize = child.GetMaximumSize();
                var proposedSize = new Size(
                    (int)Math.Round(childOriginalSize.Width * scale.Width, MidpointRounding.AwayFromZero),
                    (int)Math.Round(childOriginalSize.Height * scale.Height, MidpointRounding.AwayFromZero)
                );
                var newChildSize = new Size(
                    Math.Max(childMinSize.Width, Math.Min(childMaxSize.Width, proposedSize.Width)),
                    Math.Max(childMinSize.Height, Math.Min(childMaxSize.Height, proposedSize.Height))
                );

                // スケールを再計算（実際に適用されるサイズに基づく）
                var actualScale = new SizeF(
                    (float)newChildSize.Width / childOriginalSize.Width,
                    (float)newChildSize.Height / childOriginalSize.Height
                );

                resizeEffect.UpdateScale(child, actualScale, childOriginalSize);

                if (child is GameWindow childWindow)
                {
                    ApplyScaleToChildrenRecursive(childWindow, actualScale);
                }
            }
        }

        // 再帰的に子孫すべてにスケールを適用する最適化版メソッド
        private readonly HashSet<IEffectTarget> currentlyUpdating = new HashSet<IEffectTarget>();

        private void ApplyScaleToChildrenRecursive(GameWindow window, SizeF scale)
        {
            // 循環更新を防ぐ
            if (currentlyUpdating.Contains(window))
            {
                System.Diagnostics.Debug.WriteLine($"WindowStrategies: 循環更新を検出、{window.GetType().Name}の処理をスキップ");
                return;
            }

            try
            {
                currentlyUpdating.Add(window);

                // 重複計算を避けるため、子要素のリストを一度だけ取得
                var children = window.Children.ToList();
                
                foreach (var child in children)
                {
                    // 子要素の循環更新チェック
                    if (currentlyUpdating.Contains(child))
                    {
                        System.Diagnostics.Debug.WriteLine($"WindowStrategies: 子要素の循環更新を検出、{child.GetType().Name}の処理をスキップ");
                        continue;
                    }

                    try
                    {
                        currentlyUpdating.Add(child);

                        if (!originalSizes.ContainsKey(child))
                        {
                            originalSizes[child] = child.GetOriginalSize();
                        }

                        var childOriginalSize = originalSizes[child];
                        
                        // 各子要素の制約を考慮した新サイズ計算
                        var childMinSize = child.GetMinimumSize();
                        var childMaxSize = child.GetMaximumSize();
                        var proposedSize = new Size(
                            (int)Math.Round(childOriginalSize.Width * scale.Width, MidpointRounding.AwayFromZero),
                            (int)Math.Round(childOriginalSize.Height * scale.Height, MidpointRounding.AwayFromZero)
                        );
                        var newSize = new Size(
                            Math.Max(childMinSize.Width, Math.Min(childMaxSize.Width, proposedSize.Width)),
                            Math.Max(childMinSize.Height, Math.Min(childMaxSize.Height, proposedSize.Height))
                        );
                        
                        // 実際に適用されるスケールを再計算
                        var actualScale = new SizeF(
                            (float)newSize.Width / childOriginalSize.Width,
                            (float)newSize.Height / childOriginalSize.Height
                        );
                        
                        resizeEffect.UpdateScale(child, actualScale, childOriginalSize);

                        // さらに子がある場合は再帰的に処理（重複チェック追加）
                        if (child is GameWindow childWindow && childWindow.Children.Count > 0)
                        {
                            ApplyScaleToChildrenRecursive(childWindow, actualScale);
                        }
                    }
                    catch (Exception ex)
                    {
                        System.Diagnostics.Debug.WriteLine($"WindowStrategies: 子要素更新エラー - {ex.Message}");
                    }
                    finally
                    {
                        currentlyUpdating.Remove(child);
                    }
                }
            }
            finally
            {
                currentlyUpdating.Remove(window);
            }
        }
        private Size CalculateNewSize(GameWindow window, Point currentMousePos)
        {
            int dx = currentMousePos.X - lastMousePos.X;
            int dy = currentMousePos.Y - lastMousePos.Y;

            // 各要素のGetMaximumSize()を使用した動的制約適用
            var windowMaxSize = window.GetMaximumSize();

            Size proposedSize = new(
                Math.Max(settings.MinimumSize.Width,
                    Math.Min(windowMaxSize.Width, originalSize.Width + dx)),
                Math.Max(settings.MinimumSize.Height,
                    Math.Min(windowMaxSize.Height, originalSize.Height + dy))
            );

            // 親が不可侵ウィンドウの場合、親の境界内に制限
            if (window.Parent is GameWindow parentWindow && parentWindow.IsNoEntryWindow)
            {
                Rectangle parentBounds = parentWindow.CollisionBounds;
                Rectangle windowBounds = window.CollisionBounds;

                // 親の境界を超えないように最大サイズを制限
                int maxWidth = parentBounds.Right - windowBounds.Left;
                int maxHeight = parentBounds.Bottom - windowBounds.Top;

                proposedSize = new Size(
                    Math.Min(proposedSize.Width, maxWidth),
                    Math.Min(proposedSize.Height, maxHeight)
                );
            }

            // 境界チェックの強化（不可侵領域との衝突チェック）
            Size validSize;
            if (collisionService != null)
            {
                var options = new CollisionOptions
                {
                    ExcludeWindow = window,
                    ExcludeChildren = true,  // 親ウィンドウが子を持つ場合、子も除外
                    CheckNoEntryZones = true,
                    CheckNoEntryBoundaries = true,
                    CheckNormalWindows = window.IsNoEntryWindow,  // 不可侵ウィンドウは通常のウィンドウともぶつかる
                    CheckButtons = window.IsNoEntryWindow,  // 不可侵ウィンドウはボタンともぶつかる
                    UseZOrderFiltering = true
                };
                validSize = collisionService.ValidateSize(window.CollisionBounds, proposedSize, options);
            }
            else
            {
                validSize = ZoneManager.GetValidSize(window.CollisionBounds, proposedSize, window);
            }

            // 子要素がある場合の動的サイズチェック
            if (window.Children.Count > 0)
            {
                // 子要素の最小サイズを考慮してウィンドウサイズを調整
                int maxChildMinWidth = 0, maxChildMinHeight = 0;
                foreach (var child in window.Children)
                {
                    var childMinSize = child.GetMinimumSize();
                    maxChildMinWidth = Math.Max(maxChildMinWidth, childMinSize.Width);
                    maxChildMinHeight = Math.Max(maxChildMinHeight, childMinSize.Height);
                }
                validSize = new Size(
                    Math.Max(validSize.Width, maxChildMinWidth + 20), // パディングを考慮
                    Math.Max(validSize.Height, maxChildMinHeight + 40) // タイトルバー分を考慮
                );
            }

            return validSize;
        }

        private void StartResizing(GameWindow window)
        {
            if (isResizing) return;
            isResizing = true;
            lastMousePos = window.PointToClient(Cursor.Position);
            originalSize = window.GetOriginalSize();

            // メモリ最適化：既存のキャッシュをクリアしてから新しいサイズを記録
            originalSizes.Clear();
            
            // リサイズ開始時に、すべての子要素の元のサイズを再帰的に記録
            RecordOriginalSizesRecursive(window);
        }

        private void StopResizing()
        {
            if (!isResizing) return;
            isResizing = false;
            
            try
            {
                // メモリリーク防止のための強化されたクリーンアップ
                originalSizes.Clear();
                currentlyUpdating.Clear(); // 循環チェック用HashSetもクリア
                resizeEffect.ResetAll();
                
                // GCへのヒント（大量のオブジェクトが解放された後）
                if (originalSizes.Count > 50) // 闾値を設けて必要な場合のみ
                {
                    GC.Collect(0, GCCollectionMode.Optimized);
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"WindowStrategies: リサイズ終了処理エラー - {ex.Message}");
            }
        }

        /// <summary>
        /// ウィンドウとその子要素の元サイズを再帰的に記録（メモリ最適化）
        /// </summary>
        private void RecordOriginalSizesRecursive(GameWindow window)
        {
            foreach (var child in window.Children)
            {
                if (!originalSizes.ContainsKey(child))
                {
                    originalSizes[child] = child.GetOriginalSize();
                }
                
                // 子ウィンドウがある場合は再帰的に処理
                if (child is GameWindow childWindow && childWindow.Children.Count > 0)
                {
                    RecordOriginalSizesRecursive(childWindow);
                }
            }
        }

        public override void DrawStrategyMark(Graphics g, Rectangle bounds, bool isHovered)
        {
            DrawResizeMark(g, bounds, isHovered ? Color.White : Color.FromArgb(128, 128, 128));
        }

        protected override Cursor GetStrategyCursor() => Cursors.SizeNWSE;

        private void DrawResizeMark(Graphics g, Rectangle bounds, Color color)
        {
            int markSize = 60;
            int x = bounds.X + (bounds.Width - markSize) / 2;
            int y = bounds.Y + (bounds.Height - markSize) / 2;

            using (var pen = new Pen(color, 2))
            {
                g.DrawLine(pen, x + markSize / 4, y + markSize / 4, x + markSize * 3 / 4, y + markSize * 3 / 4);
                DrawResizeArrows(g, pen, x, y, markSize);
            }
        }

        private void DrawResizeArrows(Graphics g, Pen pen, int x, int y, int size)
        {
            // 左上矢印
            DrawArrowHead(g, pen, x + size / 4, y + size / 4, x + size / 2, y + size / 4);
            DrawArrowHead(g, pen, x + size / 4, y + size / 4, x + size / 4, y + size / 2);

            // 右下矢印
            DrawArrowHead(g, pen, x + size * 3 / 4, y + size * 3 / 4, x + size / 2, y + size * 3 / 4);
            DrawArrowHead(g, pen, x + size * 3 / 4, y + size * 3 / 4, x + size * 3 / 4, y + size / 2);
        }

        private void DrawArrowHead(Graphics g, Pen pen, int x1, int y1, int x2, int y2)
        {
            g.DrawLine(pen, x1, y1, x2, y2);
        }

        /// <summary>
        /// 親のリサイズ時に子の不可侵境界との接触をチェックしてサイズを制限
        /// 不可侵境界は外周5pxの領域
        /// </summary>
        private Size CheckChildBoundaryContact(GameWindow window, Size originalSize, Size proposedSize)
        {
            // リサイズ方向を判定
            bool isShrinkingWidth = proposedSize.Width < window.Size.Width;
            bool isShrinkingHeight = proposedSize.Height < window.Size.Height;

            Size constrainedSize = proposedSize;
            Rectangle currentBounds = window.CollisionBounds;
            const int BOUNDARY_WIDTH = 3; // 不可侵境界の幅

            // デバッグ用ログ
            System.Diagnostics.Debug.WriteLine($"[CheckChildBoundaryContact] Current: {currentBounds}, Proposed: {proposedSize}, Shrinking W:{isShrinkingWidth} H:{isShrinkingHeight}");

            // 全子孫ウィンドウをチェック（不可侵ウィンドウと通常ウィンドウの両方）
            foreach (var child in window.GetAllDescendants().OfType<GameWindow>())
            {
                Rectangle childBounds = child.CollisionBounds;
                // 不可侵ウィンドウの場合は境界を考慮、通常ウィンドウの場合は境界なし
                int boundaryWidth = child.IsNoEntryWindow ? BOUNDARY_WIDTH : 0;

                System.Diagnostics.Debug.WriteLine($"  Child {(child.IsNoEntryWindow ? "NoEntry" : "Normal")}: {childBounds}, Boundary: {boundaryWidth}px");

                // 幅方向のチェック
                if (isShrinkingWidth)
                {
                    // 縮小時: 親の右辺が子の右辺境界に達する場合
                    int proposedRight = currentBounds.X + constrainedSize.Width;
                    int childRightBoundary = childBounds.Right + boundaryWidth;

                    System.Diagnostics.Debug.WriteLine($"    Width(Shrink): proposedRight={proposedRight}, childRightBoundary={childRightBoundary}, currentRight={currentBounds.Right}");

                    if (proposedRight <= childRightBoundary && currentBounds.Right > childRightBoundary)
                    {
                        // Y座標の重なりもチェック
                        if (currentBounds.Bottom > childBounds.Top && currentBounds.Top < childBounds.Bottom)
                        {
                            int minWidth = childRightBoundary - currentBounds.X + 1; // 1px余裕
                            if (minWidth > 0 && minWidth > constrainedSize.Width)
                            {
                                System.Diagnostics.Debug.WriteLine($"    *** Width CONSTRAINED: {constrainedSize.Width} -> {minWidth}");
                                constrainedSize.Width = minWidth;
                            }
                        }
                    }
                }
                else // 拡大時
                {
                    // 拡大時: 親の右辺が子の左辺境界に達する場合
                    int proposedRight = currentBounds.X + constrainedSize.Width;
                    int childLeftBoundary = childBounds.Left - boundaryWidth;

                    System.Diagnostics.Debug.WriteLine($"    Width(Grow): proposedRight={proposedRight}, childLeftBoundary={childLeftBoundary}, currentRight={currentBounds.Right}");

                    if (proposedRight >= childLeftBoundary && currentBounds.Right < childLeftBoundary)
                    {
                        // Y座標の重なりもチェック
                        if (currentBounds.Bottom > childBounds.Top && currentBounds.Top < childBounds.Bottom)
                        {
                            int maxWidth = childLeftBoundary - currentBounds.X - 1; // 1px余裕
                            if (maxWidth > 0 && maxWidth < constrainedSize.Width)
                            {
                                System.Diagnostics.Debug.WriteLine($"    *** Width CONSTRAINED (Grow): {constrainedSize.Width} -> {maxWidth}");
                                constrainedSize.Width = maxWidth;
                            }
                        }
                    }
                }

                // 高さ方向のチェック
                if (isShrinkingHeight)
                {
                    // 縮小時: 親の下辺が子の下辺境界に達する場合
                    int proposedBottom = currentBounds.Y + constrainedSize.Height;
                    int childBottomBoundary = childBounds.Bottom + boundaryWidth;

                    System.Diagnostics.Debug.WriteLine($"    Height(Shrink): proposedBottom={proposedBottom}, childBottomBoundary={childBottomBoundary}, currentBottom={currentBounds.Bottom}");

                    if (proposedBottom <= childBottomBoundary && currentBounds.Bottom > childBottomBoundary)
                    {
                        // X座標の重なりもチェック（調整後の幅を使用）
                        int proposedRight = currentBounds.X + constrainedSize.Width;
                        if (proposedRight > childBounds.Left && currentBounds.Left < childBounds.Right)
                        {
                            int minHeight = childBottomBoundary - currentBounds.Y + 1; // 1px余裕
                            if (minHeight > 0 && minHeight > constrainedSize.Height)
                            {
                                System.Diagnostics.Debug.WriteLine($"    *** Height CONSTRAINED: {constrainedSize.Height} -> {minHeight}");
                                constrainedSize.Height = minHeight;
                            }
                        }
                    }
                }
                else // 拡大時
                {
                    // 拡大時: 親の下辺が子の上辺境界に達する場合
                    int proposedBottom = currentBounds.Y + constrainedSize.Height;
                    int childTopBoundary = childBounds.Top - boundaryWidth;

                    System.Diagnostics.Debug.WriteLine($"    Height(Grow): proposedBottom={proposedBottom}, childTopBoundary={childTopBoundary}, currentBottom={currentBounds.Bottom}");

                    if (proposedBottom >= childTopBoundary && currentBounds.Bottom < childTopBoundary)
                    {
                        // X座標の重なりもチェック（調整後の幅を使用）
                        int proposedRight = currentBounds.X + constrainedSize.Width;
                        if (proposedRight > childBounds.Left && currentBounds.Left < childBounds.Right)
                        {
                            int maxHeight = childTopBoundary - currentBounds.Y - 1; // 1px余裕
                            if (maxHeight > 0 && maxHeight < constrainedSize.Height)
                            {
                                System.Diagnostics.Debug.WriteLine($"    *** Height CONSTRAINED (Grow): {constrainedSize.Height} -> {maxHeight}");
                                constrainedSize.Height = maxHeight;
                            }
                        }
                    }
                }
            }

            return constrainedSize;
        }
    }
    public class MovableWindowStrategy : BaseWindowStrategy
    {
        private readonly MovementEffect movementEffect = new MovementEffect();
        private Point lastMousePos;
        private bool isDragging = false;
        private Point lastValidPosition;

        private bool isBlockedRight = false;
        private bool isBlockedLeft = false;
        private bool isBlockedDown = false;
        private bool isBlockedUp = false;

        public MovableWindowStrategy(
            IInputService? inputService = null,
            IGameSettings? gameSettings = null,
            INoEntryZoneManager? noEntryZoneManager = null,
            ICollisionService? collisionService = null)
            : base(inputService, gameSettings, noEntryZoneManager, collisionService)
        {
            movementEffect = new MovementEffect();
            WindowEffectManager.Current.AddEffect(movementEffect);
        }
        protected override void OnMouseDown(GameWindow window)
        {
            StartDragging(window);
            window.Capture = true;
        }
        protected override void OnMouseUp(GameWindow window)
        {
            window.Capture = false;
            StopDragging();
        }
        public override void Update(GameWindow window, float deltaTime)
        {
            if (isDragging)
            {
                UpdateMovement(window);
            }
        }
        private void UpdateMovement(GameWindow window)
        {
            Point currentMousePos = window.PointToClient(Cursor.Position);
            var movement = CalculateMovement(window, currentMousePos);

            // 移動の適用
            movementEffect.UpdateMovement(movement);
            WindowEffectManager.Current.ApplyEffects(window);
        }
        private Vector2 CalculateMovement(GameWindow window, Point currentMousePos)
        {
            int deltaX = currentMousePos.X - lastMousePos.X;
            int deltaY = currentMousePos.Y - lastMousePos.Y;

            UpdateBlockFlags(window);

            Vector2 movement = new(
                (deltaX > 0 && isBlockedRight) || (deltaX < 0 && isBlockedLeft) ? 0 : deltaX,
                (deltaY > 0 && isBlockedDown) || (deltaY < 0 && isBlockedUp) ? 0 : deltaY
            );

            Rectangle proposedBounds = new(
                window.CollisionBounds.X + (int)movement.X,
                window.CollisionBounds.Y + (int)movement.Y,
                window.CollisionBounds.Width,
                window.CollisionBounds.Height
            );

            // 親が不可侵ウィンドウの場合、親の境界内に制限
            if (window.Parent is GameWindow parentWindow && parentWindow.IsNoEntryWindow)
            {
                Rectangle parentBounds = parentWindow.CollisionBounds;

                // 親の境界を超えないように制限
                int adjustedX = proposedBounds.X;
                int adjustedY = proposedBounds.Y;

                if (proposedBounds.Left < parentBounds.Left)
                    adjustedX = parentBounds.Left;
                if (proposedBounds.Right > parentBounds.Right)
                    adjustedX = parentBounds.Right - proposedBounds.Width;
                if (proposedBounds.Top < parentBounds.Top)
                    adjustedY = parentBounds.Top;
                if (proposedBounds.Bottom > parentBounds.Bottom)
                    adjustedY = parentBounds.Bottom - proposedBounds.Height;

                proposedBounds = new Rectangle(
                    adjustedX,
                    adjustedY,
                    proposedBounds.Width,
                    proposedBounds.Height
                );
            }

            Rectangle validBounds;
            if (collisionService != null)
            {
                var options = new CollisionOptions
                {
                    ExcludeWindow = window,
                    ExcludeChildren = true,  // 親ウィンドウが子を持つ場合、子も除外
                    CheckNoEntryZones = true,
                    CheckNoEntryBoundaries = true,
                    CheckNormalWindows = window.IsNoEntryWindow,  // 不可侵ウィンドウは通常のウィンドウともぶつかる
                    CheckButtons = window.IsNoEntryWindow,  // 不可侵ウィンドウはボタンともぶつかる
                    UseZOrderFiltering = true
                };
                validBounds = collisionService.ValidatePosition(window.CollisionBounds, proposedBounds, options);
            }
            else
            {
                validBounds = ZoneManager.GetValidPosition(window.CollisionBounds, proposedBounds, window);
            }

            // 親がある場合かつ自分自身が不可侵ウィンドウの場合、validBoundsを親の境界内に再制約（3pxバッファ）
            if (window.Parent is GameWindow finalParentWindow && window.IsNoEntryWindow)
            {
                const int PARENT_BOUNDARY_BUFFER = 5; // 親境界とのバッファ（px）
                Rectangle finalParentBounds = finalParentWindow.CollisionBounds;
                int constrainedX = validBounds.X;
                int constrainedY = validBounds.Y;

                if (validBounds.Left < finalParentBounds.Left + PARENT_BOUNDARY_BUFFER)
                    constrainedX = finalParentBounds.Left + PARENT_BOUNDARY_BUFFER;
                if (validBounds.Right > finalParentBounds.Right - PARENT_BOUNDARY_BUFFER)
                    constrainedX = finalParentBounds.Right - PARENT_BOUNDARY_BUFFER - validBounds.Width;
                if (validBounds.Top < finalParentBounds.Top + PARENT_BOUNDARY_BUFFER)
                    constrainedY = finalParentBounds.Top + PARENT_BOUNDARY_BUFFER;
                if (validBounds.Bottom > finalParentBounds.Bottom - PARENT_BOUNDARY_BUFFER)
                    constrainedY = finalParentBounds.Bottom - PARENT_BOUNDARY_BUFFER - validBounds.Height;

                validBounds = new Rectangle(constrainedX, constrainedY, validBounds.Width, validBounds.Height);
            }

            return new Vector2(
                validBounds.X - window.CollisionBounds.X,
                validBounds.Y - window.CollisionBounds.Y
            );
        }
        private void UpdateBlockFlags(GameWindow window)
        {
            var bounds = window.CollisionBounds;
            isBlockedRight = CheckCollision(window, bounds, 1, 0);
            isBlockedLeft = CheckCollision(window, bounds, -1, 0);
            isBlockedDown = CheckCollision(window, bounds, 0, 1);
            isBlockedUp = CheckCollision(window, bounds, 0, -1);
        }
        private bool CheckCollision(GameWindow window, Rectangle bounds, int dx, int dy)
        {
            Rectangle checkBounds = new(
                bounds.X + dx,
                bounds.Y + dy,
                bounds.Width,
                bounds.Height
            );

            // CollisionService or fallback to ZoneManager
            if (collisionService != null)
            {
                var options = new CollisionOptions
                {
                    ExcludeWindow = window,
                    ExcludeChildren = true,  // 親ウィンドウが子を持つ場合、子も除外
                    CheckNoEntryZones = true,
                    CheckNoEntryBoundaries = true,
                    CheckNormalWindows = window.IsNoEntryWindow,  // 不可侵ウィンドウは通常のウィンドウともぶつかる
                    CheckButtons = window.IsNoEntryWindow,  // 不可侵ウィンドウはボタンともぶつかる
                    UseZOrderFiltering = true
                };
                return collisionService.CheckCollision(checkBounds, options);
            }
            else
            {
                // Z-order + Region考慮の不可侵ウィンドウ境界判定を含む（自分自身を除外）
                return ZoneManager.IntersectsWithAnyZone(checkBounds, window);
            }
        }
        public override void HandleWindowMessage(GameWindow window, Message m)
        {
            switch (m.Msg)
            {
                case WindowMessages.WM_LBUTTONDOWN:
                    OnMouseDown(window);
                    break;
                case WindowMessages.WM_LBUTTONUP:
                    OnMouseUp(window);
                    break;
                case WindowMessages.WM_MOUSEMOVE:
                    OnMouseMove(window);
                    break;
            }
        }
        private void StartDragging(GameWindow window)
        {
            if (isDragging) return;  // 既にドラッグ中なら開始しない
            isDragging = true;
            ResetBlockFlags();
            lastMousePos = window.PointToClient(Cursor.Position);
            lastValidPosition = window.Location;
        }

        private void StopDragging()
        {
            if (!isDragging) return;  // ドラッグ中でなければ何もしない
            isDragging = false;
            ResetBlockFlags();
            movementEffect.UpdateMovement(Vector2.Zero);
        }
        public override void DrawStrategyMark(Graphics g, Rectangle bounds, bool isHovered)
        {
            DrawMovementMark(g, bounds, isHovered ? Color.White : Color.FromArgb(128, 128, 128));
        }
        protected override Cursor GetStrategyCursor() => Cursors.SizeAll;
        private void DrawMovementMark(Graphics g, Rectangle bounds, Color color)
        {
            int markSize = 60;
            int x = bounds.X + (bounds.Width - markSize) / 2;
            int y = bounds.Y + (bounds.Height - markSize) / 2;

            using (var pen = new Pen(color, 2))
            {
                // 移動マークの描画
                DrawArrows(g, pen, x, y, markSize);
            }
        }
        private void DrawArrows(Graphics g, Pen pen, int x, int y, int size)
        {
            // 中心の十字
            g.DrawLine(pen, x + size / 2, y, x + size / 2, y + size);
            g.DrawLine(pen, x, y + size / 2, x + size, y + size / 2);

            // 矢印の描画
            DrawArrowHead(g, pen, x + size / 2, y, false);            // 上
            DrawArrowHead(g, pen, x + size / 2, y + size, true);      // 下
            DrawArrowHead(g, pen, x, y + size / 2, false, true);      // 左
            DrawArrowHead(g, pen, x + size, y + size / 2, true, true); // 右
        }
        private void DrawArrowHead(Graphics g, Pen pen, int x, int y, bool isReversed, bool isHorizontal = false)
        {
            int size = 10;
            if (isHorizontal)
            {
                g.DrawLine(pen, x, y, x + (isReversed ? -size : size), y - size);
                g.DrawLine(pen, x, y, x + (isReversed ? -size : size), y + size);
            }
            else
            {
                g.DrawLine(pen, x, y, x - size, y + (isReversed ? -size : size));
                g.DrawLine(pen, x, y, x + size, y + (isReversed ? -size : size));
            }
        }
        private void ResetBlockFlags()
        {
            isBlockedRight = false;
            isBlockedLeft = false;
            isBlockedDown = false;
            isBlockedUp = false;
        }
    }
    public class DeletableWindowStrategy : BaseWindowStrategy
    {
        public DeletableWindowStrategy(
            IInputService? inputService = null,
            IGameSettings? gameSettings = null,
            INoEntryZoneManager? noEntryZoneManager = null,
            ICollisionService? collisionService = null)
            : base(inputService, gameSettings, noEntryZoneManager, collisionService)
        {
        }

        public override void HandleInput(GameWindow window)
        {
            if (inputService?.IsKeyDown(Keys.Delete) == true)
            {
                RemoveAndClose(window);
            }
        }
        private void RemoveAndClose(GameWindow window)
        {
            // 子要素の解放
            foreach (var child in window.Children.ToList())
            {
                window.RemoveChild(child);
            }

            // 親からの削除
            window.Parent?.RemoveChild(window);

            window.Close();
            window.NotifyObservers(WindowChangeType.Deleted);
        }
        public override void DrawStrategyMark(Graphics g, Rectangle bounds, bool isHovered)
        {
            int markSize = 60;
            int x = bounds.X + (bounds.Width - markSize) / 2;
            int y = bounds.Y + (bounds.Height - markSize) / 2;

            using (var pen = new Pen(isHovered ? Color.White : Color.FromArgb(128, 128, 128), 2))
            {
                // X印を描画
                g.DrawLine(pen, x, y, x + markSize, y + markSize);
                g.DrawLine(pen, x + markSize, y, x, y + markSize);
            }
        }
        protected override Cursor GetStrategyCursor() => Cursors.Default;
    }
    public class MinimizableWindowStrategy : BaseWindowStrategy
    {
        private readonly MinimizeEffect minimizeEffect;

        public MinimizableWindowStrategy(
            IInputService? inputService = null,
            IGameSettings? gameSettings = null,
            INoEntryZoneManager? noEntryZoneManager = null,
            ICollisionService? collisionService = null)
            : base(inputService, gameSettings, noEntryZoneManager, collisionService)
        {
            minimizeEffect = new MinimizeEffect();
        }

        protected override void OnMouseDown(GameWindow window)
        {
            minimizeEffect.Activate();
            WindowEffectManager.Current.AddEffect(minimizeEffect);
            WindowEffectManager.Current.ApplyEffects(window);
        }
        public override void HandleWindowMessage(GameWindow window, Message m)
        {
            switch (m.Msg)
            {
                case WindowMessages.WM_SYSCOMMAND:
                    int command = m.WParam.ToInt32() & 0xFFF0;
                    if (command == WindowMessages.SC_MINIMIZE)
                    {
                        WindowEffectManager.Current.ApplyEffects(window);
                    }
                    else if (command == WindowMessages.SC_RESTORE)
                    {
                        // SC_RESTOREは基底クラスで共通処理
                        base.HandleWindowMessage(window, m);
                    }
                    break;
                default:
                    base.HandleWindowMessage(window, m);
                    break;
            }
        }

        public override void DrawStrategyMark(Graphics g, Rectangle bounds, bool isHovered)
        {
            var center = StrategyMarkUtility.GetMarkCenter(bounds, StrategyMarkUtility.DEFAULT_MARK_SIZE);
            var color = StrategyMarkUtility.GetMarkColor(isHovered);

            int markSize = StrategyMarkUtility.DEFAULT_MARK_SIZE;
            int barHeight = markSize / 6;

            using (var brush = new SolidBrush(color))
            {
                g.FillRectangle(brush,
                    center.X,
                    center.Y + (markSize - barHeight) / 2,
                    markSize,
                    barHeight);
            }
        }

        protected override Cursor GetStrategyCursor() => Cursors.Default;

        public override void Update(GameWindow window, float deltaTime)
        {
            // ここで必要に応じて最小化アニメーションなどの更新を行う
            base.Update(window, deltaTime);
        }

        public override void HandleResize(GameWindow window)
        {
            // 最小化/復元時のサイズ変更を適切に処理
            base.HandleResize(window);
        }
    }
    public class TextDisplayWindowStrategy : BaseWindowStrategy
    {
        private readonly string displayText;

        public TextDisplayWindowStrategy(
            string text,
            IInputService? inputService = null,
            IGameSettings? gameSettings = null,
            INoEntryZoneManager? noEntryZoneManager = null,
            ICollisionService? collisionService = null)
            : base(inputService, gameSettings, noEntryZoneManager, collisionService)
        {
            displayText = text;
        }
        public override void DrawStrategyMark(Graphics g, Rectangle bounds, bool isHovered)
        {
            // テキスト表示ウィンドウはマークを表示しない
        }
        protected override Cursor GetStrategyCursor() => Cursors.Default;

        public string GetDisplayText() => displayText;

        public override void HandleResize(GameWindow window)
        {
            base.HandleResize(window);
            // リサイズ時のテキスト再描画
            window.Invalidate();
        }
    }

    /// <summary>
    /// 不可侵ウィンドウストラテジー
    /// 静的なウィンドウで、境界が不可侵領域として機能する
    /// </summary>
    public class NoEntryWindowStrategy : BaseWindowStrategy
    {
        public override bool IsNoEntry => true;

        public NoEntryWindowStrategy(
            IInputService? inputService = null,
            IGameSettings? gameSettings = null,
            INoEntryZoneManager? noEntryZoneManager = null,
            ICollisionService? collisionService = null)
            : base(inputService, gameSettings, noEntryZoneManager, collisionService)
        {
        }

        public override void DrawStrategyMark(Graphics g, Rectangle bounds, bool isHovered)
        {
            // 不可侵ウィンドウにはマークを表示しない
        }

        protected override Cursor GetStrategyCursor() => Cursors.No;
    }

    /// <summary>
    /// リサイズ可能 + 不可侵ウィンドウストラテジー
    /// リサイズ機能を持ちつつ、境界が不可侵領域として機能する
    /// </summary>
    public class ResizableNoEntryWindowStrategy : ResizableWindowStrategy
    {
        public override bool IsNoEntry => true;

        public ResizableNoEntryWindowStrategy(
            IInputService? inputService = null,
            IGameSettings? gameSettings = null,
            INoEntryZoneManager? noEntryZoneManager = null,
            ICollisionService? collisionService = null)
            : base(inputService, gameSettings, noEntryZoneManager, collisionService)
        {
        }

        public override void DrawStrategyMark(Graphics g, Rectangle bounds, bool isHovered)
        {
            // 赤いアウトラインを描画（2px）
            using (Pen redPen = new Pen(Color.Red, 2))
            {
                g.DrawRectangle(redPen, bounds.X + 1, bounds.Y + 1,
                    bounds.Width - 2, bounds.Height - 2);
            }

            // リサイズマークも描画（親の実装を呼び出し）
            base.DrawStrategyMark(g, bounds, isHovered);
        }
    }

    /// <summary>
    /// 移動可能 + 不可侵ウィンドウストラテジー
    /// 移動機能を持ちつつ、境界が不可侵領域として機能する
    /// </summary>
    public class MovableNoEntryWindowStrategy : MovableWindowStrategy
    {
        public override bool IsNoEntry => true;

        public MovableNoEntryWindowStrategy(
            IInputService? inputService = null,
            IGameSettings? gameSettings = null,
            INoEntryZoneManager? noEntryZoneManager = null,
            ICollisionService? collisionService = null)
            : base(inputService, gameSettings, noEntryZoneManager, collisionService)
        {
        }

        public override void DrawStrategyMark(Graphics g, Rectangle bounds, bool isHovered)
        {
            // 赤いアウトラインを描画（2px）
            using (Pen redPen = new Pen(Color.Red, 2))
            {
                g.DrawRectangle(redPen, bounds.X + 1, bounds.Y + 1,
                    bounds.Width - 2, bounds.Height - 2);
            }

            // 移動マークも描画（親の実装を呼び出し）
            base.DrawStrategyMark(g, bounds, isHovered);
        }
    }

    /// <summary>
    /// 最小化可能 + 不可侵ウィンドウストラテジー
    /// 最小化機能を持ちつつ、境界が不可侵領域として機能する
    /// </summary>
    public class MinimizableNoEntryWindowStrategy : MinimizableWindowStrategy
    {
        public override bool IsNoEntry => true;

        public MinimizableNoEntryWindowStrategy(
            IInputService? inputService = null,
            IGameSettings? gameSettings = null,
            INoEntryZoneManager? noEntryZoneManager = null,
            ICollisionService? collisionService = null)
            : base(inputService, gameSettings, noEntryZoneManager, collisionService)
        {
        }

        public override void DrawStrategyMark(Graphics g, Rectangle bounds, bool isHovered)
        {
            // 赤いアウトラインを描画（2px）
            using (Pen redPen = new Pen(Color.Red, 2))
            {
                g.DrawRectangle(redPen, bounds.X + 1, bounds.Y + 1,
                    bounds.Width - 2, bounds.Height - 2);
            }

            // 最小化マークも描画（親の実装を呼び出し）
            base.DrawStrategyMark(g, bounds, isHovered);
        }
    }
}