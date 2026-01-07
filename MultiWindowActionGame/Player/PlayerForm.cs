using System.Drawing.Drawing2D;
using System.Numerics;
using System.Runtime.InteropServices;
using MultiWindowActionGame.Utilities;
using MultiWindowActionGame.Extensions;
using MultiWindowActionGame.Windows;
using MultiWindowActionGame.Debug;
using MultiWindowActionGame.Rendering;
using MultiWindowActionGame.Effects;
using MultiWindowActionGame.Core;
using MultiWindowActionGame.Managers;
using MultiWindowActionGame.Interfaces;
using MultiWindowActionGame.Animation;

namespace MultiWindowActionGame.Player
{
    public class PlayerForm : BaseEffectTarget
    {
        private GameSettings.PlayerSettings settings;
        private GameWindow? lastValidParent;
        private DateTime? minimizedTime;
        private Size originalSize;
        private readonly IGameSettings? gameSettings;
        private readonly IWindowManager? windowManager;
        private readonly INoEntryZoneManager? noEntryZoneManager;
        private readonly IPlayerInputHandler? inputHandler;
        private readonly IPlayerPhysics? physics;
        private IPlayerWindowInteraction? windowInteraction;
        private readonly IPlayerStateMachine? stateMachine;
        private readonly IStageManager? stageManager;  // Fallbackパターン用（null許容）

        // アニメーションシステム
        private PlayerAnimation animation = new PlayerAnimation();
        private string facing = "right"; // プレイヤーの向き

        // 領域の定義
        private Rectangle collisionBounds;  // 当たり判定領域
        private Rectangle renderBounds;     // 描画領域
        private Rectangle displayBounds;    // 表示領域

        // 当たり判定領域を基準とした各領域の比率
        private const float RENDER_RATIO = 1.0f;   // 描画領域は当たり判定と同じ大きさ
        private const float DISPLAY_RATIO = 2.0f;  // 表示領域は当たり判定の2.0倍

        public GameWindow? LastValidParent => lastValidParent;
        public bool IsGrounded => physics?.IsGrounded ?? false;
        public float VerticalVelocity => physics?.VerticalVelocity ?? 0;
        public TimeSpan TimeSinceMinimized =>
            minimizedTime.HasValue ? DateTime.Now - minimizedTime.Value : TimeSpan.MaxValue;
        public IPlayerStateMachine? StateMachine => stateMachine;

        // 当たり判定領域を公開（読み取り専用）
        public Rectangle CollisionBounds => collisionBounds;
        // 描画領域を公開（読み取り専用）
        public Rectangle RenderBounds => renderBounds;
        // 表示領域を公開（読み取り専用）
        public Rectangle DisplayBounds => displayBounds;
        // 描画領域のローカル座標を公開（Form座標系）
        public Rectangle LocalRenderBounds => new Rectangle(
            renderBounds.X - this.Location.X,
            renderBounds.Y - this.Location.Y,
            renderBounds.Width,
            renderBounds.Height
        );
        // Boundsプロパティをオーバーライドして当たり判定領域を返す
        public override Rectangle Bounds => collisionBounds;
        // DI対応コンストラクタ
        public PlayerForm(
            Point startPosition,
            IGameSettings? gameSettings = null,
            IWindowManager? windowManager = null,
            INoEntryZoneManager? noEntryZoneManager = null,
            IPlayerInputHandler? inputHandler = null,
            IPlayerPhysics? physics = null,
            IPlayerWindowInteraction? windowInteraction = null,
            IPlayerStateMachine? stateMachine = null,
            IStageManager? stageManager = null)
        {
            this.gameSettings = gameSettings;
            this.windowManager = windowManager;
            this.noEntryZoneManager = noEntryZoneManager;
            this.inputHandler = inputHandler;
            this.physics = physics;
            this.windowInteraction = windowInteraction;
            this.stateMachine = stateMachine;
            this.stageManager = stageManager;  // null許容（Fallback: StageManager.Current）

            var gameSettingsSafe = (gameSettings ?? GameSettings.Current);
            settings = gameSettingsSafe.Player;
            gameSettingsSafe.SettingsChanged += OnSettingsChanged;

            // 当たり判定領域を初期化
            collisionBounds = new Rectangle(startPosition, settings.DefaultSize);
            // 描画領域を当たり判定の1.2倍で計算
            renderBounds = CalculateRenderBounds(collisionBounds);
            // 表示領域を当たり判定の1.8倍で計算
            displayBounds = CalculateDisplayBounds(collisionBounds);
            // bounds（継承元のフィールド）に表示領域を設定
            bounds = displayBounds;

            originalSize = settings.DefaultSize;
            InitializeForm();
            this.Load += PlayerForm_Load;

            // PlayerPhysicsのOnGroundedコールバックを設定
            if (physics != null)
            {
                physics.OnGrounded = HandleGrounding;
            }

            (windowManager ?? WindowManager.Current).RegisterFormOrder(this, MultiWindowActionGame.Managers.ZOrderPriority.Player);
        }
        // 当たり判定領域から描画領域を計算（当たり判定の1.2倍）
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
        // 当たり判定領域から表示領域を計算（当たり判定の1.8倍）
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
        public Region GetMovableRegion() => GetValidRegion();
        public Rectangle GetGroundCheckArea() => new Rectangle(
            collisionBounds.X,
            collisionBounds.Bottom - settings.GroundCheckHeight,
            collisionBounds.Width,
            settings.GroundCheckHeight
        );
        private void OnSettingsChanged(object? sender, GameSettings.SettingsChangedEventArgs e)
        {
            if (e.Type == GameSettings.SettingType.Player || e.Type == GameSettings.SettingType.All)
            {
                // 設定を再読み込み
                settings = (gameSettings ?? GameSettings.Current).Player;

                if (this.InvokeRequired)
                {
                    this.Invoke(UpdatePlayerProperties);
                }
                else
                {
                    UpdatePlayerProperties();
                }
            }
        }
        private void UpdatePlayerProperties()
        {
            if (collisionBounds.Size != settings.DefaultSize)
            {
                ResetSize(settings.DefaultSize);
            }
        }
        private void InitializeForm()
        {
            this.FormBorderStyle = FormBorderStyle.None;
            this.StartPosition = FormStartPosition.Manual;
            this.Location = bounds.Location;
            this.Size = bounds.Size;
            this.ShowInTaskbar = true;
            this.BackColor = Color.Magenta;
            this.TransparencyKey = Color.Magenta;
            this.MinimumSize = new Size(5, 5);
            this.Text = "Player";
            this.Visible = false; // 初期状態で非表示に設定

            this.SetStyle(
                ControlStyles.OptimizedDoubleBuffer |
                ControlStyles.AllPaintingInWmPaint |
                ControlStyles.UserPaint |
                ControlStyles.ResizeRedraw,
                true
            );

            this.Paint += OnPaint;
        }
        private void PlayerForm_Load(object? sender, EventArgs e)
        {
            SetWindowProperties();
            (windowManager ?? WindowManager.Current).UpdateFormZOrder(this, MultiWindowActionGame.Managers.ZOrderPriority.Player);
        }
        private void SetWindowProperties()
        {
            int exStyle = WindowMessages.GetWindowLong(this.Handle, WindowMessages.GWL_EXSTYLE);
            exStyle |= WindowMessages.WS_EX_LAYERED;
            // WS_EX_TRANSPARENTを削除: サイズ変更時の黒いチラつき問題を解決
            // TransparencyKeyによる透明領域は自動的にマウスイベントを透過する
            WindowMessages.SetWindowLong(this.Handle, WindowMessages.GWL_EXSTYLE, exStyle);
        }
        public override Task UpdateAsync(float deltaTime)
        {
            if (IsMinimized) return Task.CompletedTask;

            // アニメーション状態の更新（前回の状態を保存）
            bool wasGrounded = physics?.IsGrounded ?? false;

            // 入力処理（ジャンプ、向きの更新）
            inputHandler?.HandleInput(physics, () =>
            {
                if (stateMachine != null)
                {
                    stateMachine.ChangeState(PlayerStateType.Jumping);
                }
                animation.StartJump();
            });
            facing = inputHandler?.UpdateFacing(facing) ?? facing;

            // 状態更新
            if (stateMachine != null)
            {
                // PlayerStateMachine使用時: 状態遷移ロジック
                var currentStateType = stateMachine.CurrentStateType;

                if (currentStateType == PlayerStateType.Jumping)
                {
                    if (VerticalVelocity > 0)
                    {
                        stateMachine.ChangeState(PlayerStateType.Falling);
                    }
                    else if (IsGrounded)
                    {
                        stateMachine.ChangeState(PlayerStateType.Normal);
                    }
                }
                else if (currentStateType == PlayerStateType.Falling)
                {
                    if (IsGrounded)
                    {
                        stateMachine.ChangeState(PlayerStateType.Normal);
                    }
                }
                else if (currentStateType == PlayerStateType.Normal)
                {
                    if (!IsGrounded)
                    {
                        stateMachine.ChangeState(PlayerStateType.Falling);
                    }
                }

                stateMachine.Update(deltaTime);
            }

            // 移動前の水平速度を計算（アニメーション用）
            float dx = 0;
            if (inputHandler?.IsMovingLeft() ?? false)
            {
                dx = -settings.MovementSpeed * deltaTime;
            }
            if (inputHandler?.IsMovingRight() ?? false)
            {
                dx = settings.MovementSpeed * deltaTime;
            }

            HandleMovement(deltaTime);
            physics?.CheckGrounded(collisionBounds, Parent, deltaTime);
            physics?.ApplyGravity(deltaTime);

            // アニメーション状態の更新
            animation.UpdateAnimationState(IsGrounded, wasGrounded, dx);

            // 移動可能領域の上端までの距離を計算
            float distanceToTop = CalculateDistanceToMovableBoundsTop();

            // アニメーションフレームの更新（上端距離を渡す）
            animation.UpdateAnimation(deltaTime, distanceToTop, collisionBounds.Height);

            // 描画を更新
            this.Invalidate();

            return Task.CompletedTask;
        }
        /// <summary>
        /// 移動可能領域の上端までの距離を計算（全体のウィンドウを考慮）
        /// </summary>
        private float CalculateDistanceToMovableBoundsTop()
        {
            // プレイヤーの上方向にある領域を定義（プレイヤーの幅を維持）
            Rectangle upwardArea = new Rectangle(
                collisionBounds.X,
                0, // 画面の一番上から
                collisionBounds.Width,
                collisionBounds.Top // プレイヤーの上端まで
            );

            // 上方向と交差する全てのウィンドウを取得
            var windowMgr = windowManager ?? WindowManager.Current;
            var intersectingWindows = windowMgr.GetIntersectingWindows(upwardArea);

            // 各ウィンドウのAdjustedBoundsの下端を収集
            int closestBottom = int.MinValue;
            foreach (var window in intersectingWindows)
            {
                int windowBottom = window.AdjustedBounds.Bottom;
                // プレイヤーの上端より上にあるウィンドウのみ
                if (windowBottom <= collisionBounds.Top)
                {
                    closestBottom = Math.Max(closestBottom, windowBottom);
                }
            }

            // 最も近い上端までの距離を計算
            if (closestBottom == int.MinValue)
            {
                return float.MaxValue; // 上に何もない
            }

            float distance = Math.Max(0, collisionBounds.Top - closestBottom);
            return distance;
        }
        // 物理演算と入力処理は PlayerPhysics/PlayerInputHandler に委譲済み
        // ウィンドウ処理は PlayerWindowInteraction に委譲済み（フォールバック処理あり）
        private void HandleMovement(float deltaTime)
        {
            Vector2 movement = physics?.CalculateMovement(deltaTime) ?? Vector2.Zero;

            // 横方向のスイープ衝突判定を適用（プレイヤーが細い場合の貫通を防ぐ）
            if (physics != null && Math.Abs(movement.X) > 0.1f)
            {
                movement = physics.CheckHorizontalCollision(collisionBounds, movement);
            }

            // 垂直方向のスイープ衝突判定を適用（ジャンプ時の天井貫通を防ぐ）
            if (physics != null && Math.Abs(movement.Y) > 0.1f)
            {
                Vector2 originalMovement = movement;
                movement = physics.CheckVerticalCollision(collisionBounds, movement);

                // 天井衝突検出: 上昇中に移動が制限された場合
                if (originalMovement.Y < 0 && movement.Y > originalMovement.Y)
                {
                    // 天井にぶつかった - 速度をリセットして落下開始
                    physics.SetVerticalVelocity(0);
                    animation.ResetScale();
                }
            }

            // 当たり判定領域で移動を計算
            Rectangle proposedCollision = new Rectangle(
                collisionBounds.X + (int)movement.X,
                collisionBounds.Y + (int)movement.Y,
                collisionBounds.Width,
                collisionBounds.Height
            );

            // ウィンドウ処理をPlayerWindowInteractionに委譲（DIで必ず注入される）
            // 移動可能領域チェックと調整
            if (!windowInteraction!.IsValidMove(proposedCollision, Parent))
            {
                proposedCollision = windowInteraction.AdjustMovement(collisionBounds, proposedCollision, Parent, () =>
                {
                    // 天井に当たった場合の処理（リファクタリング前の動作を復元）
                    animation.ResetScale();
                    physics?.SetVerticalVelocity(0);
                });
            }

            // ウィンドウ衝突処理（親がnullの場合のみ）
            if (Parent == null)
            {
                var (adjustedCollision, hitCeiling) = windowInteraction.HandleWindowCollisions(proposedCollision, collisionBounds);
                proposedCollision = adjustedCollision;

                // 天井衝突時は速度をリセット
                if (hitCeiling)
                {
                    physics?.SetVerticalVelocity(0);
                    animation.ResetScale();
                }
            }

            // ボタンとの衝突判定（ウィンドウ内外に関係なく実行）
            proposedCollision = HandleButtonCollisions(proposedCollision);

            // デスクトップアイコンとの衝突判定（ステージ別制御）
            var currentStage = stageManager?.GetCurrentStage() ?? StageManager.Current.GetCurrentStage();
            if (currentStage?.EnableDesktopIcons == true)
            {
                proposedCollision = HandleDesktopIconCollisions(proposedCollision);
            }

            // ウィンドウ遷移処理
            windowInteraction.HandleWindowTransitions(proposedCollision, collisionBounds);
            
            UpdatePosition(proposedCollision.Location);
        }
        private void OnEnterWindow(GameWindow window)
        {
            // 新しいウィンドウに入った時のサイズを基準として保存
            originalSize = collisionBounds.Size;
        }
        private Rectangle HandleWindowCollisions(Rectangle newCollision)
        {
            var adjustedCollision = newCollision;
            adjustedCollision = (noEntryZoneManager ?? NoEntryZoneManager.Current).GetValidPosition(collisionBounds, adjustedCollision);

            var intersectingWindows = (windowManager ?? WindowManager.Current).GetIntersectingWindows(adjustedCollision);
            foreach (var window in intersectingWindows)
            {
                Rectangle windowBounds = window.CollisionBounds;
                if (collisionBounds.Bottom <= windowBounds.Top && adjustedCollision.Bottom > windowBounds.Top)
                {
                    adjustedCollision.Y = windowBounds.Top - adjustedCollision.Height;
                }
                else if (collisionBounds.Top >= windowBounds.Bottom && adjustedCollision.Top < windowBounds.Bottom)
                {
                    adjustedCollision.Y = windowBounds.Bottom;
                }
                else if (collisionBounds.Right <= windowBounds.Left && adjustedCollision.Right > windowBounds.Left)
                {
                    adjustedCollision.X = windowBounds.Left - adjustedCollision.Width;
                }
                else if (collisionBounds.Left >= windowBounds.Right && adjustedCollision.Left < windowBounds.Right)
                {
                    adjustedCollision.X = windowBounds.Right;
                }
            }

            return adjustedCollision;
        }
        private Rectangle HandleButtonCollisions(Rectangle newCollision)
        {
            var adjustedCollision = newCollision;

            // プレイヤーの移動範囲と交差するボタンのみを対象とする
            var buttons = (windowManager ?? WindowManager.Current).GetAllButtons();
            var intersectingButtons = buttons.Where(button =>
                button.CollisionBounds.IntersectsWith(adjustedCollision) ||
                button.CollisionBounds.IntersectsWith(collisionBounds)
            ).ToList();

            foreach (var button in intersectingButtons)
            {
                Rectangle buttonBounds = button.CollisionBounds;
                if (collisionBounds.Bottom <= buttonBounds.Top && adjustedCollision.Bottom > buttonBounds.Top)
                {
                    // ボタンの上面に衝突（上から降りてきた）
                    adjustedCollision.Y = buttonBounds.Top - adjustedCollision.Height;
                }
                else if (collisionBounds.Top >= buttonBounds.Bottom && adjustedCollision.Top < buttonBounds.Bottom)
                {
                    // ボタンの底面に衝突（下からジャンプしてぶつかった）
                    adjustedCollision.Y = buttonBounds.Bottom;
                    // 垂直速度をゼロにして、すぐに降下させる
                    physics?.SetVerticalVelocity(0);
                    animation.ResetScale();
                }
                else if (collisionBounds.Right <= buttonBounds.Left && adjustedCollision.Right > buttonBounds.Left)
                {
                    adjustedCollision.X = buttonBounds.Left - adjustedCollision.Width;
                }
                else if (collisionBounds.Left >= buttonBounds.Right && adjustedCollision.Left < buttonBounds.Right)
                {
                    adjustedCollision.X = buttonBounds.Right;
                }
            }

            return adjustedCollision;
        }
        private Rectangle HandleDesktopIconCollisions(Rectangle newCollision)
        {
            var adjustedCollision = newCollision;

            try
            {
                // デスクトップアイコンマネージャーからアイコン情報を取得
                var desktopIcons = DesktopIconHelper.Instance?.GetDesktopIcons();
                if (desktopIcons == null || desktopIcons.Count == 0)
                {
                    return adjustedCollision; // アイコンがない場合は早期リターン
                }

                // プレイヤーの移動範囲と交差するアイコンのみを対象とし、Z-orderも考慮
                var intersectingIcons = desktopIcons.Where(icon =>
                    (icon.Bounds.IntersectsWith(adjustedCollision) || icon.Bounds.IntersectsWith(collisionBounds)) &&
                    IsIconVisibleAtPosition(icon, adjustedCollision)
                ).Take(10).ToList(); // 最大10個まで制限してパフォーマンス向上

                foreach (var icon in intersectingIcons)
                {
                    Rectangle iconBounds = icon.Bounds;

                    if (collisionBounds.Bottom <= iconBounds.Top && adjustedCollision.Bottom > iconBounds.Top)
                    {
                        adjustedCollision.Y = iconBounds.Top - adjustedCollision.Height;
                    }
                    else if (collisionBounds.Top >= iconBounds.Bottom && adjustedCollision.Top < iconBounds.Bottom)
                    {
                        adjustedCollision.Y = iconBounds.Bottom;
                    }
                    else if (collisionBounds.Right <= iconBounds.Left && adjustedCollision.Right > iconBounds.Left)
                    {
                        adjustedCollision.X = iconBounds.Left - adjustedCollision.Width;
                    }
                    else if (collisionBounds.Left >= iconBounds.Right && adjustedCollision.Left < iconBounds.Right)
                    {
                        adjustedCollision.X = iconBounds.Right;
                    }
                }

                return adjustedCollision;
            }
            catch (Exception ex)
            {
                // エラーが発生した場合は元の位置を返す
                return adjustedCollision;
            }
        }
        /// <summary>
        /// 指定位置でアイコンがウィンドウに隠れずに見えているかをチェック（最適化版）
        /// </summary>
        /// <param name="icon">チェック対象のアイコン</param>
        /// <param name="checkBounds">チェックする領域</param>
        /// <returns>アイコンが見えている場合はtrue</returns>
        private bool IsIconVisibleAtPosition(DesktopIcon icon, Rectangle checkBounds)
        {
            try
            {
                // パフォーマンス最適化：アイコンとcheckBoundsの交差部分のみをチェック
                var intersectionBounds = Rectangle.Intersect(icon.Bounds, checkBounds);
                if (intersectionBounds.IsEmpty)
                {
                    return true; // 交差していない場合は隠蔽の心配なし
                }

                // より効率的な方法：交差するウィンドウのみをチェック
                var intersectingWindows = (windowManager ?? WindowManager.Current).GetIntersectingWindows(intersectionBounds);

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

                return true; // アイコンは見えている
            }
            catch (Exception ex)
            {
                // エラーが発生した場合は安全側に倒して見えているとみなす
                return true;
            }
        }
        private void HandleGrounding(int groundY)
        {
            // 位置調整のみをPlayerFormで実施
            collisionBounds.Y = groundY - collisionBounds.Height;
            renderBounds = CalculateRenderBounds(collisionBounds);
            displayBounds = CalculateDisplayBounds(collisionBounds);
            bounds = displayBounds;
            this.Location = bounds.Location;

            // 接地時に通常状態に戻す
            if (stateMachine != null)
            {
                if (stateMachine.CurrentStateType != PlayerStateType.Normal)
                {
                    stateMachine.ChangeState(PlayerStateType.Normal);
                }
            }
        }
        private void OnPaint(object? sender, PaintEventArgs e)
        {
            e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;

            // 描画領域のローカル座標を計算（表示領域内での位置）
            Rectangle localRenderRect = new Rectangle(
                renderBounds.X - bounds.X,
                renderBounds.Y - bounds.Y,
                renderBounds.Width,
                renderBounds.Height
            );

            // アニメーションを適用したスケールを計算
            float visualWidth = localRenderRect.Width * animation.ScaleX;
            float visualHeight = localRenderRect.Height * animation.ScaleY;

            // 中心点を計算（足元固定）
            float centerX = localRenderRect.X + localRenderRect.Width / 2f;
            float centerY = localRenderRect.Y + localRenderRect.Height; // 足元

            // アニメーション適用後の矩形
            RectangleF animatedRect = new RectangleF(
                centerX - visualWidth / 2f,
                centerY - visualHeight,
                visualWidth,
                visualHeight
            );

            // アニメーション領域のみを表示するためにRegionを設定
            // アウトラインの太さ分を考慮して拡張
            using (var path = new System.Drawing.Drawing2D.GraphicsPath())
            {
                const float outlineWidth = 5.0f;
                var regionRect = new RectangleF(
                    animatedRect.X - outlineWidth / 2,
                    animatedRect.Y - outlineWidth / 2,
                    animatedRect.Width + outlineWidth,
                    animatedRect.Height + outlineWidth
                );
                path.AddRectangle(regionRect);
                this.Region?.Dispose();
                this.Region = new Region(path);
            }

            // フォーム全体を透明色で塗りつぶし
            e.Graphics.Clear(Color.Magenta);

            // キューブ本体の描画
            using (var brush = new SolidBrush(Color.FromArgb(174, 214, 241))) // #aed6f1
            {
                e.Graphics.FillRectangle(brush, animatedRect);
            }

            // キューブの輪郭を描画
            using (var pen = new Pen(Color.FromArgb(52, 73, 94), 5)) // #34495e, 太さ5px（親アウトラインと同じ）
            {
                e.Graphics.DrawRectangle(pen, animatedRect.X, animatedRect.Y, animatedRect.Width, animatedRect.Height);
            }

            // 目を描画
            float eyeY = centerY - visualHeight * 0.6f;
            float eyeRadius = 3;
            float eyeOffset = visualWidth * 0.2f;

            using (var brush = new SolidBrush(Color.FromArgb(52, 73, 94))) // #34495e
            {
                if (facing == "right")
                {
                    e.Graphics.FillEllipse(brush,
                        centerX + eyeOffset - eyeRadius,
                        eyeY - eyeRadius,
                        eyeRadius * 2,
                        eyeRadius * 2);
                }
                else
                {
                    e.Graphics.FillEllipse(brush,
                        centerX - eyeOffset - eyeRadius,
                        eyeY - eyeRadius,
                        eyeRadius * 2,
                        eyeRadius * 2);
                }
            }

            // アウトラインの描画（親ウィンドウの境界色）
            if (Parent != null)
            {
                var outlineColor = OutlineRenderer.CalculateOutlineColor(Parent.BackColor);
                OutlineRenderer.DrawFormOutline(e.Graphics,
                    new Rectangle((int)animatedRect.X, (int)animatedRect.Y, (int)animatedRect.Width - 1, (int)animatedRect.Height - 1), outlineColor);
            }

            // 状態に応じたデバッグ描画
            if (stateMachine != null)
            {
                stateMachine.Draw(e.Graphics, LocalRenderBounds);
            }
        }
        public override void Draw(Graphics g)
        {
        }
        public override void SetParent(GameWindow? newParent)
        {
            if (Parent != null)
            {
                lastValidParent = Parent;
                Parent.RemoveChild(this);
            }

            Parent = newParent;
            Parent?.AddChild(this);

            if (Parent != null)
            {
                OnEnterWindow(Parent);
                this.Invalidate();
            }
            else
            {
                this.Invalidate();
            }
        }
        public void ResetPosition(Point position)
        {
            collisionBounds.Location = position;
            renderBounds = CalculateRenderBounds(collisionBounds);
            displayBounds = CalculateDisplayBounds(collisionBounds);
            bounds = displayBounds;
            this.Location = bounds.Location;
            physics?.ResetPhysics();

            if (stateMachine != null)
            {
                stateMachine.ChangeState(PlayerStateType.Normal);
            }
        }
        public void ResetSize(Size size)
        {
            collisionBounds.Size = size;
            renderBounds = CalculateRenderBounds(collisionBounds);
            displayBounds = CalculateDisplayBounds(collisionBounds);
            bounds = displayBounds;
            this.Size = bounds.Size;
            originalSize = size;

            // Regionを即座に再設定（アニメーションなしのデフォルト状態）
            this.Region?.Dispose();
            using (var path = new System.Drawing.Drawing2D.GraphicsPath())
            {
                const float outlineWidth = 5.0f;
                var regionRect = new RectangleF(
                    renderBounds.X - bounds.X - outlineWidth / 2,
                    renderBounds.Y - bounds.Y - outlineWidth / 2,
                    renderBounds.Width + outlineWidth,
                    renderBounds.Height + outlineWidth
                );
                path.AddRectangle(regionRect);
                this.Region = new Region(path);
            }

            this.Invalidate();
            this.Update();
        }
        public override void UpdateTargetPosition(Point newPosition)
        {
            collisionBounds.Location = newPosition;
            renderBounds = CalculateRenderBounds(collisionBounds);
            displayBounds = CalculateDisplayBounds(collisionBounds);
            bounds = displayBounds;
            this.Location = bounds.Location;
        }
        public override void UpdateTargetSize(Size newSize)
        {
            collisionBounds.Size = newSize;
            renderBounds = CalculateRenderBounds(collisionBounds);
            displayBounds = CalculateDisplayBounds(collisionBounds);
            bounds = displayBounds;
            this.Size = bounds.Size;
            AdjustPositionAfterResize(newSize);

            this.Invalidate();
            this.Update();
        }
        private void AdjustPositionAfterResize(Size newSize)
        {
            Rectangle newCollision = new Rectangle(
                collisionBounds.X,
                collisionBounds.Y,
                newSize.Width,
                newSize.Height
            );

            // windowInteractionを使用して移動可能かチェック（DIで必ず注入される）
            if (!windowInteraction!.IsValidMove(newCollision, Parent))
            {
                newCollision = new Rectangle(
                    Math.Max(Parent?.CollisionBounds.Left ?? 0,
                        Math.Min(collisionBounds.X, (Parent?.CollisionBounds.Right ?? Program.mainForm?.ClientSize.Width ?? 1920) - newSize.Width)),
                    Math.Max(Parent?.CollisionBounds.Top ?? 0,
                        Math.Min(collisionBounds.Y, (Parent?.CollisionBounds.Bottom ?? Program.mainForm?.ClientSize.Height ?? 1080) - newSize.Height)),
                    newSize.Width,
                    newSize.Height
                );
            }

            collisionBounds = newCollision;
            renderBounds = CalculateRenderBounds(collisionBounds);
            displayBounds = CalculateDisplayBounds(collisionBounds);
            bounds = displayBounds;
            this.Location = bounds.Location;

            this.Invalidate();
            this.Update();
        }
        private Region GetValidRegion()
        {
            if (Parent != null)
            {
                return (windowManager ?? WindowManager.Current).CalculateMovableRegion(Parent);
            }
            else if (Program.mainForm != null)
            {
                return new Region(new Rectangle(0, 0,
                    Program.mainForm.ClientSize.Width,
                    Program.mainForm.ClientSize.Height));
            }
            return new Region();
        }
        public override void OnMinimize()
        {
            IsMinimized = true;
            minimizedTime = DateTime.Now;
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
                lastValidParent.AdjustedBounds.IntersectsWith(collisionBounds))
            {
                SetParent(lastValidParent);
            }
            else
            {
                var newParent = (windowManager ?? WindowManager.Current).GetTopWindowAt(collisionBounds, null);
                SetParent(newParent);
            }
        }
        public override Size GetOriginalSize() => collisionBounds.Size;
        private void UpdatePosition(Point newPosition)
        {
            // 当たり判定領域を更新
            collisionBounds.Location = newPosition;
            // 描画領域を計算して更新
            renderBounds = CalculateRenderBounds(collisionBounds);
            // 表示領域を計算して更新
            displayBounds = CalculateDisplayBounds(collisionBounds);
            bounds = displayBounds;
            this.Location = bounds.Location;
        }
        public void SetWindowInteraction(IPlayerWindowInteraction windowInteraction)
        {
            this.windowInteraction = windowInteraction;

            // コールバックを設定
            windowInteraction.SetOnParentChangedCallback(newParent =>
            {
                Parent = newParent;
                if (newParent != null)
                {
                    OnEnterWindow(newParent);
                }
            });
        }
        protected override void WndProc(ref Message m)
        {
            // PlayerForm専用のメッセージ処理はWindowMessageHandlerで処理される
            // 循環を避けるため、base.WndProc()は条件付きで呼び出し
            var result = WindowMessageHandler.HandleWindowMessage(this, m);
            if (!result.Handled)
            {
                // WindowMessageHandlerで処理されなかった場合のみbase.WndProc()を呼び出し
                base.WndProc(ref m);
            }
            else
            {
                // 処理された場合は結果を設定
                m.Result = result.Result;
            }
        }
        protected override CreateParams CreateParams
        {
            get
            {
                var cp = base.CreateParams;
                cp.ExStyle |= 0x80000;  // WS_EX_LAYERED
                // WS_EX_TRANSPARENTを削除: サイズ変更時の黒いチラつき問題を解決
                // TransparencyKeyによる透明領域は自動的にマウスイベントを透過する
                return cp;
            }
        }
        public override void ApplyEffect(IWindowEffect effect)
        {
            if (!CanReceiveEffect(effect)) return;

            switch (effect)
            {
                case MovementEffect moveEffect:
                    var newPos = new Point(
                        collisionBounds.X + (int)moveEffect.CurrentMovement.X,
                        collisionBounds.Y + (int)moveEffect.CurrentMovement.Y
                    );
                    UpdatePosition(newPos);
                    break;

                case ResizeEffect resizeEffect:
                    var scale = resizeEffect.GetCurrentScale(this);
                    var newSize = new Size(
                        (int)(originalSize.Width * scale.Width),
                        (int)(originalSize.Height * scale.Height)
                    );
                    collisionBounds.Size = newSize;
                    renderBounds = CalculateRenderBounds(collisionBounds);
                    displayBounds = CalculateDisplayBounds(collisionBounds);
                    bounds = displayBounds;
                    this.Size = bounds.Size;
                    break;
            }
        }
        // プレイヤー専用リサイズ制約実装
        public override Size GetMinimumSize()
        {
            // プレイヤーの最小サイズはFormのMinimumSize（5x5px）を使用
            return this.MinimumSize; // 5x5px
        }
        public override Size GetMaximumSize()
        {
            // プレイヤーの最大サイズを事実上無制限に設定
            return new Size(int.MaxValue, int.MaxValue);
        }
        public override bool ShouldInheritParentConstraints
        {
            get
            {
                // プレイヤーは親の制約を継承しない（独立したサイズ制御）
                return false;
            }
        }
        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                (windowManager ?? WindowManager.Current).UnregisterFormOrder(this);
                (gameSettings ?? GameSettings.Current).SettingsChanged -= OnSettingsChanged;
            }
            base.Dispose(disposing);
        }
    }
}