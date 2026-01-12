using System;
using System.Drawing;
using MultiWindowActionGame.Core;
using MultiWindowActionGame.Interfaces;
using MultiWindowActionGame.Managers;
using static System.Net.Mime.MediaTypeNames;

namespace MultiWindowActionGame.Windows
{
    public class WindowFactory : IWindowFactory
    {
        private readonly IWindowManager windowManager;
        private readonly IGameSettings gameSettings;
        private readonly IInputService inputService;
        private readonly INoEntryZoneManager noEntryZoneManager;
        private readonly ICollisionService collisionService;
        private readonly NoEntryBoundaryCollider boundaryCollider;

        public WindowFactory(
            IWindowManager windowManager,
            IGameSettings gameSettings,
            IInputService inputService,
            INoEntryZoneManager noEntryZoneManager,
            ICollisionService collisionService,
            NoEntryBoundaryCollider boundaryCollider)
        {
            this.windowManager = windowManager ?? throw new ArgumentNullException(nameof(windowManager));
            this.gameSettings = gameSettings ?? throw new ArgumentNullException(nameof(gameSettings));
            this.inputService = inputService ?? throw new ArgumentNullException(nameof(inputService));
            this.noEntryZoneManager = noEntryZoneManager ?? throw new ArgumentNullException(nameof(noEntryZoneManager));
            this.collisionService = collisionService ?? throw new ArgumentNullException(nameof(collisionService));
            this.boundaryCollider = boundaryCollider ?? throw new ArgumentNullException(nameof(boundaryCollider));
        }

        public GameWindow CreateWindow(WindowType type, Point location, Size size, string? text = null, bool showImmediately = true)
        {
            IWindowStrategy strategy = type switch
            {
                WindowType.NormalBlack => new NormalWindowStrategy(inputService, gameSettings, noEntryZoneManager, collisionService, windowManager, boundaryCollider),
                WindowType.NormalWhite => new NormalWindowStrategy(inputService, gameSettings, noEntryZoneManager, collisionService, windowManager, boundaryCollider),
                WindowType.Resizable => new ResizableWindowStrategy(inputService, gameSettings, noEntryZoneManager, collisionService, windowManager, boundaryCollider),
                WindowType.Movable => new MovableWindowStrategy(inputService, gameSettings, noEntryZoneManager, collisionService, windowManager, boundaryCollider),
                WindowType.Deletable => new DeletableWindowStrategy(inputService, gameSettings, noEntryZoneManager, collisionService, windowManager, boundaryCollider),
                WindowType.Minimizable => new MinimizableWindowStrategy(inputService, gameSettings, noEntryZoneManager, collisionService, windowManager, boundaryCollider),
                WindowType.TextDisplay => new TextDisplayWindowStrategy(text ?? "NULL", inputService, gameSettings, noEntryZoneManager, collisionService, windowManager, boundaryCollider),
                WindowType.NoEntry => new NoEntryWindowStrategy(inputService, gameSettings, noEntryZoneManager, collisionService, windowManager, boundaryCollider),
                WindowType.ResizableNoEntry => new ResizableNoEntryWindowStrategy(inputService, gameSettings, noEntryZoneManager, collisionService, windowManager, boundaryCollider),
                WindowType.MovableNoEntry => new MovableNoEntryWindowStrategy(inputService, gameSettings, noEntryZoneManager, collisionService, windowManager, boundaryCollider),
                WindowType.MinimizableNoEntry => new MinimizableNoEntryWindowStrategy(inputService, gameSettings, noEntryZoneManager, collisionService, windowManager, boundaryCollider),
                WindowType.NormalBlackNoEntry => new NoEntryWindowStrategy(inputService, gameSettings, noEntryZoneManager, collisionService, windowManager, boundaryCollider),
                WindowType.NormalWhiteNoEntry => new NoEntryWindowStrategy(inputService, gameSettings, noEntryZoneManager, collisionService, windowManager, boundaryCollider),
                _ => throw new ArgumentException("Invalid window type", nameof(type))
            };

            var window = new GameWindow(location, size, strategy, windowManager, gameSettings, showImmediately);

            windowManager.RegisterWindow(window);

            window.FormBorderStyle = FormBorderStyle.FixedSingle;
            window.MinimizeBox = (type == WindowType.Minimizable || type == WindowType.MinimizableNoEntry);

            if (type == WindowType.Minimizable || type == WindowType.MinimizableNoEntry)
            {
                window.MinimizeBox = true;
            }

            window.BackColor = type switch
            {
                WindowType.NormalBlack => Color.Black,
                WindowType.NormalWhite => Color.White,
                WindowType.Resizable => Color.LightGreen,
                WindowType.Movable => Color.LightBlue,
                WindowType.Deletable => Color.LightPink,
                WindowType.Minimizable => Color.LightPink,
                WindowType.TextDisplay => Color.Black,
                WindowType.NoEntry => Color.DimGray,
                WindowType.ResizableNoEntry => Color.LightGreen,
                WindowType.MovableNoEntry => Color.LightBlue,
                WindowType.MinimizableNoEntry => Color.LightPink,
                WindowType.NormalBlackNoEntry => Color.Black,
                WindowType.NormalWhiteNoEntry => Color.White,
                _ => Color.White
            };

            if (type == WindowType.TextDisplay)
            {
                window.Paint += (sender, e) =>
                {
                    if (strategy is TextDisplayWindowStrategy textStrategy)
                    {
                        using (Font font = new Font(CustomFonts.PressStart.FontFamily, 12))
                        {
                            string text = textStrategy.GetDisplayText();
                            SizeF textSize = e.Graphics.MeasureString(text, font);
                            float x = (window.ClientSize.Width - textSize.Width) / 2;
                            float y = (window.ClientSize.Height - textSize.Height) / 2;
                            e.Graphics.DrawString(text, font, Brushes.White, x, y);
                        }
                    }
                };
            }

            // 不可侵ウィンドウの場合、フラグを設定して登録
            if (type == WindowType.NoEntry ||
                type == WindowType.ResizableNoEntry ||
                type == WindowType.MovableNoEntry ||
                type == WindowType.MinimizableNoEntry ||
                type == WindowType.NormalBlackNoEntry ||
                type == WindowType.NormalWhiteNoEntry)
            {
                window.IsNoEntryWindow = true;
                noEntryZoneManager.RegisterNoEntryWindow(window);
            }

            return window;
        }

        public Goal CreateGoal(Point location, bool isInFront = false)
        {
            var goal = new Goal(location, isInFront);
            return goal;
        }

        public NoEntryZone CreateNoEntryZone(Point location, Size size)
        {
            var noEntryZone = new NoEntryZone(location, size);
            return noEntryZone;
        }
    }

    public enum WindowType
    {
        NormalWhite,
        NormalBlack,
        Resizable,
        Movable,
        Deletable,
        Minimizable,
        TextDisplay,
        NoEntry,  // 不可侵ウィンドウ
        ResizableNoEntry,  // リサイズ可能 + 不可侵ウィンドウ
        MovableNoEntry,    // 移動可能 + 不可侵ウィンドウ
        MinimizableNoEntry, // 最小化可能 + 不可侵ウィンドウ
        NormalBlackNoEntry, // 通常ウィンドウ（黒）+ 不可侵ウィンドウ
        NormalWhiteNoEntry  // 通常ウィンドウ（白）+ 不可侵ウィンドウ
    }
}