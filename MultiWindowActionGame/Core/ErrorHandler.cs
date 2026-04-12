using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace MultiWindowActionGame.Core
{
    public class ErrorHandler : IErrorHandler
    {
        private readonly Dictionary<Type, Action<Exception>> recoveryActions = new();
        private readonly ILogger logger;

        public ErrorHandler(ILogger logger)
        {
            this.logger = logger ?? throw new ArgumentNullException(nameof(logger));
        }

        public void HandleError(IGameError error)
        {
            // エラーをログに記録する
            logger.LogError(error.Message, error.Exception, error.Context);

            // 例外が存在する場合は回復を試みる
            if (error.Exception != null && recoveryActions.TryGetValue(error.Exception.GetType(), out var recovery))
            {
                try
                {
                    recovery(error.Exception);
                    logger.LogInfo($"Successfully recovered from error {error.ErrorId}");
                }
                catch (Exception recoveryException)
                {
                    logger.LogError($"Recovery failed for error {error.ErrorId}", recoveryException);
                }
            }

            // 深刻度に基づいて処理する
            HandleBySeverity(error);
        }

        public void HandleError(string message, ErrorSeverity severity = ErrorSeverity.Medium, Exception? exception = null, string? context = null)
        {
            var error = new GameError(message, severity, exception, context);
            HandleError(error);
        }

        public async Task<T> HandleAsync<T>(Func<Task<T>> operation, string operationName, T? defaultValue = default)
        {
            try
            {
                return await operation();
            }
            catch (Exception ex)
            {
                HandleError($"Failed to execute async operation: {operationName}", ErrorSeverity.Medium, ex, operationName);
                return defaultValue!;
            }
        }

        public T Handle<T>(Func<T> operation, string operationName, T? defaultValue = default)
        {
            try
            {
                return operation();
            }
            catch (Exception ex)
            {
                HandleError($"Failed to execute operation: {operationName}", ErrorSeverity.Medium, ex, operationName);
                return defaultValue!;
            }
        }

        public bool ShouldContinue(ErrorSeverity severity)
        {
            return severity != ErrorSeverity.Critical;
        }

        public void RegisterErrorRecovery<TException>(Action<TException> recoveryAction) where TException : Exception
        {
            recoveryActions[typeof(TException)] = ex => recoveryAction((TException)ex);
        }

        private void HandleBySeverity(IGameError error)
        {
            switch (error.Severity)
            {
                case ErrorSeverity.Low:
                    // ログに記録するだけで処理を中断しない
                    break;

                case ErrorSeverity.Medium:
                    // ログに記録し、デバッグ情報を表示する可能性がある
                    if (MainGame.IsDebugMode)
                    {
                        Console.WriteLine($"Error: {error.Message}");
                    }
                    break;

                case ErrorSeverity.High:
                    // ユーザーに通知するが処理を継続する
                    if (Program.mainForm != null && !Program.mainForm.InvokeRequired)
                    {
                        // ノンブロッキング通知を表示できる
                        Console.WriteLine($"High severity error occurred: {error.Message}");
                    }
                    break;

                case ErrorSeverity.Critical:
                    // 致命的エラー - グレースフルシャットダウンが必要な場合がある
                    logger.LogCritical($"CRITICAL ERROR: {error.Message}", error.Exception, error.Context);

                    if (Program.mainForm != null)
                    {
                        MessageBox.Show(
                            $"A critical error occurred: {error.Message}\n\nThe application will now close.",
                            "Critical Error",
                            MessageBoxButtons.OK,
                            MessageBoxIcon.Error
                        );
                        Application.Exit();
                    }
                    break;
            }
        }
    }
}