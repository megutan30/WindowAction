using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using MultiWindowActionGame.Windows;
using MultiWindowActionGame.Extensions;

namespace MultiWindowActionGame.Effects
{
    public abstract class BaseEffectTarget : Form, IEffectTarget
    {
        protected Rectangle bounds;
        public new virtual Rectangle Bounds => bounds;
        private GameWindow? parent;
        public new GameWindow? Parent
        {
            get => parent;
            protected set => parent = value;
        }
        public ICollection<IEffectTarget> Children { get; } = new HashSet<IEffectTarget>();
        public bool IsMinimized { get; protected set; }

        private volatile bool isUpdatingParent = false;

        public virtual void SetParent(GameWindow? newParent)
        {
            // 循環的な親変更を防ぐ
            if (isUpdatingParent)
            {
                System.Diagnostics.Debug.WriteLine("BaseEffectTarget: 循環的な親変更を検出、処理をスキップ");
                return;
            }

            try
            {
                isUpdatingParent = true;

                if (Parent != null)
                {
                    Parent.RemoveChild(this);
                }
                Parent = newParent;
                Parent?.AddChild(this);

                // 親が変更されたら再描画を要求
                if (this is Form form && !form.IsDisposed)
                {
                    form.Invalidate();
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"BaseEffectTarget: 親変更エラー - {ex.Message}");
            }
            finally
            {
                isUpdatingParent = false;
            }
        }

        public virtual void AddChild(IEffectTarget child)
        {
            Children.Add(child);
            if (child is GameWindow window)
            {
                window.SetParent(Parent);
            }
        }

        public virtual void RemoveChild(IEffectTarget child)
        {
            Children.Remove(child);
        }

        public virtual bool CanReceiveEffect(IWindowEffect effect)
        {
            return Parent != null;
        }

        public virtual void ApplyEffect(IWindowEffect effect)
        {
            if (!CanReceiveEffect(effect)) return;

            if (effect is MovementEffect moveEffect)
            {
                var newPos = new Point(
                    bounds.X + (int)moveEffect.CurrentMovement.X,
                    bounds.Y + (int)moveEffect.CurrentMovement.Y
                );
                UpdateTargetPosition(newPos);
            }
            else if (effect is ResizeEffect resizeEffect)
            {
                var scale = resizeEffect.GetCurrentScale(this);
                var newSize = new Size(
                    (int)(bounds.Width * scale.Width),
                    (int)(bounds.Height * scale.Height)
                );
                UpdateTargetSize(newSize);
            }
        }
        protected override void WndProc(ref Message m)
        {
            var result = WindowMessageHandler.HandleWindowMessage(this, m);
            if (!result.Handled)
            {
                base.WndProc(ref m);
            }
            else
            {
                m.Result = result.Result;
            }
        }
        public abstract void UpdateTargetPosition(Point newPosition);
        public abstract void UpdateTargetSize(Size newSize);
        public abstract void OnMinimize();
        public abstract void OnRestore();
        public abstract void Draw(Graphics g);
        public abstract Task UpdateAsync(float deltaTime);
        public abstract Size GetOriginalSize();

        // リサイズ制約のデフォルト実装
        public virtual Size GetMinimumSize()
        {
            // デフォルト: FormのMinimumSizeを使用
            return this.MinimumSize;
        }

        public virtual Size GetMaximumSize()
        {
            // デフォルト: 事実上無制限（int型の最大値）
            return new Size(int.MaxValue, int.MaxValue);
        }

        public virtual bool ShouldInheritParentConstraints
        {
            get
            {
                // デフォルト: 親の制約を継承しない（各要素が独立）
                return false;
            }
        }
    }
}
