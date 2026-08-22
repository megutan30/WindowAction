#ifndef HIERARCHY_H
#define HIERARCHY_H

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
