using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using MultiWindowActionGame.Windows;
using MultiWindowActionGame.Interfaces;

namespace MultiWindowActionGame.Effects
{
    public class WindowEffectManager : IWindowEffectManager
    {
        // インスタンス統一化のためのstatic参照
        private static WindowEffectManager? _current;
        public static WindowEffectManager Current
        {
            get => _current ?? throw new InvalidOperationException("WindowEffectManager is not initialized");
            internal set => _current = value;
        }

        private readonly List<IWindowEffect> activeEffects = new();

        public WindowEffectManager() { }

        public void AddEffect(IWindowEffect effect)
        {
            if (!activeEffects.Contains(effect))
            {
                activeEffects.Add(effect);
            }
        }

        public void RemoveEffect(IWindowEffect effect)
        {
            activeEffects.Remove(effect);
        }

        public void ApplyEffects(IEffectTarget target)
        {
            foreach (var effect in activeEffects.Where(e => e.IsActive))
            {
                effect.Apply(target);
            }
        }

        public void ClearEffects()
        {
            activeEffects.Clear();
        }
    }
}
