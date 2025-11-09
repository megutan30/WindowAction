using System;
using System.Collections.Generic;
using MultiWindowActionGame.Windows;

namespace MultiWindowActionGame.Managers
{
    public interface IWindowHierarchyManager
    {
        GameWindow? GetParentWindow(IEffectTarget child);
        HashSet<IEffectTarget> GetContainedTargets(GameWindow window);
        void CheckPotentialParentWindow(GameWindow operatedWindow, IReadOnlyList<GameWindow> allWindows);
        void AddParentChildRelation(IEffectTarget child, GameWindow parent);
        void RemoveParentChildRelation(IEffectTarget child);
        void ClearAllRelations();
        IReadOnlyDictionary<IEffectTarget, GameWindow> GetAllRelations();
    }
}