#ifndef NOENTRY_H
#define NOENTRY_H

#include <windows.h>
#include <functional>

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

/* index番目のゾーン1つだけを取り除く（可視マーカーウィンドウの破棄+配列の
   詰め直し）。NoEntry_ResetZonesとは異なり全消去ではなく、ステージエディター
   でのDeleteキーによる個別削除用（Editor_HandleDeleteInput参照）。範囲外の
   indexを渡しても安全（何もしない）。 */
void NoEntry_RemoveZone(int index);

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

/* NoEntry_IsRectVisibleFromWindowの詳細版: 可視かどうかのboolだけでなく、
   実際に見えている部分の外接矩形を`outBounds`(非NULL時)に書き込む。呼び出し
   側がその後の衝突応答（移動を止める位置の計算など）に、隠れている部分も
   含む`rect`全体ではなく実際に見えている範囲だけを使うために必要 --
   NoEntryBoundaryCollider.CheckCollisionが返すcollisionRectと同じ役割。
   これを使わずに元の`rect`（またはNoEntry_GetBoundaryRectsの生の境界）を
   そのまま衝突応答に使うと、不可侵ウィンドウの境界の一部が別のウィンドウに
   隠れている場合に、見えている/見えていない境目でプレイヤーの挙動が
   おかしくなる（実際に報告された不具合）。 */
int NoEntry_GetVisiblePortion(int windowIndex, RECT rect, RECT *outBounds);

/* C++移行フェーズ5: NoEntry_GetVisiblePortionと、collision.cppの
   IsNormalWindowVisibleFromExcludedは、どちらも「windowIndexより前面に
   あり、かつ述語isOccluderを満たすウィンドウの矩形を順にrectから
   RGN_DIFFで差し引いていき、何か残ればそのバウンディングボックスを返す」
   というZ-order+Region方式の同一アルゴリズムだったが、遮蔽対象の判定条件
   だけが異なるために別々に実装されていた（Cの時代はコールバック/述語を
   自然に渡す手段が無く統合を見送っていた）。共通コアをここに切り出し、
   isOccluderで呼び出し側ごとに異なる遮蔽条件を渡せるようにする。
   windowIndex自身・非表示(!hwnd)・windowIndex以下のZ-orderは、
   どちらの呼び出し元でも共通の除外条件のため、ここで固定で処理する。 */
int NoEntry_ComputeVisibleRegion(int windowIndex, RECT rect,
                                  const std::function<bool(int)> &isOccluder,
                                  RECT *outBounds);

/* NoEntry_GetBoundaryRectsの可視性考慮版。より前面の（NoEntryに限らない）
   ウィンドウに完全に隠されている境界帯は除外し、部分的に隠れているものは
   可視部分の外接矩形を返す。戻り値は書き込んだ矩形の個数(0～4)。
   GatherObstacles(collision.c)がMovable/Resizableウィンドウの移動・
   リサイズをNoEntyウィンドウの境界と照合する際は、NoEntry_GetBoundaryRects
   ではなく必ずこちらを使うこと -- そうしないと、実際には手前の別ウィンドウに
   隠れている境界の断片にまで移動・リサイズが弾かれてしまう。 */
int NoEntry_GetVisibleBoundaryRects(int windowIndex, RECT out[4]);

#endif
