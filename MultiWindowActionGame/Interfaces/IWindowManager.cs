using MultiWindowActionGame.Managers;
using MultiWindowActionGame.Player;
using MultiWindowActionGame.Windows;
using MultiWindowActionGame.UI;
using MultiWindowActionGame.Core;
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace MultiWindowActionGame.Interfaces
{
    public interface IWindowManager
    {
        void Initialize();
        Task InitializeWindowsAsync(IEnumerable<GameWindow> windows);

        void SetPlayer(PlayerForm player);
        PlayerForm? GetPlayer();

        void RegisterWindow(GameWindow window);
        void ClearWindows();
        IReadOnlyList<GameWindow> GetAllWindows();
        IReadOnlyList<GameButton> GetAllButtons();

        Task UpdateAsync(float deltaTime);
        void Draw(Graphics g);
        void DrawMarks(Graphics g);
        void UpdateDisplay();

        Region CalculateMovableRegion(GameWindow? currentWindow);

        // Z-order管理
        void RegisterFormOrder(Form form, MultiWindowActionGame.Managers.ZOrderPriority priority);
        void UnregisterFormOrder(Form form);
        void UpdateFormZOrder(Form form, MultiWindowActionGame.Managers.ZOrderPriority priority);
        void BringWindowToFront(GameWindow window);

        // 衝突判定
        List<GameWindow> GetIntersectingWindows(Rectangle bounds);
        GameWindow? GetWindowAt(Rectangle bounds, GameWindow? currentWindow = null);
        GameWindow? GetTopWindowAt(Rectangle bounds, GameWindow? currentWindow);
        GameWindow? GetWindowFullyContaining(Rectangle bounds);
        GameWindow? GetNearestWindow(Rectangle bounds);

        // 階層管理
        GameWindow? GetParentWindow(IEffectTarget child);
        HashSet<IEffectTarget> GetContainedTargets(GameWindow window);
        void CheckPotentialParentWindow(GameWindow operatedWindow);

        // オブザーバーパターン
        void OnWindowChanged(GameWindow window, WindowChangeType changeType);

        // Z-orderユーティリティ
        int GetWindowZIndex(GameWindow window);
        void UpdateWindowGroupZOrder();
    }
}