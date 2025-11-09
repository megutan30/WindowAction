using System.Runtime.InteropServices;
using System.Windows.Forms;
using MultiWindowActionGame.Interfaces;

namespace MultiWindowActionGame.Services
{
    public class InputService : IInputService
    {
        [DllImport("user32.dll")]
        private static extern short GetAsyncKeyState(Keys vKey);

        public bool IsKeyDown(Keys key)
        {
            return (GetAsyncKeyState(key) & 0x8000) != 0;
        }
    }
}