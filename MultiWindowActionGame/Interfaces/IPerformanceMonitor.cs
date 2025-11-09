using System;
using System.Collections.Generic;

namespace MultiWindowActionGame.Interfaces
{
    public interface IPerformanceMonitor
    {
        IDisposable BeginScope(string name);
        void UpdateFrameTime(float deltaTime);
        float GetAverageFrameTime();
        float GetCurrentFPS();
        IReadOnlyDictionary<string, float> GetTimings();
        void Reset();
    }
}