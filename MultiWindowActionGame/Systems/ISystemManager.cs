using System;
using System.Collections.Generic;
using System.Threading.Tasks;

namespace MultiWindowActionGame.Core.Systems
{
    public interface ISystemManager
    {
        void RegisterSystem(IGameSystem system);
        void UnregisterSystem(IGameSystem system);
        
        T? GetSystem<T>() where T : class, IGameSystem;
        IEnumerable<IGameSystem> GetAllSystems();
        
        Task InitializeAllAsync();
        Task UpdateAllAsync(float deltaTime);
        void ShutdownAll();
        
        void PauseAll();
        void ResumeAll();
        
        void EnableSystem<T>() where T : class, IGameSystem;
        void DisableSystem<T>() where T : class, IGameSystem;
    }
}