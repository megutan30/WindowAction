using System;
using System.Drawing;
using System.Threading.Tasks;
using MultiWindowActionGame.Windows;

namespace MultiWindowActionGame.Player
{
    public interface IPlayerWindowInteraction
    {
        GameWindow? CurrentParent { get; }
        GameWindow? LastValidParent { get; }
        Region MovableRegion { get; }
        void HandleWindowTransitions(Rectangle newBounds, Rectangle currentBounds);
        void SetParent(GameWindow? newParent);
        void SetOnParentChangedCallback(Action<GameWindow?> callback);
        (Rectangle adjustedBounds, bool hitCeiling) HandleWindowCollisions(Rectangle newBounds, Rectangle currentBounds);
        bool IsValidMove(Rectangle newBounds, GameWindow? currentParent);
        Rectangle AdjustMovement(Rectangle oldBounds, Rectangle newBounds, GameWindow? currentParent, Action? onCeilingHit);
        void OnMinimize();
        Region GetValidRegion(GameWindow? currentParent);
    }
}