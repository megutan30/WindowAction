using System;
using System.Collections.Generic;
using System.Windows.Forms;
using MultiWindowActionGame.Windows;
using MultiWindowActionGame.UI;

namespace MultiWindowActionGame.Managers
{
    public enum ZOrderPriority
    {
        DebugLayer = 1,
        Player = 2,
        Bottom = 3,
        Goal = 4,
        Button = 5,
        WindowMark = 6,
        Window = 7,
    }

    public interface IWindowZOrderManager
    {
        void RegisterFormOrder(Form form, ZOrderPriority priority);
        void UnregisterFormOrder(Form form);
        IReadOnlyList<GameWindow> BringWindowToFront(GameWindow window, IReadOnlyList<GameWindow> allWindows);
        void UpdateWindowGroupZOrder();
        void UpdateFormZOrder(Form form, ZOrderPriority priority);
        IReadOnlyList<GameButton> GetAllButtons();
        IReadOnlyDictionary<ZOrderPriority, IReadOnlyList<Form>> GetFormsByPriority();

        // Z-order比較メソッド群
        int CompareWindowZOrder(GameWindow window1, GameWindow window2, IReadOnlyList<GameWindow> allWindows);
        int GetWindowZIndex(GameWindow window, IReadOnlyList<GameWindow> allWindows);
        bool IsWindowInFront(GameWindow window1, GameWindow window2, IReadOnlyList<GameWindow> allWindows);
    }
}