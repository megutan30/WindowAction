using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using MultiWindowActionGame.Windows;

namespace MultiWindowActionGame.Managers
{
    public class WindowCollisionDetector : IWindowCollisionDetector
    {
        private readonly object lockObject = new object();
        private readonly IWindowZOrderManager zOrderManager;

        public WindowCollisionDetector(IWindowZOrderManager zOrderManager)
        {
            this.zOrderManager = zOrderManager ?? throw new ArgumentNullException(nameof(zOrderManager));
        }

        public List<GameWindow> GetIntersectingWindows(Rectangle bounds, IReadOnlyList<GameWindow> allWindows)
        {
            lock (lockObject)
            {
                return allWindows
                    .Where(w => w.CollisionBounds.IntersectsWith(bounds))
                    .OrderByDescending(w => zOrderManager.GetWindowZIndex(w, allWindows))
                    .ToList();
            }
        }

        public GameWindow? GetWindowAt(Rectangle bounds, IReadOnlyList<GameWindow> allWindows, GameWindow? currentWindow = null)
        {
            lock (lockObject)
            {
                // 親ウィンドウのみを検索対象とする
                var topLevelWindows = allWindows.Where(w => w.Parent == null);
                foreach (var window in topLevelWindows.Reverse())
                {
                    if (window == currentWindow) continue;
                    if (window.CollisionBounds.Contains(bounds))
                    {
                        return window;
                    }
                }
                return null;
            }
        }

        public GameWindow? GetTopWindowAt(Rectangle bounds, IReadOnlyList<GameWindow> allWindows, GameWindow? currentWindow)
        {
            Point[] checkPoints = new Point[]
            {
                new Point(bounds.Left, bounds.Bottom),
                new Point(bounds.Right, bounds.Bottom),
                new Point(bounds.Left, bounds.Top),
                new Point(bounds.Right, bounds.Top),
                new Point(bounds.X + bounds.Width / 2, bounds.Y + bounds.Height / 2)
            };

            lock (lockObject)
            {
                var windowsList = allWindows.ToList();

                if (currentWindow == null)
                {
                    return windowsList
                        .Where(w => IsWindowContainedWithinBounds(w.CollisionBounds, bounds))
                        .OrderByDescending(w => zOrderManager.GetWindowZIndex(w, allWindows))
                        .FirstOrDefault();
                }

                Dictionary<GameWindow, HashSet<Point>> windowPoints = new Dictionary<GameWindow, HashSet<Point>>();

                foreach (var point in checkPoints)
                {
                    for (int i = windowsList.Count - 1; i >= 0; i--)
                    {
                        var window = windowsList[i];
                        if (IsPointWithinBounds(window.CollisionBounds, point))
                        {
                            if (!windowPoints.ContainsKey(window))
                            {
                                windowPoints[window] = new HashSet<Point>();
                            }
                            windowPoints[window].Add(point);
                            break;
                        }
                    }
                }

                if (!windowPoints.Any()) return null;

                var currentWindowPoints = windowPoints.GetValueOrDefault(currentWindow, new HashSet<Point>());

                if (currentWindowPoints.Any())
                {
                    var candidateWindows = windowPoints
                        .Where(w => w.Key != currentWindow &&
                                  (w.Value.Contains(checkPoints[0]) || w.Value.Contains(checkPoints[1])))
                        .Select(w => new
                        {
                            Window = w.Key,
                            Points = w.Value,
                            BottomEdge = w.Key.CollisionBounds.Bottom
                        })
                        .OrderBy(w => w.BottomEdge)
                        .ThenByDescending(w => zOrderManager.GetWindowZIndex(w.Window, allWindows))
                        .ToList();

                    if (candidateWindows.Any())
                    {
                        var bestWindow = candidateWindows.First();
                        if (bestWindow.BottomEdge <= currentWindow.CollisionBounds.Bottom)
                        {
                            return bestWindow.Window;
                        }
                    }

                    return currentWindow;
                }
                else
                {
                    var candidateWindows = windowPoints
                        .Where(w => w.Value.Contains(checkPoints[0]) || w.Value.Contains(checkPoints[1]))
                        .Select(w => new
                        {
                            Window = w.Key,
                            Points = w.Value,
                            BottomEdge = w.Key.CollisionBounds.Bottom
                        })
                        .OrderBy(w => w.BottomEdge)
                        .ThenByDescending(w => zOrderManager.GetWindowZIndex(w.Window, allWindows))
                        .ToList();

                    if (candidateWindows.Any())
                    {
                        return candidateWindows.First().Window;
                    }
                }
            }

            return currentWindow;
        }

        public GameWindow? GetWindowFullyContaining(Rectangle bounds, IReadOnlyList<GameWindow> allWindows)
        {
            lock (lockObject)
            {
                return allWindows
                    .Where(w => IsWindowContainedWithinBounds(w.CollisionBounds, bounds))
                    .OrderByDescending(w => zOrderManager.GetWindowZIndex(w, allWindows))
                    .FirstOrDefault();
            }
        }

        private static bool IsWindowContainedWithinBounds(Rectangle containerBounds, Rectangle containedBounds)
        {
            // 境界ぴったりを除外して厳密な包含判定（めり込み防止）
            return containedBounds.Left > containerBounds.Left &&
                   containedBounds.Top > containerBounds.Top &&
                   containedBounds.Right < containerBounds.Right &&
                   containedBounds.Bottom < containerBounds.Bottom;
        }

        private static bool IsPointWithinBounds(Rectangle bounds, Point point)
        {
            // 境界上の点を除外して厳密な内部判定（めり込み防止）
            return point.X > bounds.Left &&
                   point.X < bounds.Right &&
                   point.Y > bounds.Top &&
                   point.Y < bounds.Bottom;
        }

        public GameWindow? GetNearestWindow(Rectangle bounds, IReadOnlyList<GameWindow> allWindows)
        {
            GameWindow? nearestWindow = null;
            float minDistance = float.MaxValue;

            lock (lockObject)
            {
                foreach (var window in allWindows)
                {
                    float distance = CalculateDistanceToWindow(bounds, window.CollisionBounds);
                    if (distance < minDistance)
                    {
                        minDistance = distance;
                        nearestWindow = window;
                    }
                }
            }

            return nearestWindow;
        }

        public bool IsWindowInFrontOf(GameWindow window1, GameWindow window2, IReadOnlyList<GameWindow> allWindows)
        {
            lock (lockObject)
            {
                return zOrderManager.IsWindowInFront(window1, window2, allWindows);
            }
        }

        public float CalculateDistanceToWindow(Rectangle bounds, Rectangle windowBounds)
        {
            float dx = Math.Max(0, Math.Max(windowBounds.Left - bounds.Right, bounds.Left - windowBounds.Right));
            float dy = Math.Max(0, Math.Max(windowBounds.Top - bounds.Bottom, bounds.Top - windowBounds.Bottom));
            return (float)Math.Sqrt(dx * dx + dy * dy);
        }
    }
}