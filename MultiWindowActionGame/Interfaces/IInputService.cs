using System.Windows.Forms;

namespace MultiWindowActionGame.Interfaces
{
    public interface IInputService
    {
        bool IsKeyDown(Keys key);
    }
}