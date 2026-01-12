using System;

namespace MultiWindowActionGame.Core
{
    /// <summary>
    /// ログを出力しないロガー（Release構成用）
    /// </summary>
    public class NullLogger : ILogger
    {
        public void Log(LogLevel level, string message, Exception? exception = null, string? context = null)
        {
            // 何もしない
        }

        public void LogTrace(string message, string? context = null)
        {
            // 何もしない
        }

        public void LogDebug(string message, string? context = null)
        {
            // 何もしない
        }

        public void LogInfo(string message, string? context = null)
        {
            // 何もしない
        }

        public void LogWarning(string message, Exception? exception = null, string? context = null)
        {
            // 何もしない
        }

        public void LogError(string message, Exception? exception = null, string? context = null)
        {
            // 何もしない
        }

        public void LogCritical(string message, Exception? exception = null, string? context = null)
        {
            // 何もしない
        }

        public bool IsEnabled(LogLevel level)
        {
            return false;
        }

        public void SetLogLevel(LogLevel minimumLevel)
        {
            // 何もしない
        }

        public void Flush()
        {
            // 何もしない
        }
    }
}
