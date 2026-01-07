using System;
using System.Drawing;
using System.Linq;
using System.Numerics;
using MultiWindowActionGame.Core;
using MultiWindowActionGame.Interfaces;
using MultiWindowActionGame.Managers;
using MultiWindowActionGame.Services;
using MultiWindowActionGame.UI;
using MultiWindowActionGame.Utilities;
using MultiWindowActionGame.Windows;

namespace MultiWindowActionGame.Player
{
    public class PlayerPhysics : IPlayerPhysics
    {
        private readonly GameSettings.PlayerSettings settings;
        private readonly IWindowManager windowManager;
        private readonly INoEntryZoneManager noEntryZoneManager;
        private readonly IInputService inputService;
        private readonly IStageManager? stageManager;  // Fallbackパターン用（null許容）

        private float verticalVelocity = 0;
        private bool isGrounded = false;

        public bool IsGrounded => isGrounded;
        public float VerticalVelocity => verticalVelocity;

        public PlayerPhysics(
            IGameSettings gameSettings,
            IWindowManager windowManager,
            INoEntryZoneManager noEntryZoneManager,
            IInputService inputService,
            IStageManager? stageManager = null)
        {
            this.settings = gameSettings?.Player ?? throw new ArgumentNullException(nameof(gameSettings));
            this.windowManager = windowManager ?? throw new ArgumentNullException(nameof(windowManager));
            this.noEntryZoneManager = noEntryZoneManager ?? throw new ArgumentNullException(nameof(noEntryZoneManager));
            this.inputService = inputService ?? throw new ArgumentNullException(nameof(inputService));
            this.stageManager = stageManager;  // null許容（Fallback: StageManager.Current）
        }

        public Vector2 CalculateMovement(float deltaTime)
        {
            Vector2 movement = Vector2.Zero;

            // TODO: 入力処理はPlayerInputHandlerに移動すべき（責務分離） - 優先度:保留
            // 判断理由（2025-01-11）:
            // - アーキテクチャ安定化優先（最近の大規模リファクタリング直後）
            // - AI制御・リプレイ機能の予定なし（YAGNI原則）
            // - ROI低（実装コスト2-4時間に対して具体的メリットなし）
            // 再検討条件: Player機能1ヶ月以上安定稼働 OR AI/リプレイ機能実装決定
            if (inputService.IsKeyDown(Keys.A) || inputService.IsKeyDown(Keys.Left))
            {
                movement.X -= settings.MovementSpeed * deltaTime;
            }
            if (inputService.IsKeyDown(Keys.D) || inputService.IsKeyDown(Keys.Right))
            {
                movement.X += settings.MovementSpeed * deltaTime;
            }

            movement.Y += verticalVelocity * deltaTime;
            return movement;
        }

        public void ApplyGravity(float deltaTime)
        {
            if (!isGrounded)
            {
                verticalVelocity += settings.Gravity * deltaTime;
            }
            else
            {
                verticalVelocity = 0;
            }
        }

        public void Jump()
        {
            verticalVelocity = -settings.JumpForce;
            isGrounded = false;
        }

        // 接地時のコールバック
        public Action<int>? OnGrounded { get; set; }

        public void CheckGrounded(Rectangle bounds, GameWindow? parentWindow, float deltaTime)
        {
            // ジャンプ中（上昇中）は接地判定をスキップ
            // リファクタリング前の "if (currentState is JumpingState) return;" と同等の処理
            if (verticalVelocity < 0)
            {
                isGrounded = false;
                return;
            }

            bool wasGrounded = isGrounded;
            isGrounded = false;

            Rectangle currentFeetBounds = new Rectangle(
                bounds.X,
                bounds.Bottom - 10,
                bounds.Width,
                settings.GroundCheckHeight
            );

            float maxVerticalStep = Math.Max(Math.Abs(verticalVelocity * deltaTime), 20f);
            Rectangle sweepBounds = new Rectangle(
                currentFeetBounds.X,
                Math.Min(currentFeetBounds.Y, currentFeetBounds.Y + (int)(verticalVelocity * deltaTime)) - 5,
                currentFeetBounds.Width,
                (int)maxVerticalStep + currentFeetBounds.Height + 10
            );

            // 不可侵領域との判定（従来のNoEntryZone）
            foreach (var zone in noEntryZoneManager.Zones)
            {
                if (bounds.Bottom >= zone.Bounds.Top &&
                    bounds.Bottom <= zone.Bounds.Top + 5 &&
                    bounds.Right > zone.Bounds.Left &&
                    bounds.Left < zone.Bounds.Right)
                {
                    SetGrounded(true, zone.Bounds.Top);
                    OnGrounded?.Invoke(zone.Bounds.Top);
                    return;
                }
            }

            // 不可侵ウィンドウ境界との判定（Z-order + Region考慮、sweepBounds方式ですり抜け防止）
            if (noEntryZoneManager.CheckAnyNoEntryBoundaryCollision(
                sweepBounds, out var collidingWindow, out var collisionRect))
            {
                // 境界の種類を判定（下辺＝地面のみ判定）
                if (collisionRect.HasValue)
                {
                    var rect = collisionRect.Value;

                    // 下辺（地面）判定: プレイヤーの足元が境界の上辺付近
                    if (bounds.Bottom >= rect.Top &&
                        bounds.Bottom <= rect.Top + 5 &&
                        bounds.Right > rect.Left &&
                        bounds.Left < rect.Right)
                    {
                        SetGrounded(true, rect.Top);
                        OnGrounded?.Invoke(rect.Top);
                        return;
                    }
                }
            }

            // ボタンとの地面判定
            var buttons = windowManager.GetAllButtons();
            foreach (var button in buttons)
            {
                if (bounds.Bottom >= button.CollisionBounds.Top &&
                    bounds.Bottom <= button.CollisionBounds.Top + 5 &&
                    bounds.Right > button.CollisionBounds.Left &&
                    bounds.Left < button.CollisionBounds.Right)
                {
                    SetGrounded(true, button.CollisionBounds.Top);
                    OnGrounded?.Invoke(button.CollisionBounds.Top);
                    return;
                }
            }

            // デスクトップアイコンとの地面判定（ステージ別制御）
            var currentStage = stageManager?.GetCurrentStage() ?? StageManager.Current.GetCurrentStage();
            if (currentStage?.EnableDesktopIcons == true)
            {
                try
                {
                    var desktopIcons = DesktopIconHelper.Instance?.GetDesktopIcons();
                    if (desktopIcons != null && desktopIcons.Count > 0)
                    {
                        // 足元の範囲と交差するアイコンのみを処理
                        var nearbyIcons = desktopIcons.Where(icon =>
                            icon.Bounds.IntersectsWith(currentFeetBounds) &&
                            bounds.Bottom >= icon.Bounds.Top &&
                            bounds.Bottom <= icon.Bounds.Top + 5 &&
                            bounds.Right > icon.Bounds.Left &&
                            bounds.Left < icon.Bounds.Right
                        ).Take(5).ToList();

                        foreach (var icon in nearbyIcons)
                        {
                            // アイコンがウィンドウに隠れていないかチェック
                            if (IsIconVisibleAtPosition(icon, currentFeetBounds))
                            {
                                SetGrounded(true, icon.Bounds.Top);
                                OnGrounded?.Invoke(icon.Bounds.Top);
                                return;
                            }
                        }
                    }
                }
                catch (Exception)
                {
                    // エラーが発生してもゲームを続行
                }
            }

            var intersectingWindows = windowManager.GetIntersectingWindows(sweepBounds)
                .OrderByDescending(w => windowManager.GetWindowZIndex(w));

            if (parentWindow == null)
            {
                // 外にいる場合の処理
                foreach (var window in intersectingWindows)
                {
                    // 不可侵ウィンドウは境界判定で処理済みなのでスキップ
                    if (window.IsNoEntryWindow)
                    {
                        continue;
                    }

                    if (bounds.Bottom >= window.CollisionBounds.Top &&
                        bounds.Bottom <= window.CollisionBounds.Top + 5)
                    {
                        bool isGroundValid = true;
                        foreach (var otherWindow in intersectingWindows)
                        {
                            if (windowManager.GetWindowZIndex(otherWindow) >
                                windowManager.GetWindowZIndex(window) &&
                                otherWindow.CollisionBounds.IntersectsWith(currentFeetBounds))
                            {
                                isGroundValid = false;
                                break;
                            }
                        }

                        if (isGroundValid)
                        {
                            SetGrounded(true, window.CollisionBounds.Top);
                            OnGrounded?.Invoke(window.CollisionBounds.Top);
                            return;
                        }
                    }
                }
            }
            else
            {
                // ウィンドウ内にいる場合の処理
                // 足元全体で最も高い床を検出（セグメント分割なし）
                int highestGroundY = int.MaxValue;
                GameWindow? groundWindow = null;

                // 不可侵ウィンドウの境界（内側の床）をチェック
                var noEntryWindows = intersectingWindows.Where(w => w.IsNoEntryWindow).ToList();
                foreach (var noEntryWindow in noEntryWindows)
                {
                    var boundaryRects = noEntryZoneManager.GetNoEntryBoundaryRectangles(noEntryWindow);

                    // 4辺すべてをチェック（内側にいる場合は下辺が床、上辺が天井、左右が壁）
                    foreach (var boundary in boundaryRects)
                    {
                        // sweepBounds方式で移動経路全体をカバーする地面検出エリア
                        Rectangle boundaryGroundArea = new Rectangle(
                            boundary.X,
                            boundary.Y - (int)maxVerticalStep - 5,
                            boundary.Width,
                            (int)maxVerticalStep + 10
                        );

                        // 足元全体が地面検出エリアと交差するかチェック
                        if (currentFeetBounds.IntersectsWith(boundaryGroundArea))
                        {
                            // Z-order + Region考慮の境界判定
                            if (noEntryZoneManager.CheckNoEntryBoundaryCollision(noEntryWindow, currentFeetBounds, out var rect) && rect.HasValue)
                            {
                                int groundY = rect.Value.Top;

                                // 画面に表示されている床の中で最も高い（Y座標が小さい）床を選択
                                if (groundY < highestGroundY)
                                {
                                    highestGroundY = groundY;
                                    groundWindow = noEntryWindow;
                                }
                            }
                        }
                    }
                }

                foreach (var window in intersectingWindows)
                {
                    // 不可侵ウィンドウは境界判定で処理済みなのでスキップ
                    if (window.IsNoEntryWindow)
                    {
                        continue;
                    }

                    // sweepBounds方式で移動経路全体をカバーする地面検出エリア
                    Rectangle windowGroundArea = new Rectangle(
                        window.AdjustedBounds.Left,
                        window.AdjustedBounds.Bottom - (int)maxVerticalStep - 5,
                        window.AdjustedBounds.Width,
                        (int)maxVerticalStep + 10
                    );

                    // 足元全体が地面検出エリアと交差するかチェック
                    if (currentFeetBounds.IntersectsWith(windowGroundArea))
                    {
                        // Z-orderチェック: このウィンドウの床が画面に表示されているか
                        bool isFloorVisible = true;
                        int groundY = window.AdjustedBounds.Bottom;

                        foreach (var otherWindow in intersectingWindows)
                        {
                            if (windowManager.GetWindowZIndex(otherWindow) >
                                windowManager.GetWindowZIndex(window))
                            {
                                // より前面のウィンドウが、この床の位置を隠していないかチェック
                                Rectangle floorArea = new Rectangle(
                                    currentFeetBounds.X,
                                    groundY - 2,  // 床位置の上下2px
                                    currentFeetBounds.Width,
                                    4
                                );

                                if (otherWindow.AdjustedBounds.Contains(floorArea) ||
                                    (otherWindow.AdjustedBounds.IntersectsWith(floorArea) &&
                                     otherWindow.AdjustedBounds.Bottom < groundY))
                                {
                                    // 床が他のウィンドウに隠されている
                                    isFloorVisible = false;
                                    break;
                                }
                            }
                        }

                        if (isFloorVisible)
                        {
                            // 画面に表示されている床の中で最も高い（Y座標が小さい）床を選択
                            if (groundY < highestGroundY)
                            {
                                highestGroundY = groundY;
                                groundWindow = window;
                            }
                        }
                    }
                }

                // 有効な地面が見つかった場合
                if (groundWindow != null && highestGroundY < int.MaxValue)
                {
                    SetGrounded(true, highestGroundY);
                    OnGrounded?.Invoke(highestGroundY);
                    return;
                }
            }

            // メインフォームとの判定
            if (Program.mainForm != null && bounds.Bottom >= Program.mainForm.ClientSize.Height)
            {
                SetGrounded(true, Program.mainForm.ClientSize.Height);
                OnGrounded?.Invoke(Program.mainForm.ClientSize.Height);
            }
        }

        public void SetGrounded(bool grounded, int? groundY = null)
        {
            isGrounded = grounded;
            // 上昇中（verticalVelocity < 0）の場合は速度をリセットしない
            // ジャンプ直後に地面判定が残っていても、ジャンプ速度を維持する
            if (grounded && verticalVelocity >= 0)
            {
                verticalVelocity = 0;
            }
            // groundYの処理はPlayerFormで位置調整として実装
        }

        public void ResetPhysics()
        {
            verticalVelocity = 0;
            isGrounded = false;
        }

        public void SetVerticalVelocity(float velocity)
        {
            verticalVelocity = velocity;
        }

        public Rectangle GetGroundCheckArea(Rectangle bounds)
        {
            return new Rectangle(
                bounds.X,
                bounds.Bottom - settings.GroundCheckHeight,
                bounds.Width,
                settings.GroundCheckHeight
            );
        }

        private bool IsIconVisibleAtPosition(DesktopIcon icon, Rectangle checkBounds)
        {
            try
            {
                // パフォーマンス最適化：アイコンとcheckBoundsの交差部分のみをチェック
                var intersectionBounds = Rectangle.Intersect(icon.Bounds, checkBounds);
                if (intersectionBounds.IsEmpty)
                {
                    return true;
                }

                // より効率的な方法：交差するウィンドウのみをチェック
                var intersectingWindows = windowManager.GetIntersectingWindows(intersectionBounds);

                // 最低限のチェック：実際に重なっているウィンドウがあるかを確認
                foreach (var window in intersectingWindows)
                {
                    // ウィンドウが確実にアイコンを覆っているかチェック
                    if (window.CollisionBounds.Contains(intersectionBounds) ||
                        (window.CollisionBounds.IntersectsWith(icon.Bounds) &&
                         window.CollisionBounds.IntersectsWith(checkBounds)))
                    {
                        // ウィンドウがアイコンを隠している
                        return false;
                    }
                }

                return true;
            }
            catch (Exception)
            {
                // エラーが発生した場合は安全側に倒して見えているとみなす
                return true;
            }
        }

        /// <summary>
        /// 横方向の移動に対するスイープ衝突判定
        /// プレイヤーが細い場合に不可侵ウィンドウを貫通するのを防ぐ
        /// </summary>
        /// <param name="bounds">現在のプレイヤー境界</param>
        /// <param name="movement">移動ベクトル</param>
        /// <returns>調整された移動ベクトル</returns>
        public Vector2 CheckHorizontalCollision(Rectangle bounds, Vector2 movement)
        {
            // 移動がない場合は早期リターン
            if (Math.Abs(movement.X) < 0.1f)
            {
                return movement;
            }

            Vector2 adjustedMovement = movement;
            float horizontalMovement = movement.X;
            int moveDirection = Math.Sign(horizontalMovement);

            // 横方向のスイープ境界を作成（現在位置から目標位置までをカバー）
            int sweepLeft = moveDirection > 0 ? bounds.Left : bounds.Left + (int)horizontalMovement;
            int sweepRight = moveDirection > 0 ? bounds.Right + (int)horizontalMovement : bounds.Right;
            int sweepWidth = sweepRight - sweepLeft;

            Rectangle horizontalSweepBounds = new Rectangle(
                sweepLeft,
                bounds.Top,
                sweepWidth,
                bounds.Height
            );

            // 1. 静的NoEntryZoneとの衝突チェック
            var zones = noEntryZoneManager.Zones;
            foreach (var zone in zones)
            {
                if (horizontalSweepBounds.IntersectsWith(zone.Bounds))
                {
                    // 衝突を検出 - 移動を制限
                    if (moveDirection > 0) // 右移動
                    {
                        int maxX = zone.Bounds.Left - bounds.Width;
                        adjustedMovement.X = Math.Min(adjustedMovement.X, maxX - bounds.X);
                    }
                    else // 左移動
                    {
                        int minX = zone.Bounds.Right;
                        adjustedMovement.X = Math.Max(adjustedMovement.X, minX - bounds.X);
                    }
                }
            }

            // 2. 不可侵ウィンドウ境界との衝突チェック（Z-order + Region考慮）
            var intersectingWindows = windowManager.GetIntersectingWindows(horizontalSweepBounds)
                .Where(w => w.IsNoEntryWindow)
                .OrderByDescending(w => windowManager.GetWindowZIndex(w))
                .ToList();

            foreach (var noEntryWindow in intersectingWindows)
            {
                // Z-order + Region考慮の境界判定
                if (noEntryZoneManager.CheckNoEntryBoundaryCollision(noEntryWindow, horizontalSweepBounds, out var collisionRect) && collisionRect.HasValue)
                {
                    Rectangle boundary = collisionRect.Value;

                    // 境界の左右のエッジを判定（プレイヤーサイズを考慮）
                    // 境界幅（5px）+ プレイヤーの半分 + バッファ（2px）で動的に許容値を計算
                    int edgeTolerance = CollisionFilter.NOENTRY_BOUNDARY_WIDTH + (bounds.Width / 2) + 2;
                    bool isLeftEdge = Math.Abs(boundary.Left - noEntryWindow.CollisionBounds.Left) < edgeTolerance;
                    bool isRightEdge = Math.Abs(boundary.Right - noEntryWindow.CollisionBounds.Right) < edgeTolerance;

                    // プレイヤーが不可侵ウィンドウの内側にいるか外側にいるかを判定
                    bool isInsideWindow = bounds.Left >= noEntryWindow.CollisionBounds.Left &&
                                          bounds.Right <= noEntryWindow.CollisionBounds.Right &&
                                          bounds.Top >= noEntryWindow.CollisionBounds.Top &&
                                          bounds.Bottom <= noEntryWindow.CollisionBounds.Bottom;

                    if (isInsideWindow)
                    {
                        // 内側にいる場合: 内側から外側への移動を制限
                        if (moveDirection > 0 && isRightEdge) // 右移動 → 右壁（内側）
                        {
                            // プレイヤーの右端が境界の内側に達する直前で停止
                            int maxX = boundary.Left - bounds.Width;
                            adjustedMovement.X = Math.Min(adjustedMovement.X, maxX - bounds.X);
                        }
                        else if (moveDirection < 0 && isLeftEdge) // 左移動 → 左壁（内側）
                        {
                            // プレイヤーの左端が境界の内側に達する直前で停止
                            int minX = boundary.Right;
                            adjustedMovement.X = Math.Max(adjustedMovement.X, minX - bounds.X);
                        }
                    }
                    else
                    {
                        // 外側にいる場合: 外側から内側への移動を制限（従来の動作）
                        if (moveDirection > 0 && isLeftEdge) // 右移動 → 左壁（外側）
                        {
                            // プレイヤーの右端が境界の左端に達する直前で停止
                            int maxX = boundary.Left - bounds.Width;
                            adjustedMovement.X = Math.Min(adjustedMovement.X, maxX - bounds.X);
                        }
                        else if (moveDirection < 0 && isRightEdge) // 左移動 → 右壁（外側）
                        {
                            // プレイヤーの左端が境界の右端に達する直前で停止
                            int minX = boundary.Right;
                            adjustedMovement.X = Math.Max(adjustedMovement.X, minX - bounds.X);
                        }
                    }
                }
            }

            // ステップ3削除 - プレイヤーは通常ウィンドウを貫通できる（仕様）

            return adjustedMovement;
        }

        /// <summary>
        /// 垂直方向の移動に対するスイープ衝突判定
        /// プレイヤーがジャンプ時に不可侵ウィンドウの天井を貫通するのを防ぐ
        /// </summary>
        /// <param name="bounds">現在のプレイヤー境界</param>
        /// <param name="movement">移動ベクトル</param>
        /// <returns>調整された移動ベクトル</returns>
        public Vector2 CheckVerticalCollision(Rectangle bounds, Vector2 movement)
        {
            // 移動がない場合は早期リターン
            if (Math.Abs(movement.Y) < 0.1f)
            {
                return movement;
            }

            Vector2 adjustedMovement = movement;
            float verticalMovement = movement.Y;
            int moveDirection = Math.Sign(verticalMovement);  // 1=下降, -1=上昇

            // 垂直方向のスイープ境界を作成（現在位置から目標位置までをカバー）
            int sweepTop = moveDirection > 0 ? bounds.Top : bounds.Top + (int)verticalMovement;
            int sweepBottom = moveDirection > 0 ? bounds.Bottom + (int)verticalMovement : bounds.Bottom;
            int sweepHeight = sweepBottom - sweepTop;

            Rectangle verticalSweepBounds = new Rectangle(
                bounds.Left,
                sweepTop,
                bounds.Width,
                sweepHeight
            );

            // 1. 静的NoEntryZoneとの衝突チェック
            var zones = noEntryZoneManager.Zones;
            foreach (var zone in zones)
            {
                if (verticalSweepBounds.IntersectsWith(zone.Bounds))
                {
                    // 衝突を検出 - 移動を制限
                    if (moveDirection > 0) // 下移動（落下）
                    {
                        int maxY = zone.Bounds.Top - bounds.Height;
                        adjustedMovement.Y = Math.Min(adjustedMovement.Y, maxY - bounds.Y);
                    }
                    else // 上移動（ジャンプ）
                    {
                        int minY = zone.Bounds.Bottom;
                        adjustedMovement.Y = Math.Max(adjustedMovement.Y, minY - bounds.Y);
                    }
                }
            }

            // 2. 不可侵ウィンドウ境界との衝突チェック（Z-order + Region考慮）
            var intersectingWindows = windowManager.GetIntersectingWindows(verticalSweepBounds)
                .Where(w => w.IsNoEntryWindow)
                .OrderByDescending(w => windowManager.GetWindowZIndex(w))
                .ToList();

            foreach (var noEntryWindow in intersectingWindows)
            {
                // Z-order + Region考慮の境界判定
                if (noEntryZoneManager.CheckNoEntryBoundaryCollision(noEntryWindow, verticalSweepBounds, out var collisionRect) && collisionRect.HasValue)
                {
                    Rectangle boundary = collisionRect.Value;

                    // 境界の上下のエッジを判定（プレイヤーサイズを考慮）
                    // 境界幅（5px）+ プレイヤーの半分 + バッファ（2px）で動的に許容値を計算
                    int edgeTolerance = CollisionFilter.NOENTRY_BOUNDARY_WIDTH + (bounds.Height / 2) + 2;
                    bool isTopEdge = Math.Abs(boundary.Top - noEntryWindow.CollisionBounds.Top) < edgeTolerance;
                    bool isBottomEdge = Math.Abs(boundary.Bottom - noEntryWindow.CollisionBounds.Bottom) < edgeTolerance;

                    // プレイヤーが不可侵ウィンドウの内側にいるか外側にいるかを判定
                    bool isInsideWindow = bounds.Left >= noEntryWindow.CollisionBounds.Left &&
                                          bounds.Right <= noEntryWindow.CollisionBounds.Right &&
                                          bounds.Top >= noEntryWindow.CollisionBounds.Top &&
                                          bounds.Bottom <= noEntryWindow.CollisionBounds.Bottom;

                    if (isInsideWindow)
                    {
                        // 内側にいる場合: 内側から外側への移動を制限
                        if (moveDirection > 0 && isBottomEdge) // 下移動 → 下壁（内側）
                        {
                            // プレイヤーの下端が境界の内側に達する直前で停止
                            int maxY = boundary.Top - bounds.Height;
                            adjustedMovement.Y = Math.Min(adjustedMovement.Y, maxY - bounds.Y);
                        }
                        else if (moveDirection < 0 && isTopEdge) // 上移動 → 上壁（内側）
                        {
                            // プレイヤーの上端が境界の内側に達する直前で停止
                            int minY = boundary.Bottom;
                            adjustedMovement.Y = Math.Max(adjustedMovement.Y, minY - bounds.Y);
                        }
                    }
                    else
                    {
                        // 外側にいる場合: 外側から内側への移動を制限（従来の動作）
                        if (moveDirection > 0 && isTopEdge) // 下移動 → 上壁（外側）
                        {
                            // プレイヤーの下端が境界の上端に達する直前で停止
                            int maxY = boundary.Top - bounds.Height;
                            adjustedMovement.Y = Math.Min(adjustedMovement.Y, maxY - bounds.Y);
                        }
                        else if (moveDirection < 0 && isBottomEdge) // 上移動 → 下壁（外側）
                        {
                            // プレイヤーの上端が境界の下端に達する直前で停止
                            int minY = boundary.Bottom;
                            adjustedMovement.Y = Math.Max(adjustedMovement.Y, minY - bounds.Y);
                        }
                    }
                }
            }

            // ステップ3削除 - プレイヤーは通常ウィンドウを貫通できる（仕様）

            return adjustedMovement;
        }
    }
}
