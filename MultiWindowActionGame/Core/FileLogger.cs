using System;
using System.Diagnostics;
using System.IO;
using System.Text;

namespace MultiWindowActionGame.Core
{
    public class FileLogger : ILogger, IDisposable
    {
        private readonly StreamWriter writer;
        private readonly object lockObject = new object();
        private LogLevel minimumLevel = LogLevel.Info;
        private bool disposed = false;

        public FileLogger(string logFilePath)
        {
            var directory = Path.GetDirectoryName(logFilePath);
            if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
            {
                Directory.CreateDirectory(directory);
            }

            writer = new StreamWriter(logFilePath, append: true, Encoding.UTF8)
            {
                AutoFlush = true
            };

            LogInfo("Logger initialized", "FileLogger");
        }

        public void Log(LogLevel level, string message, Exception? exception = null, string? context = null)
        {
            if (!IsEnabled(level))
                return;

            lock (lockObject)
            {
                if (disposed)
                    return;

                try
                {
                    var timestamp = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss.fff");
                    var contextPart = !string.IsNullOrEmpty(context) ? $" [{context}]" : "";
                    var logEntry = $"[{timestamp}] {level,-8} {message}{contextPart}";

                    writer.WriteLine(logEntry);

                    if (exception != null)
                    {
                        writer.WriteLine($"{"",25} Exception: {exception.GetType().Name}: {exception.Message}");
                        if (!string.IsNullOrEmpty(exception.StackTrace))
                        {
                            writer.WriteLine($"{"",25} StackTrace:");
                            var stackLines = exception.StackTrace.Split('\n');
                            foreach (var line in stackLines)
                            {
                                writer.WriteLine($"{"",25} {line.Trim()}");
                            }
                        }
                    }

                    // デバッグコンソールにも出力する
                    if (exception != null)
                    {
                    }
                }
                catch (Exception ex)
                {
                    // ファイルログが失敗した場合はデバッグ出力にフォールバックする
                }
            }
        }

        public void LogTrace(string message, string? context = null)
            => Log(LogLevel.Trace, message, context: context);

        public void LogDebug(string message, string? context = null)
            => Log(LogLevel.Debug, message, context: context);

        public void LogInfo(string message, string? context = null)
            => Log(LogLevel.Info, message, context: context);

        public void LogWarning(string message, Exception? exception = null, string? context = null)
            => Log(LogLevel.Warning, message, exception, context);

        public void LogError(string message, Exception? exception = null, string? context = null)
            => Log(LogLevel.Error, message, exception, context);

        public void LogCritical(string message, Exception? exception = null, string? context = null)
            => Log(LogLevel.Critical, message, exception, context);

        public bool IsEnabled(LogLevel level)
        {
            return level >= minimumLevel;
        }

        public void SetLogLevel(LogLevel minimumLevel)
        {
            this.minimumLevel = minimumLevel;
            LogInfo($"Log level set to {minimumLevel}", "FileLogger");
        }

        public void Flush()
        {
            lock (lockObject)
            {
                writer?.Flush();
            }
        }

        public void Dispose()
        {
            if (!disposed)
            {
                lock (lockObject)
                {
                    if (!disposed)
                    {
                        LogInfo("Logger shutting down", "FileLogger");
                        writer?.Dispose();
                        disposed = true;
                    }
                }
            }
        }
    }
}