using MultiWindowActionGame.Core;
using MultiWindowActionGame.Core.Systems;
using MultiWindowActionGame.Interfaces;
using MultiWindowActionGame.Managers;
using MultiWindowActionGame.Player;
using MultiWindowActionGame.Windows;
using MultiWindowActionGame.Utilities;
using MultiWindowActionGame.Services;
using MultiWindowActionGame.Effects;
using MultiWindowActionGame.Debug;
using MultiWindowActionGame.UI;
using System.IO;

namespace MultiWindowActionGame.DI
{
    public static class ServiceRegistration
    {
        public static void RegisterServices(IServiceContainer container)
        {
            // Register the container itself (for factories that need to resolve dependencies)
            container.RegisterSingleton<IServiceContainer>(container);

            // Register core services first
            RegisterCoreServices(container);

            // Register managers as singletons
            RegisterManagers(container);

            // Register main game services early (provides IWindowManager for NoEntry services)
            RegisterGameServices(container);

            // Register utility services (NoEntryZone services depend on IWindowManager)
            RegisterUtilityServices(container);

            // Register game settings (depends on NotificationService)
            RegisterGameSettings(container);

            // Register player components as transient (created per PlayerForm)
            RegisterPlayerComponents(container);

            // Register system management
            RegisterSystemManagement(container);
        }

        private static void RegisterCoreServices(IServiceContainer container)
        {
#if DEBUG
            // Debug構成: ファイルにログを出力
            var logPath = Path.Combine(Directory.GetCurrentDirectory(), "logs", "game.log");
            container.RegisterSingleton<ILogger>(new FileLogger(logPath));
#else
            // Release構成: ログを出力しない
            container.RegisterSingleton<ILogger>(new NullLogger());
#endif

            // Register error handling (depends on logger)
            container.RegisterSingleton<IErrorHandler, ErrorHandler>();
        }

        private static void RegisterManagers(IServiceContainer container)
        {
            container.RegisterSingleton<IWindowHierarchyManager, WindowHierarchyManager>();
            container.RegisterSingleton<IWindowZOrderManager, WindowZOrderManager>();
            container.RegisterSingleton<IWindowCollisionDetector, WindowCollisionDetector>();
            container.RegisterSingleton<IWindowRenderingManager, WindowRenderingManager>();
            container.RegisterSingleton<IDesktopIconManager, DesktopIconManager>();
        }

        private static void RegisterGameServices(IServiceContainer container)
        {
            container.RegisterSingleton<IWindowManager, WindowManager>();
            container.RegisterSingleton<IStageManager, StageManager>();
            container.RegisterSingleton<IMainGame, MainGame>();
        }

        private static void RegisterPlayerComponents(IServiceContainer container)
        {
            container.RegisterTransient<IPlayerPhysics, PlayerPhysics>();
            // PlayerWindowInteractionはPlayerFormFactory内で手動作成するため、DI登録不要
            // container.RegisterTransient<IPlayerWindowInteraction, PlayerWindowInteraction>();
            container.RegisterTransient<IPlayerInputHandler, PlayerInputHandler>();
            container.RegisterTransient<IPlayerStateMachine, PlayerStateMachine>();

            // PlayerForm factory for creating instances with specific positions
            container.RegisterSingleton<IPlayerFormFactory, PlayerFormFactory>();

            // UI button factory for creating button instances
            container.RegisterSingleton<IButtonFactory, ButtonFactory>();

            // Window factory for creating window instances
            container.RegisterSingleton<IWindowFactory, WindowFactory>();
        }

        private static void RegisterGameSettings(IServiceContainer container)
        {
            container.RegisterSingleton<IGameSettings, GameSettings>();
            
            // Note: Individual settings objects will be resolved from IGameSettings
            // container.RegisterSingleton(gameSettings.Player);
            // container.RegisterSingleton(gameSettings.Window);
            // container.RegisterSingleton(gameSettings.Gameplay);
        }

        private static void RegisterUtilityServices(IServiceContainer container)
        {
            // NoEntry services (depend on IWindowManager)
            container.RegisterSingleton<ZOrderVisibilityService, ZOrderVisibilityService>();
            container.RegisterSingleton<NoEntryBoundaryCollider, NoEntryBoundaryCollider>();
            container.RegisterSingleton<INoEntryZoneManager, NoEntryZoneManager>();

            // Collision services (depend on NoEntry services)
            container.RegisterSingleton<ZOrderCollisionHelper, ZOrderCollisionHelper>();
            container.RegisterSingleton<CollisionValidator, CollisionValidator>();
            container.RegisterSingleton<ICollisionService, CollisionService>();

            // Other managers
            container.RegisterSingleton<IWindowEffectManager, WindowEffectManager>();
            container.RegisterSingleton<IPerformanceMonitor, PerformanceMonitor>();

            // New service implementations
            container.RegisterSingleton<IInputService, InputService>();
            container.RegisterSingleton<IGameTimeService, GameTimeService>();
            container.RegisterSingleton<INotificationService, NotificationService>();
        }

        private static void RegisterSystemManagement(IServiceContainer container)
        {
            container.RegisterSingleton<ISystemManager, SystemManager>();

            // Individual game systems - singletonとして登録（SystemManagerで管理される）
            container.RegisterSingleton<InputSystem, InputSystem>();
            container.RegisterSingleton<RenderingSystem, RenderingSystem>();
            // PhysicsSystemはPlayerPhysicsと完全に重複していたため削除済み（Phase 5完了）
        }
    }
}