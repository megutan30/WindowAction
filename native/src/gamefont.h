#ifndef GAMEFONT_H
#define GAMEFONT_H

#include <windows.h>

/* AddFontMemResourceExを使い、埋め込みRCDATAリソースからprstart.ttfを読み込む。
   Rendering/CustomFonts.cs を反映（失敗時はシステムフォントにフォールバック）。 */
void GameFont_Init(HINSTANCE hInstance);

/* 指定したポイントサイズで、読み込んだカスタム書体のHFONTを返す
   （サイズごとに生成してキャッシュする）。NULLを返すことはない。 */
HFONT GameFont_Get(int pointSize);

/* 太字版。GameButton.DrawButtonContentはボタンラベル（Start/Retry/
   ToTitle/Exit）に常にFontStyle.Boldを使う一方、TextDisplayWindowStrategyの
   ラベルは通常の太さのまま -- この2つは同じキャッシュを共有してはならない。 */
HFONT GameFont_GetBold(int pointSize);

#endif
