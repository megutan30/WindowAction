using MultiWindowActionGame.Core;
using System;
using System.Threading.Tasks;

namespace MultiWindowActionGame.Core.Systems
{
    public abstract class BaseGameSystem : IGameSystem
    {
        protected readonly ILogger logger;
        protected readonly IErrorHandler errorHandler;

        public abstract string SystemName { get; }
        public abstract GameSystemPriority Priority { get; }
        
        public bool IsEnabled { get; set; } = true;
        public bool IsPaused { get; private set; } = false;

        protected BaseGameSystem(ILogger logger, IErrorHandler errorHandler)
        {
            this.logger = logger ?? throw new ArgumentNullException(nameof(logger));
            this.errorHandler = errorHandler ?? throw new ArgumentNullException(nameof(errorHandler));
        }

        public virtual async Task InitializeAsync()
        {
            try
            {
                logger.LogInfo($"Initializing {SystemName}", SystemName);
                await OnInitializeAsync();
                logger.LogInfo($"{SystemName} initialized successfully", SystemName);
            }
            catch (Exception ex)
            {
                errorHandler.HandleError($"Failed to initialize {SystemName}", ErrorSeverity.High, ex, SystemName);
                throw;
            }
        }

        public virtual async Task UpdateAsync(float deltaTime)
        {
            if (!IsEnabled || IsPaused)
                return;

            try
            {
                await OnUpdateAsync(deltaTime);
            }
            catch (Exception ex)
            {
                errorHandler.HandleError($"Error during {SystemName} update", ErrorSeverity.Medium, ex, SystemName);
            }
        }

        public virtual void Shutdown()
        {
            try
            {
                logger.LogInfo($"Shutting down {SystemName}", SystemName);
                OnShutdown();
                logger.LogInfo($"{SystemName} shut down successfully", SystemName);
            }
            catch (Exception ex)
            {
                errorHandler.HandleError($"Error during {SystemName} shutdown", ErrorSeverity.Medium, ex, SystemName);
            }
        }

        public virtual void Pause()
        {
            if (!IsPaused)
            {
                IsPaused = true;
                OnPause();
                logger.LogDebug($"{SystemName} paused", SystemName);
            }
        }

        public virtual void Resume()
        {
            if (IsPaused)
            {
                IsPaused = false;
                OnResume();
                logger.LogDebug($"{SystemName} resumed", SystemName);
            }
        }

        protected virtual async Task OnInitializeAsync()
        {
            // 派生クラスでオーバーライドする
            await Task.CompletedTask;
        }

        protected virtual async Task OnUpdateAsync(float deltaTime)
        {
            // 派生クラスでオーバーライドする
            await Task.CompletedTask;
        }

        protected virtual void OnShutdown()
        {
            // 派生クラスでオーバーライドする
        }

        protected virtual void OnPause()
        {
            // 派生クラスでオーバーライドする
        }

        protected virtual void OnResume()
        {
            // 派生クラスでオーバーライドする
        }
    }
}