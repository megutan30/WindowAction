#ifndef ANIMATION_H
#define ANIMATION_H

typedef enum {
    ANIM_IDLE,
    ANIM_RUNNING,
    ANIM_JUMPING,
    ANIM_FALLING,
    ANIM_LANDING
} PlayerAnimState;

typedef struct {
    float scaleX, scaleY;
    float targetScaleX, targetScaleY;
    PlayerAnimState state;
    int stateTimer;
    int globalTimer;
} PlayerAnimation;

void Anim_Init(PlayerAnimation *a);
void Anim_StartJump(PlayerAnimation *a);
void Anim_ResetScale(PlayerAnimation *a);

/* PlayerAnimation.UpdateAnimationState を反映: 接地状態/移動の変化から
   次の状態を決定する。Anim_Update の前に毎フレーム1回呼び出すこと。 */
void Anim_UpdateState(PlayerAnimation *a, int isGrounded, int wasGrounded, float dx);

/* PlayerAnimation.UpdateAnimation を反映: 状態ごとの目標スケールを進め、
   現在のスケールをそこへ向けて補間する。distanceToTop は
   PlayerForm.CalculateDistanceToMovableBoundsTop に対応（FLT_MAX = 制限なし）。 */
void Anim_Update(PlayerAnimation *a, float distanceToTop, float originalHeight);

#endif
