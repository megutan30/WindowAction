#ifndef ZORDER_H
#define ZORDER_H

#include <windows.h>

void ZOrder_Reset(void);
void ZOrder_Register(HWND hwnd);
void ZOrder_Unregister(HWND hwnd);

/* hwndとその登録済み子孫を最前面（内部リストの末尾）に移動し、
   実際のOS Z-orderをそれに合わせて再同期する。現在のC#
   WindowZOrderManager.BringWindowToFront の挙動を厳密に反映している。 */
void ZOrder_BringToFront(HWND hwnd);

int ZOrder_GetIndex(HWND hwnd); /* 0 = 最背面 */
int ZOrder_IsInFront(HWND a, HWND b);

/* プレイヤーのクリックで何が最前面に来ようとも、Goal/Buttonsとプレイヤーを
   すべての "Window" 種別のエントリより上に再度固定する。実際のゲームが持つ
   固定のZOrderPriority帯（Window < Button = Goal < Player）を反映しており、
   OSのZ-orderは常にこれに再同期される。毎フレーム1回呼び出すこと。 */
void ZOrder_ReassertOverlayFront(HWND playerHwnd);

#endif
