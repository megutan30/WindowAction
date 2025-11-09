using System;
using System.Collections.Generic;
using System.Drawing.Drawing2D;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MultiWindowActionGame.Extensions
{
    public static class RegionExtensions
    {
        public static GraphicsPath GetRegionPath(this Region region)
        {
            var path = new GraphicsPath();
            var rects = region.GetRegionScans(new Matrix());
            foreach (var rect in rects)
            {
                path.AddRectangle(rect);
            }
            return path;
        }
    }
}
