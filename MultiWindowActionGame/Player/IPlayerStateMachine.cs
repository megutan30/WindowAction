using System;
using System.Drawing;

namespace MultiWindowActionGame.Player
{
    public enum PlayerStateType
    {
        Normal,
        Jumping,
        Falling,
        Grounded
    }

    public interface IPlayerState
    {
        PlayerStateType StateType { get; }
        void Update(float deltaTime);
        void Draw(Graphics g, Rectangle bounds);
        void OnEnter();
        void OnExit();
    }

    public interface IPlayerStateMachine
    {
        PlayerStateType CurrentStateType { get; }
        IPlayerState CurrentState { get; }

        void ChangeState(PlayerStateType newStateType);
        void Update(float deltaTime);
        void Draw(Graphics g, Rectangle bounds);
    }
}