using System;
using System.Threading.Tasks;
using MultiWindowActionGame.Player;
using MultiWindowActionGame.Windows;
using MultiWindowActionGame.Core;

namespace MultiWindowActionGame.Interfaces
{
    public interface IStageManager
    {
        Goal? CurrentGoal { get; }
        int CurrentStageIndex { get; }

        Task StartStageAsync(int stageNumber);
        void RestartCurrentStage();
        void ToTitleStage();
        StageData GetStage(int stageNumber);
        StageData? GetCurrentStage();  // 現在のステージデータを取得
        bool CheckGoal(PlayerForm player);
        void StartNextStage();
    }
}