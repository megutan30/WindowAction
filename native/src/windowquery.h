#ifndef WINDOWQUERY_H
#define WINDOWQUERY_H

#include <windows.h>

/* `bounds`を完全に内包するウィンドウのうち、最も前面にあるものを返す。
   WindowManager.GetWindowFullyContainingと一致。該当なしなら-1を返す。 */
int WindowQuery_GetFullyContaining(RECT bounds);

/* 5点（四隅+中心、下側/「足元」の2点を重視）による、指定点における
   最前面ウィンドウの探索。より良い（階層が浅く、より前面の）候補が
   見つからない限り現在の親を維持するようバイアスをかける。
   WindowCollisionDetector.GetTopWindowAtと一致。該当なしなら-1を返す。 */
int WindowQuery_GetTopWindowAt(RECT bounds, int currentParentIdx);

/* スクリーン座標系のクライアント矩形（タイトルバーと枠を除く）。 */
void WindowQuery_GetClientBounds(int index, RECT *out);

int WindowQuery_FullyContains(RECT outer, RECT inner);
int WindowQuery_RectsOverlap(RECT a, RECT b);

/* WindowManager.CalculateMovableRegionと一致: 現在のウィンドウ自身の
   クライアント領域に、それと重なる、またはスナップ距離内にある
   トップレベル（親なし）ウィンドウのクライアント領域（およびその
   子孫全体）をUNIONしたもの。これは接続されたウィンドウを「追加」する
   だけで、子ウィンドウを「差し引く」ことはしない点に注意 --
   実際のゲームでは移動可能領域から子ウィンドウを切り抜くことはない。
   out[]に最大maxOut個の矩形を書き込み、書き込んだ数を返す。 */
int WindowQuery_BuildMovableRegion(int parentIdx, RECT *out, int maxOut);
int WindowQuery_PointInAnyRect(POINT p, const RECT *rects, int count);

#endif
