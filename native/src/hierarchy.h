#ifndef HIERARCHY_H
#define HIERARCHY_H

#include <windows.h>

void Hierarchy_Attach(int parentIdx, int childIdx);
void Hierarchy_Detach(int childIdx);

/* 移動またはリサイズが起きたウィンドウの親子関係を再検証する。
   WindowHierarchyManager.CheckPotentialParentWindowを踏襲。 */
void Hierarchy_CheckAndUpdate(int movedIndex);

void Hierarchy_PropagateMove(int index, int dx, int dy);

/* `rootIndex`の全子孫の現在サイズを、スケール計算の基準値
   （GameWindowData.origSize）として記録する。リサイズ操作の開始時に一度だけ
   呼び出す。ResizableWindowStrategy.RecordOriginalSizesRecursiveを踏襲。 */
void Hierarchy_RecordOriginalSizes(int rootIndex);

/* `rootIndex`の全子孫（Goal/ボタンを除く）を、記録済みのorigBoundsAtResizeStart
   （ジェスチャー開始時点の絶対矩形）から`oldRect`→`newRect`への変換に合わせて
   相対位置・相対サイズを保ったまま追従させる。通常のResizable
   （UpdateResizable、位置は常にSWP_NOMOVEで固定なのでoldRect/newRectは
   左上が同じ）と制限なしリサイズ+反転ウィンドウ（UpdateUnconstrained、
   アンカー基準の反転で左上そのものが動く/反転しうる）の両方から使う共通
   実装。`minSize`/`maxSize`は子の拡大縮小に課す下限/上限で、呼び出し元が
   自分の種別に応じたものを渡す（通常のResizableはMIN_WINDOW_SIZE、制限
   なしリサイズはより小さいUNCONSTRAINED_MIN_ABS_SIZE）。Player/Goal/ボタン
   は「サイズのみ変更、位置固定」のまま扱う（内部でApplyScaleToSpecialChildren
   相当を呼ぶ）。 */
void Hierarchy_ApplyRelativeTransform(int rootIndex, RECT oldRect, RECT newRect, int minSize, int maxSize);

/* `rootIndex`が今まさに反転イベントを起こした瞬間に一度だけ呼ぶ。その時点の
   全子孫（Hierarchy_ApplyRelativeTransformと違い、Goal/ボタンも含め無条件）の
   GameWindowData.inheritedFlipX/YをXORで反転させる。これは「今その祖先の
   内部にいるか」をその場で判定するライブ計算ではなく、一度反転した見た目が
   親から切り離された後も、再度どこかの祖先が反転するまで元に戻らないための
   永続フラグの更新。toggleX/toggleYはこのイベントで反転した軸だけtrueにする
   （UpdateUnconstrainedがこのフレームでの新旧反転状態を比較して呼び出す）。 */
void Hierarchy_ToggleInheritedFlip(int rootIndex, int toggleX, int toggleY);

int Hierarchy_IsDescendantOf(int candidateIndex, int ancestorIndex);

/* `startIdx`から始まる祖先チェーン上のどこかに`targetIdx`が存在すればtrue
   （startIdx自身も対象に含む）。移動したウィンドウの下のどこかに親を持つ
   エンティティが、それに追従して移動すべきかを判定するために使う --
   単純な平行移動ではレベルごとのスケール伝播は不要で、「このチェーン内に
   この祖先が存在するか」だけを見ればよい。 */
int Hierarchy_ChainContains(int startIdx, int targetIdx);

/* `index`とそのサブツリー全体（子、孫、…）を再帰的に最小化する。
   GameWindow.OnMinimizeと一致させる: 各子孫は独立して最小化され、
   かつ親から切り離される -- 後で祖先を復元しても自動的には戻らない。
   サブツリー内のどこかにプレイヤーが属していた場合は、プレイヤーも
   凍結する（Player_OnMinimize参照）。 */
void Hierarchy_MinimizeSubtree(int index);

/* `index`単体のみを復元する（GameWindow.OnRestoreと一致させ、何にも
   カスケードしない）。可視状態に戻った今、親子関係の再検出を一度だけ
   実行する。 */
void Hierarchy_RestoreWindow(int index);

#endif
