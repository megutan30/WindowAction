using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using MultiWindowActionGame.Utilities;
using MultiWindowActionGame.Core;

namespace MultiWindowActionGame.UI
{
    public class SettingsForm : Form
    {
        private readonly GameSettings settings;
        private readonly TabControl tabControl;
        private readonly Button applyButton;
        private readonly Button saveButton;
        private readonly Dictionary<string, NumericUpDown> settingControls;
        private readonly MainGame mainGame;
        private readonly Label statusLabel;
        private bool shouldResumeOnClose = true;
        public SettingsForm() : this(null, null)
        {
        }
        
        public SettingsForm(MainGame? mainGame, GameSettings? gameSettings)
        {
            this.mainGame = mainGame ?? MainGame.Current;
            settings = gameSettings ?? GameSettings.Current;
            settingControls = new Dictionary<string, NumericUpDown>();

            this.Text = "Game Settings";
            this.Size = new Size(400, 500);
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.StartPosition = FormStartPosition.CenterScreen;
            this.TopMost = true;

            tabControl = new TabControl
            {
                Dock = DockStyle.Fill,
                Padding = new Point(10, 10)
            };

            // タブページの作成
            CreatePlayerSettingsTab();
            CreateWindowSettingsTab();
            CreateGameplaySettingsTab();

            var buttonPanel = new Panel
            {
                Dock = DockStyle.Bottom,
                Height = 50
            };

            applyButton = new Button
            {
                Text = "Apply",
                Location = new Point(200, 10),
                Size = new Size(80, 30)
            };
            applyButton.Click += ApplyButton_Click;

            saveButton = new Button
            {
                Text = "Save",
                Location = new Point(290, 10),
                Size = new Size(80, 30)
            };
            saveButton.Click += SaveButton_Click;

            buttonPanel.Controls.AddRange(new Control[] { applyButton, saveButton });

            this.Controls.AddRange(new Control[] { tabControl, buttonPanel });
            // ステータスラベルの追加
            statusLabel = new Label
            {
                AutoSize = false,
                TextAlign = ContentAlignment.MiddleLeft,
                Height = 30,
                Dock = DockStyle.Bottom,
                BackColor = Color.LightGray
            };

            this.Controls.Add(statusLabel);
            // フォームを閉じる時の動作を設定
            this.FormClosing += SettingsForm_FormClosing;
        }

        private void CreatePlayerSettingsTab()
        {
            var page = new TabPage("Player");
            var layout = new TableLayoutPanel
            {
                Dock = DockStyle.Fill,
                Padding = new Padding(10),
                ColumnCount = 2,
                RowCount = 5
            };

            AddSettingControl(layout, 0, "Movement Speed", settings.Player.MovementSpeed, 0, 1000);
            AddSettingControl(layout, 1, "Gravity", settings.Player.Gravity, 0, 2000);
            AddSettingControl(layout, 2, "Jump Force", settings.Player.JumpForce, 0, 1000);
            AddSettingControl(layout, 3, "Default Width", settings.Player.DefaultSize.Width, 10, 100);
            AddSettingControl(layout, 4, "Default Height", settings.Player.DefaultSize.Height, 10, 100);

            page.Controls.Add(layout);
            tabControl.TabPages.Add(page);
        }

        private void CreateWindowSettingsTab()
        {
            var page = new TabPage("Window");
            var layout = new TableLayoutPanel
            {
                Dock = DockStyle.Fill,
                Padding = new Padding(10),
                ColumnCount = 2,
                RowCount = 4
            };

            AddSettingControl(layout, 0, "Min Width", settings.Window.MinimumSize.Width, 50, 500);
            AddSettingControl(layout, 1, "Min Height", settings.Window.MinimumSize.Height, 50, 500);

            page.Controls.Add(layout);
            tabControl.TabPages.Add(page);
        }

        private void CreateGameplaySettingsTab()
        {
            var page = new TabPage("Gameplay");
            var layout = new TableLayoutPanel
            {
                Dock = DockStyle.Fill,
                Padding = new Padding(10),
                ColumnCount = 2,
                RowCount = 3
            };

            AddSettingControl(layout, 0, "Target FPS", settings.Gameplay.TargetFPS, 30, 144);
            AddSettingControl(layout, 2, "Snap Distance", settings.Gameplay.WindowSnapDistance, 0, 50);

            page.Controls.Add(layout);
            tabControl.TabPages.Add(page);
        }

        private void AddSettingControl(TableLayoutPanel layout, int row, string label, float value, decimal min, decimal max)
        {
            layout.Controls.Add(new Label { Text = label, Dock = DockStyle.Fill }, 0, row);

            var numericUpDown = new NumericUpDown
            {
                Minimum = min,
                Maximum = max,
                Value = (decimal)value,
                DecimalPlaces = 1,
                Dock = DockStyle.Fill
            };

            layout.Controls.Add(numericUpDown, 1, row);
            settingControls.Add(label, numericUpDown);
        }

        private void ApplyButton_Click(object? sender, EventArgs e)
        {
            UpdateSettings();
            ShowStatus("Settings applied successfully!", Color.Green);
        }

        private void SaveButton_Click(object? sender, EventArgs e)
        {
            UpdateSettings();
            settings.SaveSettings();
            shouldResumeOnClose = true;
            this.Close();
        }

        private void ShowStatus(string message, Color color)
        {
            statusLabel.Text = message;
            statusLabel.ForeColor = color;
        }
        private void UpdateSettings()
        {
            // プレイヤー設定
            settings.Player.MovementSpeed = (float)settingControls["Movement Speed"].Value;
            settings.Player.Gravity = (float)settingControls["Gravity"].Value;
            settings.Player.JumpForce = (float)settingControls["Jump Force"].Value;
            settings.Player.DefaultSize = new Size(
                (int)settingControls["Default Width"].Value,
                (int)settingControls["Default Height"].Value
            );

            // ウィンドウ設定
            settings.Window.MinimumSize = new Size(
                (int)settingControls["Min Width"].Value,
                (int)settingControls["Min Height"].Value
            );

            // ゲームプレイ設定
            settings.Gameplay.TargetFPS = (int)settingControls["Target FPS"].Value;
            settings.Gameplay.WindowSnapDistance = (float)settingControls["Snap Distance"].Value;
        }
        private void SettingsForm_FormClosing(object? sender, FormClosingEventArgs e)
        {
            UpdateSettings();
            settings.SaveSettings();
            mainGame.ResumeGame();
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            base.OnFormClosing(e);
            if (shouldResumeOnClose)
            {
                // フォームが完全に閉じた後でゲームを再開するようにする
                BeginInvoke(new Action(() => mainGame.ResumeGame()));
            }
        }
    }
}
