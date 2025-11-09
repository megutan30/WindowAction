using System;

namespace MultiWindowActionGame.Core
{
    public class GameError : IGameError
    {
        public string ErrorId { get; }
        public string Message { get; }
        public Exception? Exception { get; }
        public ErrorSeverity Severity { get; }
        public DateTime Timestamp { get; }
        public string? Context { get; }

        public GameError(
            string message,
            ErrorSeverity severity = ErrorSeverity.Medium,
            Exception? exception = null,
            string? context = null)
        {
            ErrorId = Guid.NewGuid().ToString("N")[..8];
            Message = message;
            Exception = exception;
            Severity = severity;
            Timestamp = DateTime.Now;
            Context = context;
        }

        public override string ToString()
        {
            var contextPart = !string.IsNullOrEmpty(Context) ? $" [{Context}]" : "";
            var exceptionPart = Exception != null ? $" - {Exception.GetType().Name}: {Exception.Message}" : "";
            return $"[{ErrorId}] {Severity}: {Message}{contextPart}{exceptionPart}";
        }
    }
}