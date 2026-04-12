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
            // コンテナ自身を登録する（依存関係を解決する必要があるファクトリー用）
            container.RegisterSingleton<IServiceContainer>(container);

            // コアサービスを最初に登録する
            RegisterCoreServices(container);

            // マネージャーをシングルトンとして登録する
            RegisterManagers(container);

            // メインゲームサービスを早期に登録する（NoEntryサービスにIWindowManagerを提供）
            RegisterGameServices(container);

            // ユーティリティサービスを登録する（NoEntryZoneサービスはIWindowManagerに依存）
            RegisterUtilityServices(container);

            // ゲーム設定を登録する（NotificationServiceに依存）
            RegisterGameSettings(container);

            // プレイヤーコンポーネントをトランジェントとして登録する（PlayerFormごとに生成）
            RegisterPlayerComponents(container);

            // システム管理を登録する
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

            // エラーハンドリングを登録する（ロガーに依存）
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

            // 特定の位置でインスタンスを生成するPlayerFormファクトリー
            container.RegisterSingleton<IPlayerFormFactory, PlayerFormFactory>();

            // ボタンインスタンスを生成するUIボタンファクトリー
            container.RegisterSingleton<IButtonFactory, ButtonFactory>();

            // ウィンドウインスタンスを生成するウィンドウファクトリー
            container.RegisterSingleton<IWindowFactory, WindowFactory>();
        }

        private static void RegisterGameSettings(IServiceContainer container)
        {
            container.RegisterSingleton<IGameSettings, GameSettings>();
            
            // 注意: 個別の設定オブジェクトはIGameSettingsから解決される
            // container.RegisterSingleton(gameSettings.Player);
            // container.RegisterSingleton(gameSettings.Window);
            // container.RegisterSingleton(gameSettings.Gameplay);
        }

        private static void RegisterUtilityServices(IServiceContainer container)
        {
            // NoEntryサービス（IWindowManagerに依存）
            container.RegisterSingleton<ZOrderVisibilityService, ZOrderVisibilityService>();
            container.RegisterSingleton<NoEntryBoundaryCollider, NoEntryBoundaryCollider>();
            container.RegisterSingleton<INoEntryZoneManager, NoEntryZoneManager>();

            // 衝突判定サービス（NoEntryサービスに依存）
            container.RegisterSingleton<ZOrderCollisionHelper, ZOrderCollisionHelper>();
            container.RegisterSingleton<CollisionValidator, CollisionValidator>();
            container.RegisterSingleton<ICollisionService, CollisionService>();

            // その他のマネージャー
            container.RegisterSingleton<IWindowEffectManager, WindowEffectManager>();
            container.RegisterSingleton<IPerformanceMonitor, PerformanceMonitor>();

            // 新しいサービス実装
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