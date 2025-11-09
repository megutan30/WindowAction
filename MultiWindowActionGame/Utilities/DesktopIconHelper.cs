using System;
using System.IO;
using MultiWindowActionGame.Core;
using MultiWindowActionGame.Managers;

namespace MultiWindowActionGame.Utilities
{
    /// <summary>
    /// DesktopIconManagerにアクセスするためのヘルパークラス
    /// </summary>
    public static class DesktopIconHelper
    {
        private static IDesktopIconManager? _current;
        public static IDesktopIconManager? Current
        {
            get => _current;
            internal set => _current = value;
        }

        // 後方互換性のために残す
        public static IDesktopIconManager? Instance => Current;

        public static void Initialize()
        {
            if (_current == null)
            {
                try
                {
                    // ログディレクトリを作成
                    var logDir = Path.Combine(Directory.GetCurrentDirectory(), "logs");
                    if (!Directory.Exists(logDir))
                    {
                        Directory.CreateDirectory(logDir);
                    }

                    // 簡易ロガーとエラーハンドラーを作成
                    var logPath = Path.Combine(logDir, "desktop_icons.log");
                    var logger = new FileLogger(logPath);
                    var errorHandler = new ErrorHandler(logger);
                    _current = new DesktopIconManager(logger, errorHandler);

                    // 即座にアイコンを取得してログに記録
                    _current.RefreshIcons();
                }
                catch
                {
                    // 初期化に失敗した場合はnullのまま
                    _current = null;
                }
            }
        }
    }
}