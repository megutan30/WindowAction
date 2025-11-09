using System;

namespace MultiWindowActionGame.Player
{
    public interface IPlayerInputHandler
    {
        bool ShouldJump();
        bool IsMovingLeft();
        bool IsMovingRight();

        void HandleInput(IPlayerPhysics physics, Action onJumpTriggered);
        string UpdateFacing(string currentFacing);
    }
}