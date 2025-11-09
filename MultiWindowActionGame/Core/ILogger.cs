using System;

namespace MultiWindowActionGame.Core
{
    public enum LogLevel
    {
        Trace,
        Debug,
        Info,
        Warning,
        Error,
        Critical
    }

    public interface ILogger
    {
        void Log(LogLevel level, string message, Exception? exception = null, string? context = null);
        void LogTrace(string message, string? context = null);
        void LogDebug(string message, string? context = null);
        void LogInfo(string message, string? context = null);
        void LogWarning(string message, Exception? exception = null, string? context = null);
        void LogError(string message, Exception? exception = null, string? context = null);
        void LogCritical(string message, Exception? exception = null, string? context = null);

        bool IsEnabled(LogLevel level);
        void SetLogLevel(LogLevel minimumLevel);
        void Flush();
    }
}