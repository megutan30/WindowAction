using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace MultiWindowActionGame
{
    public class ZOrderManager
    {
        private const int HWND_NOTOPMOST = -2;
        private static readonly IntPtr HWND_TOP = new IntPtr(0);
        private static readonly IntPtr HWND_TOPMOST = new IntPtr(-1);
        private static readonly IntPtr HWND_BOTTOM = new IntPtr(1);
        private const uint SWP_NOMOVE = 0x0002;
        private const uint SWP_NOSIZE = 0x0001;

        // Z-order の優先順位
        public enum ZOrderPriority
        {
            DebugLayer = 6,
            Player = 5,
            Goal = 4,
            Button = 4,  // ゴールと同じ優先度
            WindowMark = 3,
            Window = 2,
            Bottom = 1
        }

        [DllImport("user32.dll")]
        private static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter,
            int X, int Y, int cx, int cy, uint uFlags);

        private readonly Dictionary<IntPtr, ZOrderPriority> handlePriorities = new();
        private readonly SortedDictionary<ZOrderPriority, List<Form>> formsByPriority = new();

        public void RegisterForm(Form form, ZOrderPriority priority)
        {
            handlePriorities[form.Handle] = priority;
            if (!formsByPriority.ContainsKey(priority))
            {
                formsByPriority[priority] = new List<Form>();
            }
            formsByPriority[priority].Add(form);
        }

        public void UnregisterForm(Form form)
        {
            if (handlePriorities.TryGetValue(form.Handle, out var priority))
            {
                handlePriorities.Remove(form.Handle);
                formsByPriority[priority].Remove(form);
            }
        }

        public void UpdateZOrder()
        {
            // 優先度の低い順に処理
            foreach (var priorityGroup in formsByPriority)
            {
                foreach (var form in priorityGroup.Value)
                {
                    if (!form.IsDisposed && form.Handle != IntPtr.Zero)
                    {
                        IntPtr insertAfter = GetInsertAfterHandle(priorityGroup.Key);
                        SetWindowPos(form.Handle, insertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                    }
                }
            }
        }

        private IntPtr GetInsertAfterHandle(ZOrderPriority priority)
        {
            return priority switch
            {
                ZOrderPriority.DebugLayer => HWND_TOPMOST,
                ZOrderPriority.Bottom => HWND_BOTTOM,
                _ => HWND_TOP
            };
        }

        public void BringToFront(Form form)
        {
            if (handlePriorities.TryGetValue(form.Handle, out var priority))
            {
                var list = formsByPriority[priority];
                list.Remove(form);
                list.Add(form);  // リストの最後に移動
                UpdateZOrder();
            }
        }
    }
}
