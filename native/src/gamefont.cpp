/* Rendering/CustomFonts.cs から移植: PrivateFontCollectionの代わりに
   AddFontMemResourceExを使い、埋め込みリソースからprstart.ttfを読み込む。 */
#include "gamefont.h"

#define MAX_CACHED_SIZES 16

static char g_fontFamily[LF_FACESIZE] = "MS Gothic"; /* 埋め込みが失敗した場合のシステムフォールバック */
static struct
{
    int pointSize;
    int bold;
    HFONT font;
} g_cache[MAX_CACHED_SIZES];
static int g_cacheCount = 0;

void GameFont_Init(HINSTANCE hInstance)
{
    HRSRC res = FindResourceA(hInstance, "PRSTART_FONT", RT_RCDATA);
    if (!res)
        return;

    HGLOBAL mem = LoadResource(hInstance, res);
    if (!mem)
        return;

    void *data = LockResource(mem);
    DWORD size = SizeofResource(hInstance, res);
    if (!data || size == 0)
        return;

    DWORD numFonts = 0;
    HANDLE handle = AddFontMemResourceEx(data, size, NULL, &numFonts);
    if (!handle || numFonts == 0)
        return;

    /* prstart.ttfの内部フォントファミリー名（似た見た目の公開フォントが
       名乗る "Press Start 2P" ではなく、フォントメタデータから確認したもの）。
       下記GameFont_Get内のCreateFontAは、AddFontMemResourceExがこのプロセス
       専用に登録したプライベートテーブルからこれを解決する。 */
    lstrcpynA(g_fontFamily, "Press Start", LF_FACESIZE);
}

static HFONT GetOrCreate(int pointSize, int bold)
{
    for (int i = 0; i < g_cacheCount; i++)
        if (g_cache[i].pointSize == pointSize && g_cache[i].bold == bold)
            return g_cache[i].font;

    HDC dc = GetDC(NULL);
    int height = -MulDiv(pointSize, GetDeviceCaps(dc, LOGPIXELSY), 72);
    ReleaseDC(NULL, dc);

    HFONT font = CreateFontA(height, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, g_fontFamily);

    if (g_cacheCount < MAX_CACHED_SIZES)
    {
        g_cache[g_cacheCount].pointSize = pointSize;
        g_cache[g_cacheCount].bold = bold;
        g_cache[g_cacheCount].font = font;
        g_cacheCount++;
    }
    return font;
}

HFONT GameFont_Get(int pointSize) { return GetOrCreate(pointSize, 0); }
HFONT GameFont_GetBold(int pointSize) { return GetOrCreate(pointSize, 1); }
