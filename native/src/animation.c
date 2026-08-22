/* Animation/PlayerAnimation.cs と AnimationCurves.cs から移植。 */
#include "animation.h"
#include <math.h>
#include <float.h>

#define ANIM_LERP_SPEED 0.25f
#define ANIM_DURATION 30 /* フレーム数 */

static float StretchAndBack(float x)
{
    if (x <= 0.0f || x >= 1.0f)
        return 1.0f;
    return 1.0f + sinf(x * 3.14159265358979323846f) * 0.5f;
}

static float EaseOutElastic(float x)
{
    if (x <= 0.0f)
        return 0.0f;
    if (x >= 1.0f)
        return 1.0f;
    const float c4 = (2.0f * 3.14159265358979323846f) / 3.0f;
    return powf(2.0f, -10.0f * x) * sinf((x * 10.0f - 0.75f) * c4) + 1.0f;
}

void Anim_Init(PlayerAnimation *a)
{
    a->scaleX = a->scaleY = 1.0f;
    a->targetScaleX = a->targetScaleY = 1.0f;
    a->state = ANIM_IDLE;
    a->stateTimer = 0;
    a->globalTimer = 0;
}

void Anim_StartJump(PlayerAnimation *a)
{
    a->state = ANIM_JUMPING;
    a->stateTimer = 0;
}

void Anim_ResetScale(PlayerAnimation *a)
{
    a->scaleX = a->scaleY = 1.0f;
    a->targetScaleX = a->targetScaleY = 1.0f;
}

void Anim_UpdateState(PlayerAnimation *a, int isGrounded, int wasGrounded, float dx)
{
    if (isGrounded && !wasGrounded)
    {
        a->state = ANIM_LANDING;
        a->stateTimer = 0;
        return;
    }

    if (a->state == ANIM_LANDING)
    {
        if (a->stateTimer >= ANIM_DURATION)
        {
            a->stateTimer = 0;
            a->state = (dx == 0.0f) ? ANIM_IDLE : ANIM_RUNNING;
        }
        else if (dx != 0.0f)
        {
            a->state = ANIM_RUNNING;
            a->stateTimer = 0;
        }
        return;
    }

    if (a->state == ANIM_JUMPING)
    {
        if (isGrounded)
        {
            a->state = ANIM_LANDING;
            a->stateTimer = 0;
        }
        else if (a->stateTimer >= ANIM_DURATION)
        {
            a->state = ANIM_FALLING;
        }
        return;
    }

    if (isGrounded)
    {
        a->state = (dx == 0.0f) ? ANIM_IDLE : ANIM_RUNNING;
        return;
    }

    a->state = ANIM_JUMPING;
    a->stateTimer = 0;
}

static float CalculateScaleLimit(float distanceToTop, float originalHeight)
{
    float threshold = originalHeight * 0.5f;
    if (distanceToTop >= threshold)
        return FLT_MAX;
    float ratio = distanceToTop / threshold;
    return 1.0f + 0.5f * ratio;
}

void Anim_Update(PlayerAnimation *a, float distanceToTop, float originalHeight)
{
    a->globalTimer++;
    if (a->state == ANIM_JUMPING || a->state == ANIM_LANDING)
        a->stateTimer++;

    float progress = (float)a->stateTimer / (float)ANIM_DURATION;

    switch (a->state)
    {
    case ANIM_IDLE:
        a->targetScaleY = 1.0f + sinf(a->globalTimer * 0.05f) * 0.05f;
        a->targetScaleX = 1.0f / a->targetScaleY;
        break;

    case ANIM_RUNNING:
        a->targetScaleY = 1.0f + fabsf(sinf(a->globalTimer * 0.25f)) * 0.2f;
        a->targetScaleX = 1.0f / a->targetScaleY;
        break;

    case ANIM_JUMPING:
    {
        a->targetScaleY = StretchAndBack(progress);
        float limit = CalculateScaleLimit(distanceToTop, originalHeight);
        if (a->targetScaleY > limit)
            a->targetScaleY = limit;
        a->targetScaleX = 1.0f / a->targetScaleY;
        break;
    }

    case ANIM_FALLING:
    {
        a->targetScaleY = StretchAndBack(1.0f);
        float limit = CalculateScaleLimit(distanceToTop, originalHeight);
        if (a->targetScaleY > limit)
            a->targetScaleY = limit;
        a->targetScaleX = 1.0f / a->targetScaleY;
        break;
    }

    case ANIM_LANDING:
    {
        float landingProgress = EaseOutElastic(progress);
        a->targetScaleY = 1.0f - 0.6f * (1.0f - landingProgress);
        a->targetScaleX = 1.0f / a->targetScaleY;
        break;
    }
    }

    a->scaleX += (a->targetScaleX - a->scaleX) * ANIM_LERP_SPEED;
    a->scaleY += (a->targetScaleY - a->scaleY) * ANIM_LERP_SPEED;
}
