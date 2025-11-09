using System;
using System.Collections.Generic;
using System.Linq;
using MultiWindowActionGame.Windows;

namespace MultiWindowActionGame.Managers
{
    public class WindowHierarchyManager : IWindowHierarchyManager
    {
        private readonly Dictionary<IEffectTarget, GameWindow> parentChildRelations = new Dictionary<IEffectTarget, GameWindow>();
        private readonly object lockObject = new object();
        private readonly IWindowZOrderManager zOrderManager;

        public WindowHierarchyManager(IWindowZOrderManager zOrderManager)
        {
            this.zOrderManager = zOrderManager ?? throw new ArgumentNullException(nameof(zOrderManager));
        }

        public GameWindow? GetParentWindow(IEffectTarget child)
        {
            lock (lockObject)
            {
                return parentChildRelations.TryGetValue(child, out var parent) ? parent : null;
            }
        }

        public HashSet<IEffectTarget> GetContainedTargets(GameWindow window)
        {
            lock (lockObject)
            {
                return new HashSet<IEffectTarget>(
                    parentChildRelations
                        .Where(kv => kv.Value == window)
                        .Select(kv => kv.Key)
                );
            }
        }

        public void CheckPotentialParentWindow(GameWindow operatedWindow, IReadOnlyList<GameWindow> allWindows)
        {
            lock (lockObject)
            {
                // 現在の親子関係が有効かチェック
                if (operatedWindow.Parent != null)
                {
                    if (!IsWindowContainedWithinBounds(operatedWindow.Parent.AdjustedBounds, operatedWindow.AdjustedBounds))
                    {
                        var oldParent = operatedWindow.Parent;
                        oldParent.RemoveChild(operatedWindow);
                        RemoveParentChildRelation(operatedWindow);
                        Console.WriteLine($"Window {operatedWindow.Id} detached from parent {oldParent.Id}");
                    }
                }

                // 子ウィンドウとの関係をチェック
                foreach (var child in operatedWindow.Children.OfType<GameWindow>().ToList())
                {
                    if (!IsWindowContainedWithinBounds(operatedWindow.AdjustedBounds, child.AdjustedBounds))
                    {
                        operatedWindow.RemoveChild(child);
                        RemoveParentChildRelation(child);
                        Console.WriteLine($"Child {child.Id} detached from parent {operatedWindow.Id}");
                    }
                }

                // 既存の親子グループを取得
                var existingGroup = new HashSet<GameWindow>();
                if (operatedWindow.Children.Any())
                {
                    existingGroup.Add(operatedWindow);
                    existingGroup.UnionWith(operatedWindow.GetAllDescendants());
                }

                // 親候補を探す（Z-orderで奥にあるウィンドウのみ - レガシー実装と同じロジック）
                var allPotentialParents = allWindows
                    .Where(w => !existingGroup.Contains(w) && w != operatedWindow &&
                           zOrderManager.IsWindowInFront(operatedWindow, w, allWindows))
                    .OrderByDescending(w => zOrderManager.GetWindowZIndex(w, allWindows));

                // 完全に含んでいる最も手前のウィンドウを探す
                GameWindow? bestParent = null;
                int bestParentIndex = -1;

                foreach (var potentialParent in allPotentialParents)
                {
                    if (IsWindowContainedWithinBounds(potentialParent.AdjustedBounds, operatedWindow.AdjustedBounds))
                    {
                        int currentIndex = zOrderManager.GetWindowZIndex(potentialParent, allWindows);
                        if (bestParentIndex < currentIndex)
                        {
                            // 現在の親より良い候補が見つかった場合
                            if (operatedWindow.Parent == null ||
                                zOrderManager.IsWindowInFront(potentialParent, operatedWindow.Parent, allWindows))
                            {
                                bestParent = potentialParent;
                                bestParentIndex = currentIndex;
                            }
                        }
                    }
                }

                // より適切な親が見つかった場合は親子関係を変更
                if (bestParent != null && bestParent != operatedWindow.Parent)
                {
                    // 既存の親子関係を解除
                    if (operatedWindow.Parent != null)
                    {
                        var oldParent = operatedWindow.Parent;
                        oldParent.RemoveChild(operatedWindow);
                        RemoveParentChildRelation(operatedWindow);
                        Console.WriteLine($"Window {operatedWindow.Id} detached from old parent {oldParent.Id}");
                    }

                    // 新しい親子関係を設定
                    bestParent.AddChild(operatedWindow);
                    AddParentChildRelation(operatedWindow, bestParent);
                    Console.WriteLine($"Window {operatedWindow.Id} became child of new parent {bestParent.Id}");
                    return;
                }

                // 子ウィンドウを探す処理
                var potentialChildren = allWindows
                    .Where(w => !existingGroup.Contains(w) && w != operatedWindow &&
                           zOrderManager.GetWindowZIndex(w, allWindows) > zOrderManager.GetWindowZIndex(operatedWindow, allWindows))
                    .OrderBy(w => zOrderManager.GetWindowZIndex(w, allWindows));

                foreach (var potentialChild in potentialChildren)
                {
                    if (IsWindowContainedWithinBounds(operatedWindow.AdjustedBounds, potentialChild.AdjustedBounds))
                    {
                        // 既存の親子関係があっても、より適切な親となれる場合は変更
                        if (potentialChild.Parent == null ||
                            zOrderManager.IsWindowInFront(operatedWindow, potentialChild.Parent, allWindows))
                        {
                            if (potentialChild.Parent != null)
                            {
                                potentialChild.Parent.RemoveChild(potentialChild);
                                RemoveParentChildRelation(potentialChild);
                            }
                            operatedWindow.AddChild(potentialChild);
                            AddParentChildRelation(potentialChild, operatedWindow);
                            Console.WriteLine($"Window {operatedWindow.Id} became parent of {potentialChild.Id}");
                        }
                    }
                }
            }
        }

        public void AddParentChildRelation(IEffectTarget child, GameWindow parent)
        {
            lock (lockObject)
            {
                parentChildRelations[child] = parent;
            }
        }

        public void RemoveParentChildRelation(IEffectTarget child)
        {
            lock (lockObject)
            {
                parentChildRelations.Remove(child);
            }
        }

        public void ClearAllRelations()
        {
            lock (lockObject)
            {
                parentChildRelations.Clear();
            }
        }

        public IReadOnlyDictionary<IEffectTarget, GameWindow> GetAllRelations()
        {
            lock (lockObject)
            {
                return new Dictionary<IEffectTarget, GameWindow>(parentChildRelations);
            }
        }

        private static bool IsWindowContainedWithinBounds(Rectangle containerBounds, Rectangle containedBounds)
        {
            return containedBounds.Left >= containerBounds.Left &&
                   containedBounds.Top >= containerBounds.Top &&
                   containedBounds.Right <= containerBounds.Right &&
                   containedBounds.Bottom <= containerBounds.Bottom;
        }
    }
}