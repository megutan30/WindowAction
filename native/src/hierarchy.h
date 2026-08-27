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

/* `rootIndex`の全子孫を、記録済みのorigSizeから(scaleX, scaleY)倍にリスケール
   する。位置(X,Y)は変更しない -- 実ゲームのResizeEffectはUpdateTargetSizeのみ
   呼び出し、UpdateTargetPositionは呼び出さないため、子要素は絶対スクリーン位置
   を保ったまま拡大縮小のみ行う。 */
void Hierarchy_ApplyScale(int rootIndex, float scaleX, float scaleY);

/* Hierarchy_ApplyScaleの「相対位置も保つ」版。WT_UNCONSTRAINED(制限なし
   リサイズ+反転ウィンドウ)専用: アンカー基準の反転により親の可視矩形の
   左上そのものが動く/反転しうるため、子の絶対位置を固定したままサイズだけ
   変えるとcの相対配置が崩れる。`oldRect`はrootIndex自身のジェスチャー開始時
   点の可視矩形、`newRect`は現在フレームでの可視矩形。子（Goal/ボタンを除く）
   はrootIndexに対する相対オフセット・相対サイズを保ったまま追従する。
   Player/Goal/ボタンはHierarchy_ApplyScaleと同じ「サイズのみ変更、位置固定」
   のまま扱う（内部でApplyScaleToSpecialChildren相当を呼ぶ）。 */
void Hierarchy_ApplyRelativeTransform(int rootIndex, RECT oldRect, RECT newRect);

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
