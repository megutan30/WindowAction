using System.Drawing.Text;

public static class CustomFonts
{
    private static PrivateFontCollection privateFonts = new PrivateFontCollection();
    public static Font PressStart;

    static CustomFonts()
    {
        // フォントファイルをリソースから読み込む
        var fontPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "Resources", "Fonts", "prstart.ttf");
        privateFonts.AddFontFile(fontPath);
        PressStart = new Font(privateFonts.Families[0], 12f);
    }
}