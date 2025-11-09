using System;
using System.Windows.Forms;
using MultiWindowActionGame.Interfaces;

namespace MultiWindowActionGame.Player
{
    public class PlayerInputHandler : IPlayerInputHandler
    {
        private readonly IInputService inputService;

        public PlayerInputHandler(IInputService inputService)
        {
            this.inputService = inputService ?? throw new ArgumentNullException(nameof(inputService));
        }

        public bool ShouldJump()
        {
            return inputService.IsKeyDown(Keys.Space) ||
                   inputService.IsKeyDown(Keys.Up) ||
                   inputService.IsKeyDown(Keys.W);
        }

        public bool IsMovingLeft()
        {
            return inputService.IsKeyDown(Keys.A) || inputService.IsKeyDown(Keys.Left);
        }

        public bool IsMovingRight()
        {
            return inputService.IsKeyDown(Keys.D) || inputService.IsKeyDown(Keys.Right);
        }

        public void HandleInput(IPlayerPhysics physics, Action onJumpTriggered)
        {
            if (physics.IsGrounded && ShouldJump())
            {
                physics.Jump();
                onJumpTriggered?.Invoke();
            }
        }

        public string UpdateFacing(string currentFacing)
        {
            if (IsMovingLeft())
            {
                return "left";
            }
            if (IsMovingRight())
            {
                return "right";
            }
            return currentFacing;
        }
    }
}