using System.Drawing;

namespace MultiWindowActionGame
{
    public interface IDrawable
    {
        void Draw(Graphics g);
    }

    public interface IUpdatable
    {
        Task UpdateAsync(float deltaTime);
    }
}