using System.Collections.Generic;
using System.Drawing;
using System.Numerics;

namespace MultiWindowActionGame.Windows
{
    public interface IEffectTarget : IDrawable, IUpdatable
    {
        Rectangle Bounds { get; }
        GameWindow? Parent { get; }
        ICollection<IEffectTarget> Children { get; }
        void ApplyEffect(IWindowEffect effect);
        bool CanReceiveEffect(IWindowEffect effect);
        void AddChild(IEffectTarget child);
        void RemoveChild(IEffectTarget child);
        void UpdateTargetSize(Size newSize);
        void UpdateTargetPosition(Point newPositon);
        void OnMinimize();
        void OnRestore();
        Size GetOriginalSize();
        bool IsMinimized { get; }

        // リサイズ制約メソッド（新規追加）
        Size GetMinimumSize();
        Size GetMaximumSize();
        bool ShouldInheritParentConstraints { get; }
    }
    public interface IWindowEffect
    {
        EffectType Type { get; }
        bool IsActive { get; }
        void Apply(IEffectTarget target);
    }
    public enum EffectType
    {
        Movement,
        Resize,
        Delete,
        Minimize
    }
    public abstract class BaseWindowEffect : IWindowEffect
    {
        public bool IsActive { get; protected set; }
        public abstract EffectType Type { get; }

        public virtual void Apply(IEffectTarget target)
        {
            if (!IsActive || !target.CanReceiveEffect(this)) return;

            // 対象自身にエフェクトを適用
            ApplyToTarget(target);

            // 直接の子にのみ適用
            foreach (var child in GetDirectChildren(target))
            {
                Apply(child);
            }
        }

        protected abstract void ApplyToTarget(IEffectTarget target);

        protected IEnumerable<IEffectTarget> GetDirectChildren(IEffectTarget parent)
        {
            // ToList()でスナップショットを取得して、列挙中のコレクション変更エラーを防ぐ
            return parent.Children.Where(child =>
            {
                if (child is GameWindow childWindow)
                {
                    return childWindow.Parent == parent;
                }
                return true;
            }).ToList();
        }
    }
    public class MovementEffect : BaseWindowEffect
    {
        private Vector2 movement;
        public Vector2 CurrentMovement => movement;
        public override EffectType Type => EffectType.Movement;

        public void UpdateMovement(Vector2 newMovement)
        {
            movement = newMovement;
            IsActive = movement != Vector2.Zero;
        }
        protected override void ApplyToTarget(IEffectTarget target)
        {
            var newPosition = target is GameWindow window
                ? new Point(
                    window.Location.X + (int)movement.X,
                    window.Location.Y + (int)movement.Y)
                : new Point(
                    target.Bounds.X + (int)movement.X,
                    target.Bounds.Y + (int)movement.Y);

            target.UpdateTargetPosition(newPosition);
        }
    }
    public class ResizeEffect : BaseWindowEffect
    {
        private readonly Dictionary<IEffectTarget, SizeF> targetScales = new();
        private readonly Dictionary<IEffectTarget, Size> referenceSize = new();
        public override EffectType Type => EffectType.Resize;
        public void UpdateScale(IEffectTarget target, SizeF newScale, Size baseSize)
        {
            referenceSize[target] = baseSize;
            targetScales[target] = newScale;
            IsActive = true;
        }

        protected override void ApplyToTarget(IEffectTarget target)
        {
            if (!referenceSize.ContainsKey(target)) return;

            var baseSize = referenceSize[target];
            var scale = targetScales.GetValueOrDefault(target, new SizeF(1.0f, 1.0f));

            Size newSize = new Size(
                (int)(baseSize.Width * scale.Width),
                (int)(baseSize.Height * scale.Height)
            );

            target.UpdateTargetSize(newSize);
        }
        public SizeF GetCurrentScale(IEffectTarget target)
        {
            return targetScales.GetValueOrDefault(target, new SizeF(1.0f, 1.0f));
        }
        public void ResetAll()
        {
            targetScales.Clear();
            referenceSize.Clear();
            IsActive = false;
        }
        public void Reset(IEffectTarget target)
        {
            if (target == null) return;

            targetScales.Remove(target);
            referenceSize.Remove(target);
            IsActive = targetScales.Count > 0;
        }
    }
    public class MinimizeEffect : BaseWindowEffect
    {
        public override EffectType Type => EffectType.Minimize;
        public void Activate()
        {
            IsActive = true;
        }
        protected override void ApplyToTarget(IEffectTarget target)
        {
            if (!IsActive) return;
            target.OnMinimize();
            IsActive = false;
        }
    }
}