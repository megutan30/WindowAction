using System;
using System.Collections.Generic;
using System.Drawing;
using MultiWindowActionGame.Player;
using MultiWindowActionGame.Windows;

namespace MultiWindowActionGame.Managers
{
    public interface IWindowRenderingManager
    {
        void Draw(Graphics g, IEnumerable<IEffectTarget> allComponents);
        void DrawMarks(Graphics g, IReadOnlyList<GameWindow> allWindows);
        IEnumerable<IEffectTarget> GetAllComponents(
            IReadOnlyList<GameWindow> windows,
            PlayerForm? player,
            IReadOnlyDictionary<ZOrderPriority, IReadOnlyList<Form>> formsByPriority);
    }
}