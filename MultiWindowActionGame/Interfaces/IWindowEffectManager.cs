using MultiWindowActionGame.Windows;

namespace MultiWindowActionGame.Interfaces
{
    public interface IWindowEffectManager
    {
        void AddEffect(IWindowEffect effect);
        void RemoveEffect(IWindowEffect effect);
        void ApplyEffects(IEffectTarget target);
        void ClearEffects();
    }
}