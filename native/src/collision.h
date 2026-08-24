#ifndef COLLISION_H
#define COLLISION_H

#include <windows.h>

typedef struct {
    int excludeIndex;      /* -1 = なし */
    int excludeChildren;   /* excludeIndexの子孫も除外する */
    int checkNormalWindows; /* 通常/移動可能/リサイズ可能ウィンドウ、ボタン、プレイヤーも
                                障害物として扱う（移動中のウィンドウ自体がNoEntryの場合に使用
                                -- CollisionFilter.CreateStandardOptionsがexcludeWindow.IsNoEntryWindow
                                に基づいてCheckNormalWindows/CheckButtons/CheckPlayerを
                                まとめて設定する処理と一致） */
} CollisionOptions;

RECT Collision_ValidatePosition(RECT current, RECT proposed, CollisionOptions opts);
SIZE Collision_ValidateSize(RECT current, SIZE proposed, CollisionOptions opts);

/* Collision_ValidateSizeの下限/上限を差し替えられる版。制限なしリサイズ+
   反転ウィンドウ(WT_UNCONSTRAINED*)はMIN_WINDOW_SIZE(100)ではなく、より
   小さいUNCONSTRAINED_MIN_ABS_SIZEを絶対サイズの下限に使うため、こちらを
   呼ぶ。Collision_ValidateSizeはminSize=MIN_WINDOW_SIZE, maxSize=MAX_WINDOW_SIZE
   でこちらに委譲する。 */
SIZE Collision_ValidateSizeEx(RECT current, SIZE proposed, CollisionOptions opts, int minSize, int maxSize);

/* BaseWindowStrategy.CheckCollision(window, checkBounds)相当: スイープ経路では
   なく、指定した矩形がその場で障害物と重なっているかどうかだけを判定する
   静的な当たり判定。MovableWindowStrategy.UpdateBlockFlagsが1px先読みの
   ブロック方向判定に使う。 */
int Collision_CheckOverlap(RECT bounds, CollisionOptions opts);

#endif
