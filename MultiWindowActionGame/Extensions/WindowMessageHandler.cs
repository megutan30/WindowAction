using MultiWindowActionGame;
using MultiWindowActionGame.Windows;
using MultiWindowActionGame.Effects;
using MultiWindowActionGame.Core;

namespace MultiWindowActionGame.Extensions
{
    public static class WindowMessageHandler
    {
        public readonly struct MessageHandleResult
        {
            public bool Handled { get; }
            public IntPtr Result { get; }

            public MessageHandleResult(bool handled, IntPtr result = default)
            {
                Handled = handled;
                Result = result;
            }

            public static MessageHandleResult NotHandled => new(false);
            public static MessageHandleResult Success => new(true, IntPtr.Zero);
            public static MessageHandleResult WithResult(IntPtr result) => new(true, result);
        }

        public static MessageHandleResult HandleWindowMessage(BaseEffectTarget target, Message m)
        {
            if (target is MultiWindowActionGame.Player.PlayerForm player)
            {
                // PlayerForm専用の処理
                var playerResult = HandlePlayerFormMessages(player, m);
                if (playerResult.Handled)
                {
                    return playerResult;
                }
            }
            else if (target is GameWindow window)
            {
                // GameWindow特有の処理
                if (m.Msg == WindowMessages.WM_ACTIVATE && !window.IsInitializing)
                {
                    if (m.WParam.ToInt32() != 0)
                    {
                        HandleLeftButtonDown(window);
                        // タスクバーからの選択時は親子判定も実行（BeginInvokeで遅延実行）
                        window.BeginInvoke(new Action(() => {
                            WindowManager.Current.CheckPotentialParentWindow(window);
                        }));
                        return MessageHandleResult.Success;
                    }
                }

                // ストラテジーにメッセージを渡す前に共通処理を実行
                var commonResult = HandleCommonMessages(window, m);
                if (commonResult.Handled)
                {
                    return commonResult;
                }

                // ストラテジー固有の処理
                window.Strategy.HandleWindowMessage(window, m);

                // ストラテジー処理後の共通処理
                var postResult = HandlePostStrategyMessages(window, m);
                if (postResult.Handled)
                {
                    return postResult;
                }
            }
            else
            {
                // その他のBaseEffectTarget共通の処理
                var result = HandleBaseEffectTargetMessages(target, m);
                if (result.Handled)
                {
                    return result;
                }
            }

            return MessageHandleResult.NotHandled;
        }

        private static MessageHandleResult HandlePlayerFormMessages(MultiWindowActionGame.Player.PlayerForm player, Message m)
        {
            // PlayerForm固有のメッセージ処理
            switch (m.Msg)
            {
                case 0x0100: // WM_KEYDOWN
                case 0x0101: // WM_KEYUP
                             // キーボード入力はPlayerFormの既存システムに委譲
                    return MessageHandleResult.NotHandled;

                case WindowMessages.WM_SYSCOMMAND:
                    int command = m.WParam.ToInt32() & 0xFFF0;
                    switch (command)
                    {
                        case WindowMessages.SC_MINIMIZE:
                            player.OnMinimize();
                            return MessageHandleResult.Success;
                        case WindowMessages.SC_RESTORE:
                            player.OnRestore();
                            return MessageHandleResult.Success;
                        case WindowMessages.SC_CLOSE:
                            // プレイヤーは閉じられない
                            return MessageHandleResult.Success;
                    }
                    break;

                case WindowMessages.WM_MOUSEACTIVATE:
                    // プレイヤーはマウスでアクティブ化しない
                    return new MessageHandleResult(true, (IntPtr)WindowMessages.MA_NOACTIVATE);

                case WindowMessages.WM_ACTIVATE:
                    // プレイヤーのアクティブ化処理は無視
                    return MessageHandleResult.Success;

                case WindowMessages.WM_LBUTTONDOWN:
                case WindowMessages.WM_LBUTTONUP:
                case WindowMessages.WM_MOUSEMOVE:
                    // マウス操作は無視（キーボード操作のみ）
                    return MessageHandleResult.Success;
            }

            return MessageHandleResult.NotHandled;
        }

        private static MessageHandleResult HandleBaseEffectTargetMessages(BaseEffectTarget target, Message m)
        {
            if (m.Msg == WindowMessages.WM_MOUSEACTIVATE)
            {
                return new MessageHandleResult(true, (IntPtr)WindowMessages.MA_NOACTIVATE);
            }

            if (m.Msg == WindowMessages.WM_SYSCOMMAND)
            {
                int command = m.WParam.ToInt32() & 0xFFF0;
                switch (command)
                {
                    case WindowMessages.SC_MINIMIZE:
                        target.OnMinimize();
                        // base.WndProc()でOSのデフォルト処理を実行する必要があるため、NotHandledを返す
                        return MessageHandleResult.NotHandled;
                    case WindowMessages.SC_RESTORE:
                        target.OnRestore();
                        // base.WndProc()でOSのデフォルト処理を実行する必要があるため、NotHandledを返す
                        return MessageHandleResult.NotHandled;
                    case WindowMessages.SC_CLOSE:
                        return MessageHandleResult.Success;
                }
            }

            return MessageHandleResult.NotHandled;
        }

        private static MessageHandleResult HandleCommonMessages(GameWindow window, Message m)
        {
            return m.Msg switch
            {
                WindowMessages.WM_MOUSEACTIVATE => new MessageHandleResult(true, (IntPtr)WindowMessages.MA_NOACTIVATE),
                WindowMessages.WM_NCHITTEST => HandleHitTest(window, m),
                WindowMessages.WM_SYSCOMMAND => HandleSysCommand(window, m),
                _ => MessageHandleResult.NotHandled
            };
        }

        private static MessageHandleResult HandlePostStrategyMessages(GameWindow window, Message m)
        {
            switch (m.Msg)
            {
                case WindowMessages.WM_LBUTTONDOWN:
                case WindowMessages.WM_LBUTTONDBLCLK:  // ダブルクリックも通常クリックと同じ処理
                    HandleLeftButtonDown(window);
                    return MessageHandleResult.Success;

                case WindowMessages.WM_LBUTTONUP:
                    HandleLeftButtonUp(window);
                    return MessageHandleResult.Success;

                case WindowMessages.WM_MOUSEMOVE:
                    return MessageHandleResult.Success;

                default:
                    return MessageHandleResult.NotHandled;
            }
        }

        private static MessageHandleResult HandleHitTest(GameWindow window, Message m)
        {
            // キャプション領域でのヒットテスト
            if (m.Result.ToInt32() == WindowMessages.HTCAPTION)
            {
                return MessageHandleResult.WithResult(IntPtr.Zero);
            }
            return MessageHandleResult.NotHandled;
        }

        private static MessageHandleResult HandleSysCommand(GameWindow window, Message m)
        {
            int command = m.WParam.ToInt32() & 0xFFF0;
            switch (command)
            {
                case WindowMessages.SC_CLOSE:
                    return MessageHandleResult.Success;

                case WindowMessages.SC_MINIMIZE:
                    // Strategyに委譲してWindowごとの最小化処理を実行
                    window.Strategy.HandleWindowMessage(window, m);
                    // base.WndProc()でOSのデフォルト処理を実行する必要があるため、NotHandledを返す
                    return MessageHandleResult.NotHandled;

                case WindowMessages.SC_RESTORE:
                    // Strategyに委譲してWindowごとの復元処理を実行
                    window.Strategy.HandleWindowMessage(window, m);
                    // OSの復元処理後に親子判定を実行（BeginInvokeで遅延実行）
                    window.BeginInvoke(new Action(() => {
                        WindowManager.Current.CheckPotentialParentWindow(window);
                    }));
                    // base.WndProc()でOSのデフォルト処理を実行する必要があるため、NotHandledを返す
                    return MessageHandleResult.NotHandled;

                default:
                    return MessageHandleResult.NotHandled;
            }
        }

        private static void HandleLeftButtonDown(GameWindow window)
        {
            WindowManager.Current.BringWindowToFront(window);
        }

        private static void HandleLeftButtonUp(GameWindow window)
        {
            WindowManager.Current.CheckPotentialParentWindow(window);
        }

    }
}