using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MultiWindowActionGame.Extensions
{
    public static class RectangleExtensions
    {
        public static Point Center(this Rectangle rect)
        {
            return new Point(
                rect.X + rect.Width / 2,
                rect.Y + rect.Height / 2
            );
        }
    }
}
