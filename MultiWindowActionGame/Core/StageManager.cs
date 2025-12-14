using MultiWindowActionGame.Interfaces;
using MultiWindowActionGame.Player;
using MultiWindowActionGame.UI;
using MultiWindowActionGame.Windows;
using MultiWindowActionGame.Core;
using MultiWindowActionGame.Utilities;
using System.Diagnostics;
using System.Numerics;

namespace MultiWindowActionGame.Core
{
    public class StageManager : IStageManager
    {
        // インスタンス統一化のためのstatic参照
        private static StageManager? _current;
        public static StageManager Current
        {
            get => _current ?? throw new InvalidOperationException("StageManager is not initialized");
            internal set => _current = value;
        }

        // DI対応のフィールド
        private readonly IWindowManager? windowManager;
        private readonly INoEntryZoneManager? noEntryZoneManager;
        private IMainGame? mainGame;
        private IButtonFactory? buttonFactory;
        private IWindowFactory? windowFactory;

        private int currentStage = 0;
        private Goal? currentGoal;
        public Goal? CurrentGoal => currentGoal;
        public int CurrentStageIndex => currentStage;
        private List<StageData> stages = new List<StageData>();
        private RetryButton? currentRetryButton;
        private StartButton? currentStartButton;
        private ToTitleButton? currentToTitaleButton;
        private ExitButton? currentExitButton;
        private List<GameWindow> currentStageWindows = new List<GameWindow>();
        private enum StageInitializationState
        {
            NotStarted,
            WindowsInitialized,
            UIInitialized,
            PlayerInitialized,
            Completed
        }

        // ButtonFactoryを設定するメソッド
        public void SetButtonFactory(IButtonFactory buttonFactory)
        {
            this.buttonFactory = buttonFactory;
        }

        // WindowFactoryを設定するメソッド
        public void SetWindowFactory(IWindowFactory windowFactory)
        {
            this.windowFactory = windowFactory;
        }

        // DI対応コンストラクタ
        public StageManager(IWindowManager windowManager, INoEntryZoneManager noEntryZoneManager)
        {
            this.windowManager = windowManager ?? throw new ArgumentNullException(nameof(windowManager));
            this.noEntryZoneManager = noEntryZoneManager ?? throw new ArgumentNullException(nameof(noEntryZoneManager));
            InitializeStages();
            Current = this;
        }

        public void SetMainGame(IMainGame mainGame)
        {
            this.mainGame = mainGame ?? throw new ArgumentNullException(nameof(mainGame));
        }


        private void InitializeStages()
        {
            //不可侵ウィンドウ
            stages.Add(new StageData
            {
                Windows = new List<(WindowType type, Point location, Size size, string? text)>
            {
                (WindowType.NormalBlack, new Point(500, 250), new Size(500, 600),null),
                (WindowType.NormalBlack, new Point(900, 250), new Size(500, 600),null),
                (WindowType.NormalBlackNoEntry, new Point(125, 650), new Size(625, 350),null),
                (WindowType.NormalBlackNoEntry, new Point(1250, 650), new Size(625, 350),null),
                (WindowType.TextDisplay, new Point(625, 63), new Size(375, 125), "Stage 2"),
                //(WindowType.Minimizable, new Point(300, 250), new Size(450, 300),null),
            },
                GoalPosition = new Point(625, 375),
                GoalInFront = true,
                PlayerStartPosition = new Point(188, 813),
                NoEntryZones = new List<(Point, Size)>
                {
                },

                ToTitaleButtonPosition = new Point(106, 113),
                RetryButtonPosition = new Point(369, 113),
            });

            //不可侵ウィンドウ
            stages.Add(new StageData
            {
                Windows = new List<(WindowType type, Point location, Size size, string? text)>
            {
                (WindowType.NormalBlack, new Point(500, 550), new Size(500, 300),null),
                (WindowType.NormalBlack, new Point(900, 550), new Size(500, 300),null),
                (WindowType.NormalBlackNoEntry, new Point(125, 750), new Size(625, 250),null),
                (WindowType.NormalBlackNoEntry, new Point(1250, 750), new Size(625, 250),null),
                (WindowType.TextDisplay, new Point(625, 63), new Size(375, 125), "Stage 2"),
                //(WindowType.Minimizable, new Point(300, 250), new Size(450, 300),null),
            },
                GoalPosition = new Point(1625, 875),
                GoalInFront = true,
                PlayerStartPosition = new Point(188, 813),
                NoEntryZones = new List<(Point, Size)>
                {
                },

                ToTitaleButtonPosition = new Point(106, 113),
                RetryButtonPosition = new Point(369, 113),
            });

            //ムーバル + 不可侵ウィンドウバリエーション
            stages.Add(new StageData
            {
                Windows = new List<(WindowType type, Point location, Size size, string? text)>
            {
                (WindowType.NormalBlackNoEntry, new Point(125, 750), new Size(625, 250),null),
                (WindowType.Movable, new Point(688, 763), new Size(200, 150),null),
                (WindowType.NormalBlack, new Point(1250, 750), new Size(625, 250),null),
                (WindowType.Resizable, new Point(100, 200), new Size(1500, 550),null),
                (WindowType.MovableNoEntry, new Point(500, 400), new Size(180, 120),null),
                (WindowType.TextDisplay, new Point(625, 63), new Size(375, 125), "Stage 2"),
                //(WindowType.Minimizable, new Point(300, 250), new Size(450, 300),null),
            },
                GoalPosition = new Point(1625, 875),
                GoalInFront = true,
                PlayerStartPosition = new Point(188, 813),
                NoEntryZones = new List<(Point, Size)>
                {
                },

                ToTitaleButtonPosition = new Point(106, 113),
                RetryButtonPosition = new Point(369, 113),
            });

            // ステージデータの初期化
            // タイトルステージ（インデックス0）
            stages.Add(new StageData
            {
                Windows = new List<(WindowType type, Point location, Size size, string? text)>
            {
                (WindowType.NoEntry, new Point(625, 750), new Size(625, 188),null),
                (WindowType.NormalBlack, new Point(250, 563), new Size(500, 250),null),
                (WindowType.NormalBlack, new Point(1125, 563), new Size(500, 250),null),
                (WindowType.Movable, new Point(250, 375), new Size(500, 250),null),
                (WindowType.NormalBlack, new Point(1125, 375), new Size(500, 250),null),
                (WindowType.TextDisplay, new Point(625, 150), new Size(625, 313), "Window Action Game"),
                // 必要に応じて他のウィンドウを追加
            },
                PlayerStartPosition = new Point(913, 813),
                StartButtonPosition = new Point(850, 500),     // 少し左に移動
                ExitButtonPosition = new Point(850, 625),
                IsTitleStage = true,
            });

            //Stage1
            //操作方法と移動をさせる
            stages.Add(new StageData
            {
                Windows = new List<(WindowType type, Point location, Size size, string? text)>
            {

                (WindowType.NormalBlack, new Point(125, 750), new Size(625, 250),null),
                (WindowType.NormalWhite, new Point(625, 625), new Size(750, 250),null),
                (WindowType.TextDisplay, new Point(625, 63), new Size(375, 125), "Stage 1"),
            },
                GoalPosition = new Point(1250, 750),
                GoalInFront = true,
                PlayerStartPosition = new Point(188, 813),
                NoEntryZones = new List<(Point, Size)>
                {
                },
                ToTitaleButtonPosition = new Point(106, 113),
                RetryButtonPosition = new Point(369, 113),
            });
            //Stage2
            //ムーバル
            stages.Add(new StageData
            {
                Windows = new List<(WindowType type, Point location, Size size, string? text)>
            {
                (WindowType.NormalBlack, new Point(125, 750), new Size(625, 250),null),
                (WindowType.Movable, new Point(688, 763), new Size(250, 250),null),
                (WindowType.NormalBlack, new Point(1250, 750), new Size(625, 250),null),
                (WindowType.TextDisplay, new Point(625, 63), new Size(375, 125), "Stage 2"),
            },
                GoalPosition = new Point(1625, 875),
                GoalInFront = true,
                PlayerStartPosition = new Point(188, 813),
                NoEntryZones = new List<(Point, Size)>
                {
                },

                ToTitaleButtonPosition = new Point(106, 113),
                RetryButtonPosition = new Point(369, 113),
            });
            //Stage3
            //リサイズウィンドウ
            stages.Add(new StageData
            {
                Windows = new List<(WindowType type, Point location, Size size, string? text)>
            {
                (WindowType.NormalBlack, new Point(125, 245), new Size(625, 250),null),
                (WindowType.Resizable, new Point(725, 250), new Size(250, 250),null),
                (WindowType.NormalBlack, new Point(1250, 750), new Size(625, 250),null),
                (WindowType.TextDisplay, new Point(625, 63), new Size(375, 125), "Stage 3"),
            },
                GoalPosition = new Point(1625, 875),
                GoalInFront = true,
                PlayerStartPosition = new Point(188, 313),
                NoEntryZones = new List<(Point, Size)>
                {
                },

                ToTitaleButtonPosition = new Point(106, 113),
                RetryButtonPosition = new Point(369, 113),
            });
            //Stage4
            //Zバッファが当たり判定
            stages.Add(new StageData
            {
                Windows = new List<(WindowType type, Point location, Size size, string? text)>
            {
                (WindowType.NormalBlack, new Point(63, 838), new Size(625, 250),null),
                (WindowType.NormalWhite, new Point(563, 675), new Size(625, 250),null),
                (WindowType.NormalBlack, new Point(1063, 513), new Size(625, 250),null),
                (WindowType.NormalWhite, new Point(563, 350), new Size(625, 250),null),
                (WindowType.NormalBlack, new Point(63, 188), new Size(625, 250),null),
                (WindowType.TextDisplay, new Point(625, 63), new Size(375, 125), "Stage 4"),
            },
                GoalPosition = new Point(125, 1000),
                GoalInFront = true,
                PlayerStartPosition = new Point(438, 355),
                NoEntryZones = new List<(Point, Size)>
                {
                },

                ToTitaleButtonPosition = new Point(106, 113),
                RetryButtonPosition = new Point(369, 113),
            });
            //Stage5
            //Zバッファ2
            stages.Add(new StageData
            {
                Windows = new List<(WindowType type, Point location, Size size, string? text)>
            {
                (WindowType.NormalBlack, new Point(63, 838), new Size(1000, 250),null),
                (WindowType.NormalWhite, new Point(63, 675), new Size(625, 250),null),
                (WindowType.NormalBlack, new Point(63, 513), new Size(625, 250),null),
                (WindowType.NormalWhite, new Point(63, 350), new Size(625, 250),null),
                (WindowType.NormalBlack, new Point(63, 188), new Size(625, 250),null),


                (WindowType.NormalBlack, new Point(1063, 188), new Size(625, 250),null),
                (WindowType.NormalWhite, new Point(1063, 350), new Size(625, 250),null),
                (WindowType.NormalBlack, new Point(1063, 513), new Size(625, 250),null),
                (WindowType.NormalWhite, new Point(1063, 675), new Size(625, 250),null),
                (WindowType.NormalBlack, new Point(688, 838), new Size(1000, 250),null),

                (WindowType.TextDisplay, new Point(625, 63), new Size(375, 125), "Stage 5"),
            },
                GoalPosition = new Point(1500, 250),
                GoalInFront = true,
                PlayerStartPosition = new Point(313, 250),
                NoEntryZones = new List<(Point, Size)>
                {
                },

                ToTitaleButtonPosition = new Point(106, 113),
                RetryButtonPosition = new Point(369, 113),
            });
            //Stage3
            //ゴール移動
            stages.Add(new StageData
            {
                Windows = new List<(WindowType type, Point location, Size size, string? text)>
            {
                (WindowType.NormalBlack, new Point(125, 750), new Size(375, 250),null),
                (WindowType.Movable, new Point(1500, 125), new Size(250, 250),null),
                (WindowType.TextDisplay, new Point(625, 63), new Size(375, 125), "Stage 6"),
            },
                GoalPosition = new Point(1625, 250),
                GoalInFront = true,
                PlayerStartPosition = new Point(188, 813),
                NoEntryZones = new List<(Point, Size)>
            {
                (new Point(825, 375), new Size(125, 750)),
                (new Point(1200, 0), new Size(125, 750)),
                (new Point(1200, 0), new Size(125, 750)),
            },

                ToTitaleButtonPosition = new Point(106, 113),
                RetryButtonPosition = new Point(369, 113),
            });

            //リサイズウィンドウ2
            stages.Add(new StageData
            {
                Windows = new List<(WindowType type, Point location, Size size, string? text)>
            {
                (WindowType.Resizable, new Point(725, 375), new Size(250, 813),null),
                (WindowType.NormalBlack, new Point(725, 125), new Size(250, 313),null),
                (WindowType.TextDisplay, new Point(625, 63), new Size(375, 125), "Stage 7"),
            },
                GoalPosition = new Point(750, 250),
                GoalInFront = true,
                PlayerStartPosition = new Point(750, 938),
                NoEntryZones = new List<(Point, Size)>
                {
                },

                ToTitaleButtonPosition = new Point(106, 113),
                RetryButtonPosition = new Point(369, 113),
            });

            //リサイズウィンドウ3
            stages.Add(new StageData
            {
                Windows = new List<(WindowType type, Point location, Size size, string? text)>
            {
                (WindowType.Resizable, new Point(1100, 63), new Size(250, 250),null),
                (WindowType.NormalBlack, new Point(100, 750), new Size(1063, 250),null),
                (WindowType.TextDisplay, new Point(625, 63), new Size(375, 125), "Stage 8"),
            },
                GoalPosition = new Point(1125, 125),
                GoalInFront = true,
                PlayerStartPosition = new Point(125, 875),
                NoEntryZones = new List<(Point, Size)>
                {
                },

                ToTitaleButtonPosition = new Point(106, 113),
                RetryButtonPosition = new Point(369, 113),
            });

            //Stage6
            //最初から親子関係のあるウィンドウ
            stages.Add(new StageData
            {
                Windows = new List<(WindowType type, Point location, Size size, string? text)>
            {
                (WindowType.Resizable, new Point(938, 125), new Size(938, 750),null),
                (WindowType.NormalBlack, new Point(969, 188), new Size(375, 250),null),
                (WindowType.NormalBlack, new Point(313, 125), new Size(375, 188),null),
                (WindowType.TextDisplay, new Point(625, 63), new Size(375, 125), "Stage 9"),
            },
                GoalPosition = new Point(375, 225),
                GoalInFront = true,
                PlayerStartPosition = new Point(1313, 750),
                NoEntryZones = new List<(Point, Size)>
                {
                },

                ToTitaleButtonPosition = new Point(106, 50),
                RetryButtonPosition = new Point(369, 50),
            });
            //親子関係2
            //ムーバル
            stages.Add(new StageData
            {
                Windows = new List<(WindowType type, Point location, Size size, string? text)>
            {
                (WindowType.NormalBlack, new Point(1000, 375), new Size(625, 250),null),
                (WindowType.Movable, new Point(188, 388), new Size(625, 625),null),
                (WindowType.NormalBlack, new Point(375, 500), new Size(250, 250),null),
                (WindowType.TextDisplay, new Point(625, 63), new Size(375, 125), "Stage 10"),
            },
                GoalPosition = new Point(469, 543),
                GoalInFront = true,
                PlayerStartPosition = new Point(313, 813),
                NoEntryZones = new List<(Point, Size)>
                {
                },

                ToTitaleButtonPosition = new Point(106, 113),
                RetryButtonPosition = new Point(369, 113),
            });
            //親子関係3
            //ムーバル
            stages.Add(new StageData
            {
                Windows = new List<(WindowType type, Point location, Size size, string? text)>
            {
                (WindowType.Movable, new Point(188, 368), new Size(625, 625),null),
                (WindowType.TextDisplay, new Point(625, 63), new Size(375, 125), "Stage 11"),
            },
                GoalPosition = new Point(419, 543),
                GoalInFront = true,
                PlayerStartPosition = new Point(313, 813),
                NoEntryZones = new List<(Point, Size)>
                {
                },

                ToTitaleButtonPosition = new Point(106, 113),
                RetryButtonPosition = new Point(369, 113),
            });
            //親子関係4
            //ムーバル
            stages.Add(new StageData
            {
                Windows = new List<(WindowType type, Point location, Size size, string? text)>
            {
                (WindowType.Movable, new Point(188, 388), new Size(625, 625),null),
            },
                GoalPosition = new Point(419, 563),
                GoalInFront = true,
                PlayerStartPosition = new Point(313, 813),
                NoEntryZones = new List<(Point, Size)>
                {
                },

                ToTitaleButtonPosition = new Point(106, 113),
                RetryButtonPosition = new Point(369, 113),
            });
            //Stage7
            //親子関係を利用する
            stages.Add(new StageData
            {
                Windows = new List<(WindowType type, Point location, Size size, string? text)>
            {
                (WindowType.Movable, new Point(63, 750), new Size(250, 250),null),
                (WindowType.Resizable, new Point(63, 250), new Size(500, 500),null),
                (WindowType.NormalBlack, new Point(1250, 750), new Size(625, 250),null),
                (WindowType.TextDisplay, new Point(625, 63) , new Size(375, 125), "Stage 13"),
            },
                GoalPosition = new Point(1625, 875),
                GoalInFront = true,
                PlayerStartPosition = new Point(188, 813),
                NoEntryZones = new List<(Point, Size)>
            {
                (new Point(1075, 0), new Size(125, 500)),
                (new Point(1075, 625), new Size(125, 500)),
            },

                ToTitaleButtonPosition = new Point(106, 113),
                RetryButtonPosition = new Point(369, 113),
            });
            //Stage8
            //最小化
            stages.Add(new StageData
            {
                Windows = new List<(WindowType type, Point location, Size size, string? text)>
            {
                (WindowType.NormalBlack, new Point(625, 313), new Size(625, 725),null),
                (WindowType.Minimizable, new Point(625, 375), new Size(625, 375),null),
                (WindowType.TextDisplay, new Point(625, 63), new Size(375, 125), "Stage 14"),
            },
                GoalPosition = new Point(750, 875),
                GoalInFront = true,
                PlayerStartPosition = new Point(750, 500),
                NoEntryZones = new List<(Point, Size)>
                {
                },

                ToTitaleButtonPosition = new Point(106, 113),
                RetryButtonPosition = new Point(369, 113),
            });
            //Stage9
            //ウィンドウ外に出る
            stages.Add(new StageData
            {
                Windows = new List<(WindowType type, Point location, Size size, string? text)>
            {
                (WindowType.Minimizable, new Point(250, 750), new Size(375, 250),null),
                (WindowType.NormalBlack, new Point(625, 594), new Size(375, 250),null),
                (WindowType.NormalBlack, new Point(875, 438), new Size(375, 250),null),
                (WindowType.Minimizable, new Point(1250, 63), new Size(625, 250),null),
                (WindowType.TextDisplay, new Point(625, 63), new Size(375, 125), "Stage 15"),
            },
                GoalPosition = new Point(1750, 125),
                GoalInFront = true,
                PlayerStartPosition = new Point(375, 875),
                NoEntryZones = new List<(Point, Size)>
            {
                (new Point(1250, 313), new Size(625, 625)),
                (new Point(0, 906), new Size(125, 188)),
            },

                ToTitaleButtonPosition = new Point(106, 113),
                RetryButtonPosition = new Point(369, 113),
            });
            //クリア画面
            stages.Add(new StageData
            {
                Windows = new List<(WindowType type, Point location, Size size, string? text)>
            {
                (WindowType.NormalBlack, new Point(625, 750), new Size(625, 188),null),
                (WindowType.TextDisplay, new Point(250, 563), new Size(500, 250),""),
                (WindowType.TextDisplay, new Point(1125, 563), new Size(500, 250),""),
                (WindowType.NormalBlack, new Point(250, 375), new Size(500, 250),null),
                (WindowType.NormalBlack, new Point(1125, 375), new Size(500, 250),null),
                (WindowType.TextDisplay, new Point(625, 150), new Size(625, 313), "Thank you!!"),
                // 必要に応じて他のウィンドウを追加
            },
                PlayerStartPosition = new Point(913, 813),
                ToTitaleButtonPosition = new Point(850, 500),  //画面中央付近
                IsTitleStage = true
            });
        }
        public async Task StartStageAsync(int stageNumber)
        {
            if (stageNumber < 0 || stageNumber >= stages.Count) return;

            await ClearCurrentStage();
            currentStage = stageNumber;
            var stageData = stages[currentStage];

            // プレイヤーが最小化状態の場合は復元する
            var player = MainGame.GetPlayer();
            if (player != null && player.IsMinimized)
            {
                player.OnRestore();
            }

            // すべての要素を非表示で初期化
            await InitializeWindows(stageData);
            InitializeUIElements(stageData); // 同期呼び出しに変更
            InitializePlayer(stageData); // 同期呼び出しに変更
            FinalizeStageSetup();

            // すべての初期化完了後に一斉表示
            ShowAllElements();

            // ステージ遷移完了フラグをリセット
            (mainGame ?? MainGame.Current).ResetStageTransition();
        }
        private async Task ClearCurrentStage()
        {

            // 初回ステージ（ステージ0開始時）の場合はスキップ
            if (currentStage == 0 && (currentGoal == null && currentRetryButton == null && currentStartButton == null))
            {
                return;
            }

            // Program.mainFormのnullチェック
            if (Program.mainForm == null || Program.mainForm.IsDisposed)
            {
                return;
            }

            try
            {
                // UI要素を非表示（同期的に実行）
                await Task.Run(() =>
                {
                    if (Program.mainForm?.InvokeRequired == true)
                    {
                        Program.mainForm.Invoke(new Action(() =>
                        {
                            HideAllElements();
                        }));
                    }
                    else
                    {
                        HideAllElements();
                    }
                });


                // 短い遅延の後に実際の削除処理
                await Task.Delay(100);

                // UI要素を削除
                await Task.Run(() =>
                {
                    if (Program.mainForm?.InvokeRequired == true)
                    {
                        Program.mainForm.Invoke(new Action(() =>
                        {
                            CleanupAllElements();
                        }));
                    }
                    else
                    {
                        CleanupAllElements();
                    }
                });

            }
            catch (Exception ex)
            {
                // エラーが発生してもゲームを継続
            }
        }

        private void HideAllElements()
        {
            try
            {
                // 全要素を一斉に非表示（最小化状態を考慮）
                var player = MainGame.GetPlayer();
                if (player != null && !player.IsDisposed && !player.IsMinimized)
                {
                    player.Hide();
                }

                // 全ウィンドウを非表示
                var windows = (windowManager ?? WindowManager.Current).GetAllWindows();
                foreach (var window in windows)
                {
                    if (window != null && !window.IsDisposed)
                    {
                        window.Hide();
                    }
                }

                // UI要素を非表示（Disposed チェック付き）
                if (currentGoal != null && !currentGoal.IsDisposed)
                    currentGoal.Hide();

                if (currentRetryButton != null && !currentRetryButton.IsDisposed)
                    currentRetryButton.Hide();

                if (currentStartButton != null && !currentStartButton.IsDisposed)
                    currentStartButton.Hide();

                if (currentToTitaleButton != null && !currentToTitaleButton.IsDisposed)
                    currentToTitaleButton.Hide();

                if (currentExitButton != null && !currentExitButton.IsDisposed)
                    currentExitButton.Hide();
            }
            catch (Exception ex)
            {
            }
        }

        private void CleanupAllElements()
        {
            try
            {
                (windowManager ?? WindowManager.Current).ClearWindows();
                (noEntryZoneManager ?? NoEntryZoneManager.Current).ClearZones();

                // UI要素を削除してnullでリセット
                currentGoal?.Close();
                currentGoal = null;

                currentRetryButton?.Close();
                currentRetryButton = null;

                currentStartButton?.Close();
                currentStartButton = null;

                currentToTitaleButton?.Close();
                currentToTitaleButton = null;

                currentExitButton?.Close();
                currentExitButton = null;

                // 現在のステージウィンドウリストをクリア
                currentStageWindows.Clear();
            }
            catch (Exception ex)
            {
            }
        }
        private async Task InitializeWindows(StageData stageData)
        {
            var createdWindows = new List<GameWindow>();

            // ウィンドウの生成（非表示）
            foreach (var windowData in stageData.Windows)
            {
                var window = windowFactory?.CreateWindow(windowData.type, windowData.location, windowData.size, windowData.text, showImmediately: false);
                createdWindows.Add(window);
            }

            // 新しく作成されたウィンドウを保存
            currentStageWindows = createdWindows;

            // 初期化完了を待機
            await (windowManager ?? WindowManager.Current).InitializeWindowsAsync(createdWindows);

            // 不可侵領域の設定
            foreach (var zoneData in stageData.NoEntryZones)
            {
                (noEntryZoneManager ?? NoEntryZoneManager.Current).AddZone(zoneData.location, zoneData.size);
            }
        }
        private void InitializeUIElements(StageData stageData)
        {

            try
            {
                // UI要素を直接同期的に初期化（デッドロック回避）
                if (!stageData.IsTitleStage)
                {
                    currentGoal = windowFactory?.CreateGoal(stageData.GoalPosition, stageData.GoalInFront) ?? new Goal(stageData.GoalPosition, stageData.GoalInFront);
                    currentGoal.Hide(); // 初期化直後に非表示
                }

                if (stageData.RetryButtonPosition.HasValue)
                {
                    currentRetryButton = buttonFactory?.CreateRetryButton(stageData.RetryButtonPosition.Value) ?? new RetryButton(stageData.RetryButtonPosition.Value);
                    currentRetryButton.Hide(); // 初期化直後に非表示
                }

                if (stageData.StartButtonPosition.HasValue)
                {
                    currentStartButton = buttonFactory?.CreateStartButton(stageData.StartButtonPosition.Value) ?? new StartButton(stageData.StartButtonPosition.Value);
                    currentStartButton.Hide(); // 初期化直後に非表示
                }

                if (stageData.ToTitaleButtonPosition.HasValue)
                {
                    currentToTitaleButton = buttonFactory?.CreateToTitleButton(stageData.ToTitaleButtonPosition.Value) ?? new ToTitleButton(stageData.ToTitaleButtonPosition.Value);
                    currentToTitaleButton.Hide(); // 初期化直後に非表示
                }

                if (stageData.ExitButtonPosition.HasValue)
                {
                    currentExitButton = buttonFactory?.CreateExitButton(stageData.ExitButtonPosition.Value) ?? new ExitButton(stageData.ExitButtonPosition.Value);
                    currentExitButton.Hide(); // 初期化直後に非表示
                }

            }
            catch (Exception ex)
            {
            }
        }
        private void InitializePlayer(StageData stageData)
        {

            try
            {
                var player = MainGame.GetPlayer();

                if (player == null)
                {
                    (mainGame ?? MainGame.Current).InitializePlayer(stageData.PlayerStartPosition);
                    player = MainGame.GetPlayer();
                    // プレイヤーは初期状態で非表示のため、Hide()呼び出しは不要
                }
                else
                {
                    var gameSettings = GameSettings.Current;
                    player.ResetSize(gameSettings.Player.DefaultSize);
                    player.ResetPosition(stageData.PlayerStartPosition);

                    // リセット時に親ウィンドウを一度クリア
                    if (player.Parent != null)
                    {
                        player.Parent.RemoveChild(player);
                        player.SetParent(null);
                    }
                }

                // 生成時に親子関係をチェック
                if (player != null)
                {
                    var windowMgr = (windowManager ?? WindowManager.Current);
                    var intersectingWindows = windowMgr
                        .GetIntersectingWindows(player.Bounds)
                        .OrderByDescending(w => windowMgr.GetWindowZIndex(w));

                    foreach (var window in intersectingWindows)
                    {
                        if (window.AdjustedBounds.Contains(player.Bounds))
                        {
                            player.SetParent(window);
                            break;
                        }
                    }
                }

            }
            catch (Exception ex)
            {
            }
        }
        private void FinalizeStageSetup()
        {
            var player = MainGame.GetPlayer();
            if (player != null)
            {
                // プレイヤーの初期位置での親ウィンドウをチェック
                var windowMgr = (windowManager ?? WindowManager.Current);
                var intersectingWindows = windowMgr
                    .GetIntersectingWindows(player.Bounds)
                    .OrderByDescending(w => windowMgr.GetWindowZIndex(w));

                foreach (var window in intersectingWindows)
                {
                    if (window.AdjustedBounds.Contains(player.Bounds))
                    {
                        player.SetParent(window);
                        break;
                    }
                }
            }

            (windowManager ?? WindowManager.Current).UpdateWindowGroupZOrder();
        }

        private void ShowAllElements()
        {

            // Program.mainFormのnullチェック
            if (Program.mainForm == null || Program.mainForm.IsDisposed)
            {
                return;
            }

            try
            {
                ShowAllElementsSync();
            }
            catch (Exception ex)
            {
                // エラーが発生してもゲームを継続
            }
        }

        private void ShowAllElementsSync()
        {
            try
            {

                // すべての要素を一斉に表示（最小化状態を考慮）
                var player = MainGame.GetPlayer();
                if (player != null && !player.IsDisposed)
                {
                    if (player.IsMinimized)
                    {
                        // 最小化状態の場合は復元処理
                        player.OnRestore();
                    }
                    else
                    {
                        player.Show();
                    }
                }

                // 新しく作成されたウィンドウのみを一括表示（明滅防止、既存の最小化ウィンドウは除外）
                ShowWindowsBatch(currentStageWindows);

                // UI要素を表示（null・Disposed チェック付き）
                if (currentGoal != null && !currentGoal.IsDisposed)
                {
                    currentGoal.Show();
                }

                if (currentRetryButton != null && !currentRetryButton.IsDisposed)
                {
                    currentRetryButton.Show();
                }

                if (currentStartButton != null && !currentStartButton.IsDisposed)
                {
                    currentStartButton.Show();
                }

                if (currentToTitaleButton != null && !currentToTitaleButton.IsDisposed)
                {
                    currentToTitaleButton.Show();
                }

                if (currentExitButton != null && !currentExitButton.IsDisposed)
                {
                    currentExitButton.Show();
                }

                // Z-orderを更新
                WindowManager.Current.UpdateWindowGroupZOrder();
            }
            catch (Exception ex)
            {
            }
        }

        public async void RestartCurrentStage()
        {
            await StartStageAsync(currentStage);
        }
        public async void ToTitleStage()
        {
            await StartStageAsync(0);
        }
        public StageData GetStage(int stageNumber)
        {
            if (stageNumber < 0 || stageNumber >= stages.Count)
            {
                throw new ArgumentOutOfRangeException(nameof(stageNumber));
            }
            return stages[stageNumber];
        }
        public bool CheckGoal(PlayerForm player)
        {
            if (currentGoal == null || currentGoal.IsMinimized) return false;
            // 毎フレームのデバッグ情報は削除（パフォーマンス向上のため）

            // プレイヤーとゴールの重なりをチェック
            if (!player.Bounds.IntersectsWith(currentGoal.Bounds))
            {
                return false;
            }

            // Z-バッファのチェック
            var windowMgr = (windowManager ?? WindowManager.Current);
            var intersectingWindows = windowMgr.GetIntersectingWindows(
                Rectangle.Intersect(player.Bounds, currentGoal.Bounds)
            ).ToList();

            // デバッグ出力
            foreach (var window in intersectingWindows)
            {
            }

            if (true)
            {
                // ゴール達成
                return true;
            }
        }
        public void StartNextStage()
        {
            var nextStageNumber = currentStage + 1;
            if (nextStageNumber < stages.Count)
            {
                // プレイヤーを次のステージの開始位置に設定
                var nextStageData = stages[nextStageNumber];
                var player = MainGame.GetPlayer();
                if (player != null)
                {
                    player.ResetPosition(nextStageData.PlayerStartPosition);
                }

                _ = StartStageAsync(nextStageNumber);
            }
        }

        /// <summary>
        /// 複数のウィンドウを一括で表示し、明滅を防止する
        /// </summary>
        private void ShowWindowsBatch(IReadOnlyList<GameWindow> windows)
        {
            if (windows == null || windows.Count == 0) return;

            var validWindows = windows.Where(w => w != null && !w.IsDisposed && !w.IsMinimized && w.WindowState != FormWindowState.Minimized).ToList();
            if (validWindows.Count == 0) return;

            try
            {
                // DeferWindowPosを使用した一括表示
                IntPtr hdwp = WindowMessages.BeginDeferWindowPos(validWindows.Count);
                if (hdwp != IntPtr.Zero)
                {
                    foreach (var window in validWindows)
                    {
                        // 最小化ウィンドウの場合はSWP_SHOWWINDOWを除外
                        uint flags = WindowMessages.SWP_NOMOVE | WindowMessages.SWP_NOSIZE | WindowMessages.SWP_NOACTIVATE;
                        if (!window.IsMinimized && window.WindowState != FormWindowState.Minimized)
                        {
                            flags |= WindowMessages.SWP_SHOWWINDOW;
                        }

                        hdwp = WindowMessages.DeferWindowPos(
                            hdwp,
                            window.Handle,
                            IntPtr.Zero,
                            0, 0, 0, 0,
                            flags
                        );

                        if (hdwp == IntPtr.Zero) break;
                    }

                    if (hdwp != IntPtr.Zero)
                    {
                        WindowMessages.EndDeferWindowPos(hdwp);
                    }
                }

                // DeferWindowPosが失敗した場合のフォールバック
                if (hdwp == IntPtr.Zero)
                {
                    foreach (var window in validWindows)
                    {
                        if (!window.IsMinimized && window.WindowState != FormWindowState.Minimized)
                        {
                            window.Show();
                        }
                    }
                }
            }
            catch (Exception)
            {
                // エラー時のフォールバック
                foreach (var window in validWindows)
                {
                    try
                    {
                        if (!window.IsMinimized && window.WindowState != FormWindowState.Minimized)
                        {
                            window.Show();
                        }
                    }
                    catch
                    {
                        // 個別のウィンドウでエラーが発生しても継続
                    }
                }
            }
        }
    }

    public class StageData
    {
        public List<(WindowType type, Point location, Size size, string? text)> Windows { get; set; } = new();
        public Point GoalPosition { get; set; }
        public bool GoalInFront { get; set; }
        public Point PlayerStartPosition { get; set; }
        public List<(Point location, Size size)> NoEntryZones { get; set; } = new List<(Point, Size)>();
        public Point? RetryButtonPosition { get; set; }
        public Point? StartButtonPosition { get; set; }
        public Point? ExitButtonPosition { get; set; }
        public Point? ToTitaleButtonPosition { get; set; }
        public bool IsTitleStage { get; set; }
    }
}