using System;
using System.Collections.Generic;
using System.Drawing;
using MultiWindowActionGame.Windows;

namespace MultiWindowActionGame.Managers
{
    public interface IWindowCollisionDetector
    {
        List<GameWindow> GetIntersectingWindows(Rectangle bounds, IReadOnlyList<GameWindow> allWindows);
        GameWindow? GetWindowAt(Rectangle bounds, IReadOnlyList<GameWindow> allWindows, GameWindow? currentWindow = null);
        GameWindow? GetTopWindowAt(Rectangle bounds, IReadOnlyList<GameWindow> allWindows, GameWindow? currentWindow);
        GameWindow? GetWindowFullyContaining(Rectangle bounds, IReadOnlyList<GameWindow> allWindows);
        GameWindow? GetNearestWindow(Rectangle bounds, IReadOnlyList<GameWindow> allWindows);
        bool IsWindowInFrontOf(GameWindow window1, GameWindow window2, IReadOnlyList<GameWindow> allWindows);
        float CalculateDistanceToWindow(Rectangle bounds, Rectangle windowBounds);
    }
}