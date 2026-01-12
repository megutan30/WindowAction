using System.Drawing.Text;
using System.Reflection;
using System.Runtime.InteropServices;

public static class CustomFonts
{
    private static PrivateFontCollection privateFonts = new PrivateFontCollection();
    public static Font PressStart;

    static CustomFonts()
    {
        try
        {
            // 埋め込みリソースからフォントを読み込む
            var assembly = Assembly.GetExecutingAssembly();
            var resourceName = "MultiWindowActionGame.Resources.Fonts.prstart.ttf";

            using (var stream = assembly.GetManifestResourceStream(resourceName))
            {
                if (stream != null)
                {
                    // ストリームからメモリにフォントを読み込む
                    byte[] fontData = new byte[stream.Length];
                    stream.Read(fontData, 0, (int)stream.Length);

                    // メモリ上のフォントデータをPrivateFontCollectionに追加
                    IntPtr fontPtr = Marshal.AllocCoTaskMem(fontData.Length);
                    Marshal.Copy(fontData, 0, fontPtr, fontData.Length);
                    privateFonts.AddMemoryFont(fontPtr, fontData.Length);
                    Marshal.FreeCoTaskMem(fontPtr);

                    PressStart = new Font(privateFonts.Families[0], 12f);
                }
                else
                {
                    // フォールバック: 物理ファイルから読み込み（開発環境用）
                    var fontPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "Resources", "Fonts", "prstart.ttf");
                    if (File.Exists(fontPath))
                    {
                        privateFonts.AddFontFile(fontPath);
                        PressStart = new Font(privateFonts.Families[0], 12f);
                    }
                    else
                    {
                        // 最終フォールバック: システムフォント
                        PressStart = new Font(FontFamily.GenericMonospace, 12f);
                    }
                }
            }
        }
        catch
        {
            // エラー時はシステムフォントを使用
            PressStart = new Font(FontFamily.GenericMonospace, 12f);
        }
    }
}