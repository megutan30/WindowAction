using System;
using System.Collections.Generic;
using System.Drawing;

namespace MultiWindowActionGame.Player
{
    public class PlayerStateMachine : IPlayerStateMachine
    {
        private readonly Dictionary<PlayerStateType, IPlayerState> states;
        private IPlayerState currentState;

        public PlayerStateType CurrentStateType => currentState.StateType;
        public IPlayerState CurrentState => currentState;

        public PlayerStateMachine()
        {
            states = new Dictionary<PlayerStateType, IPlayerState>
            {
                { PlayerStateType.Normal, new NormalPlayerState() },
                { PlayerStateType.Jumping, new JumpingPlayerState() },
                { PlayerStateType.Falling, new FallingPlayerState() },
                { PlayerStateType.Grounded, new GroundedPlayerState() }
            };

            currentState = states[PlayerStateType.Normal];
            currentState.OnEnter();
        }

        public void ChangeState(PlayerStateType newStateType)
        {
            if (currentState.StateType == newStateType)
                return;

            currentState.OnExit();
            currentState = states[newStateType];
            currentState.OnEnter();
        }

        public void Update(float deltaTime)
        {
            currentState.Update(deltaTime);
        }

        public void Draw(Graphics g, Rectangle bounds)
        {
            currentState.Draw(g, bounds);
        }
    }

    // Individual state implementations
    public class NormalPlayerState : IPlayerState
    {
        public PlayerStateType StateType => PlayerStateType.Normal;

        public void OnEnter() { }
        public void OnExit() { }

        public void Update(float deltaTime)
        {
            // Normal state logic
        }

        public void Draw(Graphics g, Rectangle bounds)
        {
            // Normal state drawing - basic blue rectangle is drawn by PlayerForm
        }
    }

    public class JumpingPlayerState : IPlayerState
    {
        public PlayerStateType StateType => PlayerStateType.Jumping;

        public void OnEnter() { }
        public void OnExit() { }

        public void Update(float deltaTime)
        {
            // Jumping state logic
        }

        public void Draw(Graphics g, Rectangle bounds)
        {
            // ジャンプ状態のデバッグ描画は不要
        }
    }

    public class FallingPlayerState : IPlayerState
    {
        public PlayerStateType StateType => PlayerStateType.Falling;

        public void OnEnter() { }
        public void OnExit() { }

        public void Update(float deltaTime)
        {
            // Falling state logic
        }

        public void Draw(Graphics g, Rectangle bounds)
        {
            // 落下状態のデバッグ描画は不要
        }
    }

    public class GroundedPlayerState : IPlayerState
    {
        public PlayerStateType StateType => PlayerStateType.Grounded;

        public void OnEnter() { }
        public void OnExit() { }

        public void Update(float deltaTime)
        {
            // Grounded state logic
        }

        public void Draw(Graphics g, Rectangle bounds)
        {
            // 接地状態のデバッグ描画は不要
        }
    }
}