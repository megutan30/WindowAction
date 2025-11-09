using System;
using System.Threading.Tasks;

namespace MultiWindowActionGame.Core
{
    public enum ErrorSeverity
    {
        Low,
        Medium,
        High,
        Critical
    }

    public interface IGameError
    {
        string ErrorId { get; }
        string Message { get; }
        Exception? Exception { get; }
        ErrorSeverity Severity { get; }
        DateTime Timestamp { get; }
        string? Context { get; }
    }

    public interface IErrorHandler
    {
        void HandleError(IGameError error);
        void HandleError(string message, ErrorSeverity severity = ErrorSeverity.Medium, Exception? exception = null, string? context = null);
        Task<T> HandleAsync<T>(Func<Task<T>> operation, string operationName, T? defaultValue = default);
        T Handle<T>(Func<T> operation, string operationName, T? defaultValue = default);
        bool ShouldContinue(ErrorSeverity severity);
        void RegisterErrorRecovery<TException>(Action<TException> recoveryAction) where TException : Exception;
    }
}