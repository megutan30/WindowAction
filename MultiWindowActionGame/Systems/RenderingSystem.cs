using MultiWindowActionGame.Core.Systems;
using MultiWindowActionGame.Interfaces;
using MultiWindowActionGame.Utilities;
using MultiWindowActionGame.Windows;
using MultiWindowActionGame.Debug;
using MultiWindowActionGame.Managers;
using MultiWindowActionGame.Rendering;
using System;
using System.Drawing;
using System.Threading.Tasks;

namespace MultiWindowActionGame.Core.Systems
{
    public class RenderingSystem : BaseGameSystem
    {
        private readonly IWindowManager windowManager;
        private readonly IStageManager stageManager;
        private readonly IPerformanceMonitor? performanceMonitor;
        private readonly INoEntryZoneManager? noEntryZoneManager;
        private readonly INotificationService? notificationService;
        private BufferedGraphics? graphicsBuffer;
        private int frameCount = 0;
        private float fpsTimer = 0f;
        private float currentFPS = 0f;

        public override string SystemName => "Rendering System";
        public override GameSystemPriority Priority => GameSystemPriority.Rendering;

        public RenderingSystem(ILogger logger, IErrorHandler errorHandler,
            IWindowManager windowManager, IStageManager stageManager,
            IPerformanceMonitor? performanceMonitor = null,
            INoEntryZoneManager? noEntryZoneManager = null,
            INotificationService? notificationService = null)
            : base(logger, errorHandler)
        {
            this.windowManager = windowManager ?? throw new ArgumentNullException(nameof(windowManager));
            this.stageManager = stageManager ?? throw new ArgumentNullException(nameof(stageManager));
            this.performanceMonitor = performanceMonitor;
            this.noEntryZoneManager = noEntryZoneManager;
            this.notificationService = notificationService;
        }
        
        private IPerformanceMonitor GetPerformanceMonitorSafely()
        {
            return performanceMonitor ?? PerformanceMonitor.Current;
        }
        
        private INoEntryZoneManager GetNoEntryZoneManagerSafely()
        {
            return noEntryZoneManager ?? NoEntryZoneManager.Current;
        }

        protected override async Task OnInitializeAsync()
        {
            InitializeGraphicsBuffer();
            
            if (Program.mainForm != null)
            {
                Program.mainForm.Resize += MainForm_Resize;
            }
            
            logger.LogInfo("Rendering system initialized with graphics buffer", SystemName);
            await Task.CompletedTask;
        }

        protected override async Task OnUpdateAsync(float deltaTime)
        {
            try
            {
                // パフォーマンス情報描画のためにdeltaTimeを保存する
                lastDeltaTime = deltaTime;

                // 通知を更新する
                notificationService?.Update(deltaTime);

                // NoEntryウィンドウのボーダーアニメーションを更新
                OutlineRenderer.UpdateAnimation(deltaTime);

                // FPSカウンターを更新する
                UpdateFPSCounter(deltaTime);

                // レンダリングを実行する
                await RenderFrame();
            }
            catch (Exception ex)
            {
                errorHandler.HandleError("Error during rendering", ErrorSeverity.High, ex, SystemName);
            }
        }

        private async Task RenderFrame()
        {
            using (GetPerformanceMonitorSafely().BeginScope("Total Render"))
            {
                if (graphicsBuffer == null)
                {
                    logger.LogWarning("Graphics buffer not available for rendering", null, SystemName);
                    return;
                }

                Graphics g = graphicsBuffer.Graphics;

                try
                {
                    // 画面をクリアする
                    g.Clear(Color.Transparent);

                    // ゲーム要素を順番にレンダリングする
                    await RenderGameElements(g);

                    // UI要素をレンダリングする
                    RenderUI(g);

                    // フレームを表示する
                    graphicsBuffer.Render();

                    // ウィンドウ表示を更新する
                    windowManager.UpdateDisplay();
                    
                    frameCount++;
                }
                catch (Exception ex)
                {
                    errorHandler.HandleError("Error during frame rendering", ErrorSeverity.Medium, ex, SystemName);
                }
            }

            await Task.CompletedTask;
        }

        private async Task RenderGameElements(Graphics g)
        {
            try
            {
                // ステージ要素（ゴールなど）をレンダリングする
                stageManager.CurrentGoal?.Draw(g);

                // 不可侵領域をレンダリングする
                GetNoEntryZoneManagerSafely().Draw(g);

                // ウィンドウとゲームオブジェクトをレンダリングする
                windowManager.Draw(g);

                // デスクトップアイコン衝突境界をレンダリングする（現在のステージで有効な場合）
                RenderDesktopIconCollisionBounds(g);

                // ウィンドウマーク・インジケーターをレンダリングする
                if (MainGame.IsDebugMode)
                {
                    windowManager.DrawMarks(g);
                }
            }
            catch (Exception ex)
            {
                errorHandler.HandleError("Error rendering game elements", ErrorSeverity.Medium, ex, SystemName);
            }

            await Task.CompletedTask;
        }

        private void RenderUI(Graphics g)
        {
            try
            {
                // デバッグ情報をレンダリングする
                if (MainGame.IsDebugMode)
                {
                    RenderDebugInfo(g);
                }

                // 設定通知をレンダリングする
                notificationService?.Draw(g);

                // パフォーマンス情報をレンダリングする
                if (MainGame.IsDebugMode)
                {
                    RenderPerformanceInfo(g);
                }
            }
            catch (Exception ex)
            {
                errorHandler.HandleError("Error rendering UI", ErrorSeverity.Low, ex, SystemName);
            }
        }

        private void RenderDebugInfo(Graphics g)
        {
            try
            {
                using (var font = new Font("Arial", 12))
                using (var brush = new SolidBrush(Color.White))
                {
                    var debugText = $"FPS: {currentFPS:F1}\nFrames: {frameCount}";
                    g.DrawString(debugText, font, brush, new PointF(10, 10));
                }
            }
            catch (Exception ex)
            {
                errorHandler.HandleError("Error rendering debug info", ErrorSeverity.Low, ex, SystemName);
            }
        }

        private float lastDeltaTime = 0f;

        private void RenderPerformanceInfo(Graphics g)
        {
            try
            {
                // パフォーマンス情報の描画 - 現時点では簡略化
                using (var font = new Font("Arial", 10))
                using (var brush = new SolidBrush(Color.Yellow))
                {
                    var perfText = $"Frame Time: {lastDeltaTime:F3}ms";
                    g.DrawString(perfText, font, brush, new PointF(10, 60));
                }
            }
            catch (Exception ex)
            {
                errorHandler.HandleError("Error rendering performance info", ErrorSeverity.Low, ex, SystemName);
            }
        }

        private void UpdateFPSCounter(float deltaTime)
        {
            fpsTimer += deltaTime;
            if (fpsTimer >= 1.0f)
            {
                currentFPS = frameCount / fpsTimer;
                frameCount = 0;
                fpsTimer = 0f;
                
                logger.LogTrace($"Current FPS: {currentFPS:F1}", SystemName);
            }
        }

        private void InitializeGraphicsBuffer()
        {
            try
            {
                if (Program.mainForm != null && 
                    Program.mainForm.ClientSize.Width > 0 && 
                    Program.mainForm.ClientSize.Height > 0)
                {
                    BufferedGraphicsContext context = BufferedGraphicsManager.Current;
                    graphicsBuffer = context.Allocate(Program.mainForm.CreateGraphics(),
                        Program.mainForm.ClientRectangle);
                    
                    logger.LogDebug($"Graphics buffer created: {Program.mainForm.ClientSize}", SystemName);
                }
            }
            catch (Exception ex)
            {
                errorHandler.HandleError("Failed to initialize graphics buffer", ErrorSeverity.High, ex, SystemName);
            }
        }

        private void MainForm_Resize(object? sender, EventArgs e)
        {
            logger.LogDebug("Main form resized, reinitializing graphics buffer", SystemName);
            InitializeGraphicsBuffer();
        }

        protected override void OnPause()
        {
            logger.LogInfo("Rendering paused", SystemName);
        }

        protected override void OnResume()
        {
            logger.LogInfo("Rendering resumed", SystemName);
            // 一時停止後にFPSカウンターをリセットする
            frameCount = 0;
            fpsTimer = 0f;
        }

        private void RenderDesktopIconCollisionBounds(Graphics g)
        {
            try
            {
                // デスクトップアイコン衝突判定が有効なステージかチェック
                var currentStage = stageManager.GetCurrentStage();
                if (currentStage?.EnableDesktopIcons != true)
                {
                    return; // 無効な場合は描画しない
                }

                // デスクトップアイコンを取得
                var desktopIcons = DesktopIconHelper.Instance?.GetDesktopIcons();
                if (desktopIcons == null || desktopIcons.Count == 0)
                {
                    return; // アイコンがない場合は早期リターン
                }

                // 衝突判定領域の枠のみを描画（塗りつぶしなし）
                using (var pen = new Pen(Color.FromArgb(180, 255, 140, 0), 3)) // 半透明オレンジ、太さ3px
                {
                    foreach (var icon in desktopIcons)
                    {
                        if (icon.IsValid)
                        {
                            // 衝突判定領域の枠を描画
                            g.DrawRectangle(pen, icon.CollisionBounds);
                        }
                    }
                }

                logger.LogTrace($"Rendered {desktopIcons.Count} desktop icon collision bounds", SystemName);
            }
            catch (Exception ex)
            {
                // エラーが発生しても描画を続行（他の要素の描画に影響を与えない）
                errorHandler.HandleError("Error rendering desktop icon collision bounds", ErrorSeverity.Low, ex, SystemName);
            }
        }

        protected override void OnShutdown()
        {
            try
            {
                if (Program.mainForm != null)
                {
                    Program.mainForm.Resize -= MainForm_Resize;
                }

                graphicsBuffer?.Dispose();
                graphicsBuffer = null;

                logger.LogInfo("Rendering system shutdown complete", SystemName);
            }
            catch (Exception ex)
            {
                errorHandler.HandleError("Error during rendering system shutdown", ErrorSeverity.Medium, ex, SystemName);
            }
        }
    }
}