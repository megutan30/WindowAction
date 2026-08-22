#ifndef NOENTRY_H
#define NOENTRY_H

#include <windows.h>

/* "WA_NoEntryZone"ウィンドウクラスを登録する。NoEntry_AddZoneを呼ぶ前に
   起動時に一度だけ呼ぶこと。 */
void NoEntry_RegisterWindowClass(HINSTANCE hInstance);

/* 衝突判定用の静的NoEntryゾーンを追加し、その可視マーカーも生成する:
   矩形全体を覆う、透明・クリックスルー・常に最前面のポップアップウィンドウで、
   赤黒の縞模様をスクロールさせて塗りつぶす -- NoEntryZone.cs
   （WT_*_NOENTRY種別のウィンドウに対してOutlineRendererが描く薄い枠ではなく、
   ゾーンごとの専用Form）と一致。 */
void NoEntry_AddZone(HINSTANCE hInstance, int x, int y, int w, int h);

void NoEntry_UpdateAnimation(float dt);

/* すべてのゾーンの可視マーカーウィンドウを破棄し、ゾーンリストをクリアする。
   ステージロードのたびにResetWindowRegistryから呼び出す。 */
void NoEntry_ResetZones(void);

/* NoEntryフラグ付きウィンドウの4方向の境界帯矩形（上下左右、幅5px）を
   out[4]に書き込む。ウィンドウがNoEntryでなければ0を返す。 */
int NoEntry_GetBoundaryRects(int windowIndex, RECT out[4]);

/* boundsが、いずれかの静的NoEntryZoneまたはNoEntryウィンドウの4辺境界帯と
   交差していればtrue。NoEntryZoneManager.IntersectsWithAnyZoneと一致。 */
int NoEntry_IntersectsAny(RECT bounds);

/* `rect`（NoEntryウィンドウ`windowIndex`の境界帯のいずれかとチェック領域との
   交差部分であることが前提）のうち、より前面にある非最小化のNoEntry
   ウィンドウに覆われていない部分が残っていればtrue。
   NoEntryBoundaryCollider.CheckCollisionのZ-order/Region可視性判定と一致。
   NoEntryウィンドウの境界に対するプレイヤーの衝突判定（移動ブロック、
   接地判定）は、NoEntry_GetBoundaryRects単体のAABB重なりではなく、
   必ずこの判定を経由すること -- そうしないと、実際にはより前面のウィンドウに
   隠れている境界の断片にプレイヤーが衝突してしまう。 */
int NoEntry_IsRectVisibleFromWindow(int windowIndex, RECT rect);

#endif
