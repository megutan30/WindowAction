using MultiWindowActionGame.Core;
using MultiWindowActionGame.Core.Systems;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;

namespace MultiWindowActionGame.Core.Systems
{
    public class SystemManager : ISystemManager
    {
        private readonly List<IGameSystem> systems = new();
        private readonly ILogger logger;
        private readonly IErrorHandler errorHandler;
        private readonly object lockObject = new object();

        public SystemManager(ILogger logger, IErrorHandler errorHandler)
        {
            this.logger = logger ?? throw new ArgumentNullException(nameof(logger));
            this.errorHandler = errorHandler ?? throw new ArgumentNullException(nameof(errorHandler));
        }

        public void RegisterSystem(IGameSystem system)
        {
            if (system == null) throw new ArgumentNullException(nameof(system));

            lock (lockObject)
            {
                if (systems.Any(s => s.GetType() == system.GetType()))
                {
                    logger.LogWarning($"System {system.SystemName} is already registered", null, "SystemManager");
                    return;
                }

                systems.Add(system);
                
                // 優先度順にソートする
                systems.Sort((a, b) => a.Priority.CompareTo(b.Priority));
                
                logger.LogInfo($"Registered system: {system.SystemName} (Priority: {system.Priority})", "SystemManager");
            }
        }

        public void UnregisterSystem(IGameSystem system)
        {
            if (system == null) throw new ArgumentNullException(nameof(system));

            lock (lockObject)
            {
                if (systems.Remove(system))
                {
                    logger.LogInfo($"Unregistered system: {system.SystemName}", "SystemManager");
                    
                    try
                    {
                        system.Shutdown();
                    }
                    catch (Exception ex)
                    {
                        errorHandler.HandleError($"Error shutting down system {system.SystemName} during unregistration", 
                            ErrorSeverity.Medium, ex, "SystemManager");
                    }
                }
            }
        }

        public T? GetSystem<T>() where T : class, IGameSystem
        {
            lock (lockObject)
            {
                return systems.OfType<T>().FirstOrDefault();
            }
        }

        public IEnumerable<IGameSystem> GetAllSystems()
        {
            lock (lockObject)
            {
                return systems.ToList();
            }
        }

        public async Task InitializeAllAsync()
        {
            logger.LogInfo("Initializing all systems", "SystemManager");
            
            var systemsCopy = GetAllSystems().ToList();
            
            foreach (var system in systemsCopy)
            {
                try
                {
                    await system.InitializeAsync();
                }
                catch (Exception ex)
                {
                    errorHandler.HandleError($"Failed to initialize system {system.SystemName}", 
                        ErrorSeverity.High, ex, "SystemManager");
                    
                    // 重要なシステムの場合、初期化を停止する必要がある場合がある
                    if (!errorHandler.ShouldContinue(ErrorSeverity.High))
                    {
                        throw;
                    }
                }
            }
            
            logger.LogInfo("All systems initialized", "SystemManager");
        }

        public async Task UpdateAllAsync(float deltaTime)
        {
            var systemsCopy = GetAllSystems().Where(s => s.IsEnabled && !s.IsPaused).ToList();
            
            foreach (var system in systemsCopy)
            {
                await system.UpdateAsync(deltaTime);
            }
        }

        public void ShutdownAll()
        {
            logger.LogInfo("Shutting down all systems", "SystemManager");
            
            var systemsCopy = GetAllSystems().Reverse().ToList(); // 逆順でシャットダウンする
            
            foreach (var system in systemsCopy)
            {
                try
                {
                    system.Shutdown();
                }
                catch (Exception ex)
                {
                    errorHandler.HandleError($"Error shutting down system {system.SystemName}", 
                        ErrorSeverity.Medium, ex, "SystemManager");
                }
            }
            
            lock (lockObject)
            {
                systems.Clear();
            }
            
            logger.LogInfo("All systems shut down", "SystemManager");
        }

        public void PauseAll()
        {
            logger.LogInfo("Pausing all systems", "SystemManager");
            
            var systemsCopy = GetAllSystems().ToList();
            foreach (var system in systemsCopy)
            {
                system.Pause();
            }
        }

        public void ResumeAll()
        {
            logger.LogInfo("Resuming all systems", "SystemManager");
            
            var systemsCopy = GetAllSystems().ToList();
            foreach (var system in systemsCopy)
            {
                system.Resume();
            }
        }

        public void EnableSystem<T>() where T : class, IGameSystem
        {
            var system = GetSystem<T>();
            if (system != null)
            {
                system.IsEnabled = true;
                logger.LogInfo($"Enabled system: {system.SystemName}", "SystemManager");
            }
        }

        public void DisableSystem<T>() where T : class, IGameSystem
        {
            var system = GetSystem<T>();
            if (system != null)
            {
                system.IsEnabled = false;
                logger.LogInfo($"Disabled system: {system.SystemName}", "SystemManager");
            }
        }
    }
}