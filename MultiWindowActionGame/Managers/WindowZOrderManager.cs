using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows.Forms;
using MultiWindowActionGame.Windows;
using MultiWindowActionGame.UI;

namespace MultiWindowActionGame.Managers
{
    public class WindowZOrderManager : IWindowZOrderManager
    {
        private readonly Dictionary<IntPtr, ZOrderPriority> handlePriorities = new();
        private readonly SortedDictionary<ZOrderPriority, List<Form>> formsByPriority = new();
        private readonly object lockObject = new object();

        public void RegisterFormOrder(Form form, ZOrderPriority priority)
        {
            lock (lockObject)
            {
                handlePriorities[form.Handle] = priority;
                if (!formsByPriority.ContainsKey(priority))
                {
                    formsByPriority[priority] = new List<Form>();
                }
                formsByPriority[priority].Add(form);
                UpdateFormZOrder(form, priority);
            }
        }

        public void UnregisterFormOrder(Form form)
        {
            lock (lockObject)
            {
                if (!form.IsDisposed && handlePriorities.TryGetValue(form.Handle, out var priority))
                {
                    handlePriorities.Remove(form.Handle);
                    if (formsByPriority.ContainsKey(priority))
                    {
                        formsByPriority[priority].Remove(form);
                    }
                }
            }
        }

        public IReadOnlyList<GameWindow> BringWindowToFront(GameWindow window, IReadOnlyList<GameWindow> allWindows)
        {
            lock (lockObject)
            {
                var windowsList = allWindows.ToList();

                // クリックされたウィンドウとその子孫を最前面に移動
                // （親の有無に関わらず、視覚的なZ-orderと内部リストを一致させる）
                var windowGroup = new List<GameWindow> { window };
                windowGroup.AddRange(window.GetAllDescendants());

                foreach (var groupWindow in windowGroup)
                {
                    windowsList.Remove(groupWindow);
                }

                windowsList.AddRange(windowGroup);

                // 同じ優先度内での順序も更新
                if (handlePriorities.TryGetValue(window.Handle, out var priority))
                {
                    if (formsByPriority.ContainsKey(priority))
                    {
                        var forms = formsByPriority[priority];
                        foreach (var groupWindow in CollectRelatedWindows(window))
                        {
                            if (forms.Contains(groupWindow))
                            {
                                forms.Remove(groupWindow);
                                forms.Add(groupWindow);
                            }
                        }
                    }
                }

                UpdateWindowGroupZOrder();

                // 更新されたリストを返す
                return windowsList;
            }
        }

        public void UpdateWindowGroupZOrder()
        {
            lock (lockObject)
            {
                foreach (var priorityGroup in formsByPriority.Reverse())
                {
                    for (int i = priorityGroup.Value.Count - 1; i >= 0; i--)
                    {
                        var form = priorityGroup.Value[i];
                        if (!form.IsDisposed && form.Handle != IntPtr.Zero)
                        {
                            IntPtr insertAfter = i == priorityGroup.Value.Count - 1
                                ? GetInsertAfterHandle(priorityGroup.Key)
                                : priorityGroup.Value[i + 1].Handle;

                            WindowMessages.SetWindowPos(
                                form.Handle,
                                insertAfter,
                                0, 0, 0, 0,
                                WindowMessages.SWP_NOMOVE | WindowMessages.SWP_NOSIZE | WindowMessages.SWP_NOACTIVATE
                            );
                        }
                    }
                }
            }
        }

        public void UpdateFormZOrder(Form form, ZOrderPriority priority)
        {
            if (!form.IsDisposed && form.Handle != IntPtr.Zero)
            {
                IntPtr insertAfter = GetInsertAfterHandle(priority);
                WindowMessages.SetWindowPos(
                    form.Handle,
                    insertAfter,
                    0, 0, 0, 0,
                    WindowMessages.SWP_NOMOVE | WindowMessages.SWP_NOSIZE | WindowMessages.SWP_NOACTIVATE
                );
            }
        }

        public IReadOnlyList<GameButton> GetAllButtons()
        {
            lock (lockObject)
            {
                if (formsByPriority.TryGetValue(ZOrderPriority.Button, out var buttonForms))
                {
                    return buttonForms.OfType<GameButton>().ToList();
                }
                return new List<GameButton>();
            }
        }

        public IReadOnlyDictionary<ZOrderPriority, IReadOnlyList<Form>> GetFormsByPriority()
        {
            lock (lockObject)
            {
                return formsByPriority.ToDictionary(
                    kvp => kvp.Key,
                    kvp => (IReadOnlyList<Form>)kvp.Value.ToList()
                );
            }
        }

        private IntPtr GetInsertAfterHandle(ZOrderPriority priority)
        {
            return WindowMessages.HWND_TOPMOST;
        }

        private List<GameWindow> CollectRelatedWindows(GameWindow root)
        {
            var result = new List<GameWindow>();
            CollectRelatedWindowsRecursive(root, result);
            return result;
        }

        private void CollectRelatedWindowsRecursive(GameWindow window, List<GameWindow> collection)
        {
            collection.Add(window);

            var childWindows = window.Children.OfType<GameWindow>();
            foreach (var child in childWindows)
            {
                CollectRelatedWindowsRecursive(child, collection);
            }
        }

        // Z-order比較メソッドの実装
        public int CompareWindowZOrder(GameWindow window1, GameWindow window2, IReadOnlyList<GameWindow> allWindows)
        {
            lock (lockObject)
            {
                int index1 = GetWindowZIndex(window1, allWindows);
                int index2 = GetWindowZIndex(window2, allWindows);
                return index1.CompareTo(index2);
            }
        }

        public int GetWindowZIndex(GameWindow window, IReadOnlyList<GameWindow> allWindows)
        {
            lock (lockObject)
            {
                var windowsList = allWindows.ToList();
                return windowsList.IndexOf(window);
            }
        }

        public bool IsWindowInFront(GameWindow window1, GameWindow window2, IReadOnlyList<GameWindow> allWindows)
        {
            lock (lockObject)
            {
                // インデックスが大きいほうが手前（Z-orderが高い）
                return GetWindowZIndex(window1, allWindows) > GetWindowZIndex(window2, allWindows);
            }
        }
    }
}