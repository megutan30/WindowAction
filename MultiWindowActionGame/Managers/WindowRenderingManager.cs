using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;
using MultiWindowActionGame.Player;
using MultiWindowActionGame.Windows;
using MultiWindowActionGame.Rendering;

namespace MultiWindowActionGame.Managers
{
    public class WindowRenderingManager : IWindowRenderingManager
    {
        private readonly object lockObject = new object();

        public void Draw(Graphics g, IEnumerable<IEffectTarget> allComponents)
        {
            lock (lockObject)
            {
                foreach (var target in allComponents)
                {
                    // 親を持つか、不可侵ウィンドウの場合はアウトラインを描画
                    bool shouldDrawOutline = target.Parent != null ||
                                             (target is GameWindow gw && gw.IsNoEntryWindow);

                    if (shouldDrawOutline && !(target is Goal) && !(target is PlayerForm))
                    {
                        int currentIndex = allComponents.ToList().IndexOf(target);
                        var coveringTargets = allComponents
                            .Skip(currentIndex + 1)
                            .Where(t => t.Bounds.IntersectsWith(target.Bounds));

                        // GameWindowの場合はCollisionBoundsを使用
                        if (target is GameWindow window)
                        {
                            OutlineRenderer.DrawClippedOutline(g, target, coveringTargets, window.CollisionBounds);
                        }
                        else
                        {
                            OutlineRenderer.DrawClippedOutline(g, target, coveringTargets, target.Bounds);
                        }
                    }

                    target.Draw(g);
                }
            }
        }

        public void DrawMarks(Graphics g, IReadOnlyList<GameWindow> allWindows)
        {
            lock (lockObject)
            {
                foreach (var window in allWindows)
                {
                    DrawWindowMark(g, window, allWindows);
                    if (window.Parent != null)
                    {
                        //DrawParentChildConnection(g, window);
                    }
                }
            }
        }

        public IEnumerable<IEffectTarget> GetAllComponents(
            IReadOnlyList<GameWindow> windows,
            PlayerForm? player,
            IReadOnlyDictionary<ZOrderPriority, IReadOnlyList<Form>> formsByPriority)
        {
            lock (lockObject)
            {
                var components = new List<IEffectTarget>();

                // ZOrderPriorityの順序に従ってコンポーネントを追加
                foreach (var priority in Enum.GetValues<ZOrderPriority>().OrderByDescending(p => (int)p))
                {
                    switch (priority)
                    {
                        case ZOrderPriority.Player:
                            if (player != null)
                            {
                                components.Add(player);
                            }
                            break;

                        case ZOrderPriority.Window:
                            components.AddRange(windows);
                            break;

                        case ZOrderPriority.Button:
                            if (formsByPriority.TryGetValue(ZOrderPriority.Button, out var buttonForms))
                            {
                                components.AddRange(buttonForms.OfType<IEffectTarget>());
                            }
                            break;

                        case ZOrderPriority.Goal:
                            if (formsByPriority.TryGetValue(ZOrderPriority.Goal, out var goalForms))
                            {
                                components.AddRange(goalForms.OfType<IEffectTarget>());
                            }
                            break;
                    }
                }

                return components;
            }
        }

        private void DrawWindowMark(Graphics g, GameWindow window, IReadOnlyList<GameWindow> allWindows)
        {
            Rectangle markBounds = window.CollisionBounds;

            // この領域と交差する、より前面にあるウィンドウを取得
            var windowsList = allWindows.ToList();
            var coveringWindows = windowsList
                .Where(w => w != window &&
                           windowsList.IndexOf(w) > windowsList.IndexOf(window) &&
                           w.CollisionBounds.IntersectsWith(markBounds))
                .ToList();

            // マウスの現在位置を取得
            Point mousePos = Cursor.Position;
            bool isHovered = window.RectangleToScreen(window.ClientRectangle).Contains(mousePos) ||
                             new Rectangle(window.Location, new Size(window.Width, window.RectangleToScreen(window.ClientRectangle).Y - window.Location.Y)).Contains(mousePos);

            // 前面のウィンドウがマウス位置と重なっているかチェック
            if (isHovered)
            {
                foreach (var coveringWindow in coveringWindows)
                {
                    if (coveringWindow.CollisionBounds.Contains(mousePos))
                    {
                        isHovered = false;
                        break;
                    }
                }
            }

            if (coveringWindows.Any())
            {
                using (Region clipRegion = new Region(markBounds))
                {
                    foreach (var coveringWindow in coveringWindows)
                    {
                        clipRegion.Exclude(coveringWindow.CollisionBounds);
                    }

                    Region originalClip = g.Clip;
                    g.Clip = clipRegion;

                    window.Strategy.DrawStrategyMark(g, window.CollisionBounds, isHovered);

                    g.Clip = originalClip;
                }
            }
            else
            {
                window.Strategy.DrawStrategyMark(g, window.CollisionBounds, isHovered);
            }
        }

        private void DrawParentChildConnection(Graphics g, GameWindow childWindow)
        {
            if (childWindow.Parent != null)
            {
                using (var pen = new Pen(Color.Yellow, 2))
                {
                    Point childCenter = new Point(
                        childWindow.Bounds.X + childWindow.Bounds.Width / 2,
                        childWindow.Bounds.Y + childWindow.Bounds.Height / 2
                    );
                    Point parentCenter = new Point(
                        childWindow.Parent.Bounds.X + childWindow.Parent.Bounds.Width / 2,
                        childWindow.Parent.Bounds.Y + childWindow.Parent.Bounds.Height / 2
                    );
                    g.DrawLine(pen, childCenter, parentCenter);
                }
            }
        }
    }
}