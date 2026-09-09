#include "BossAttackSpin.h"
#include "src/Boss/Boss.h"
#include "src/Audio/GameSounds.h"
#include "src/Boss/Effect/BossParticles.h"
#include "src/Interface/ITargetLocator.h"
#include "Easing.h"
#include "MyMath.h"
#include "line/LineRenderer.h"
#include <algorithm>

using namespace Hagine;

namespace {
/// <summary>予兆のこの割合を過ぎたら突進方向を固定する（残りは回避のための猶予）</summary>
constexpr float kAimLockRatio = 0.7f;

/// <summary>突進中に土煙を出す間隔（秒）。毎フレーム出すと濃すぎるので間引く</summary>
constexpr float kDashDustInterval = 0.06f;
} // namespace

void BossAttackSpin::Start(const BossAttackContext &context) {
    phase_ = Phase::Telegraph;
    timer_ = 0.0f;
    spinSpeed_ = 0.0f;
    hitApplied_ = false;
    dustTimer_ = 0.0f;
    dashTravel_ = 0.0f;

    // 露出度が上がるほど予兆が短く、突進が速くなる。
    // 攻撃の途中で値が揺れないよう、開始時に確定させる
    const float exposure = std::clamp(context.exposure, 0.0f, 1.0f);
    if (pParams_ && pExposureParams_) {
        scaledTelegraphTime_ = pParams_->telegraphTime *
                               Lerp(1.0f, pExposureParams_->spinTelegraphScaleAtFull, exposure);
        scaledDashSpeed_ = pParams_->dashSpeed *
                           Lerp(1.0f, pExposureParams_->spinDashSpeedScaleAtFull, exposure);
    }

    // 回っているあいだ鳴らし続ける。止めるのは終わったときと中断されたとき（ひるみ含む）
    GameSounds::GetInstance()->StartLoop(GameSounds::Id::RotateSphere);

    AimAtTarget(context);
}

void BossAttackSpin::Update(const BossAttackContext &context) {
    if (!pParams_ || !pWallStaggerParams_ || !context.boss) {
        phase_ = Phase::Finished;
        return;
    }

    timer_ += context.deltaTime;

    switch (phase_) {
    case Phase::Telegraph:
        UpdateTelegraph(context);
        break;
    case Phase::Dash:
        UpdateDash(context);
        break;
    case Phase::Recover:
        UpdateRecover(context);
        break;
    case Phase::Finished:
        break;
    }

    // どの段階でも回している自転を反映する
    context.boss->AddSpin(spinSpeed_ * context.deltaTime);
}

void BossAttackSpin::Cancel(const BossAttackContext &context) {
    (void)context;
    phase_ = Phase::Finished;
    spinSpeed_ = 0.0f;
    GameSounds::GetInstance()->StopLoop(GameSounds::Id::RotateSphere);
}

const char *BossAttackSpin::GetPhaseName() const {
    switch (phase_) {
    case Phase::Telegraph:
        return "予兆";
    case Phase::Dash:
        return "突進";
    case Phase::Recover:
        return "硬直";
    default:
        return "終了";
    }
}

void BossAttackSpin::UpdateTelegraph(const BossAttackContext &context) {
    const float duration = (std::max)(0.01f, scaledTelegraphTime_);
    const float progress = (std::min)(timer_ / duration, 1.0f);

    // 自転を徐々に上げる（回転が上がりきると突進が来る、という読みを与える）
    spinSpeed_ = ApplyEasing(EasingType::InQuad, 0.0f, pParams_->telegraphSpinSpeed, progress, 1.0f);

    // 終盤までは追尾し、そこから先は固定する（固定後は横へ抜ければ回避できる）
    if (progress < kAimLockRatio) {
        AimAtTarget(context);
    } else {
        DrawTelegraph(context, (progress - kAimLockRatio) / (1.0f - kAimLockRatio));
    }

    if (timer_ >= duration) {
        phase_ = Phase::Dash;
        timer_ = 0.0f;
        hitApplied_ = false;
    }
}

void BossAttackSpin::UpdateDash(const BossAttackContext &context) {
    Boss *boss = context.boss;

    const float step = scaledDashSpeed_ * context.deltaTime;
    const Vector3 position = boss->GetBossPosition() + dashDirection_ * step;
    boss->SetBossPosition(position);
    dashTravel_ += step;

    // 削るように土を巻き上げながら進む
    dustTimer_ += context.deltaTime;
    if (dustTimer_ >= kDashDustInterval) {
        dustTimer_ -= kDashDustInterval;
        BossParticles::GetInstance()->BurstOnGround(BossParticles::Id::DashTrail, boss->GetBossPosition());
    }

    // 壁に激突したらそこで突進は終わり、ボスはひるむ。
    // 判定は中心ではなく進行方向の先端で見る（見た目に触れる前に止まらないように）。
    // 壁際で出したときに即ひるまないよう、ある程度進んでいることを条件にする
    const Vector3 nose = boss->GetBossPosition() + dashDirection_ * boss->GetBodyRadius();
    if (dashTravel_ >= pWallStaggerParams_->minTravel && boss->IsBeyondBounds(nose)) {
        BossParticles::GetInstance()->BurstOnGround(BossParticles::Id::SlamDust, boss->GetBossPosition());
        boss->BeginWallStagger();
        spinSpeed_ = 0.0f;
        phase_ = Phase::Finished;
        GameSounds::GetInstance()->StopLoop(GameSounds::Id::RotateSphere);
        return;
    }

    // 接触判定（1回の突進につき1度だけ当てる）
    if (!hitApplied_) {
        const float reach = boss->GetBodyRadius() + pParams_->contactMargin;
        if (boss->IsTargetWithin(boss->GetBossPosition(), reach)) {
            boss->DealDamageToTarget(pParams_->damage, boss->GetBossPosition());
            hitApplied_ = true;
        }
    }

    if (timer_ >= pParams_->dashTime) {
        phase_ = Phase::Recover;
        timer_ = 0.0f;
    }
}

void BossAttackSpin::UpdateRecover(const BossAttackContext &context) {
    const float duration = (std::max)(0.01f, pParams_->recoverTime);
    const float progress = (std::min)(timer_ / duration, 1.0f);

    // 回転を通常の緩やかな自転まで落とす
    const float idleSpin = context.boss->GetParameters().Battle().idleSpinSpeed;
    spinSpeed_ = ApplyEasing(EasingType::OutQuad, pParams_->telegraphSpinSpeed, idleSpin, progress, 1.0f);

    if (timer_ >= duration) {
        phase_ = Phase::Finished;
        GameSounds::GetInstance()->StopLoop(GameSounds::Id::RotateSphere);
    }
}

void BossAttackSpin::AimAtTarget(const BossAttackContext &context) {
    if (!context.target || !context.target->IsTargetValid() || !context.boss) {
        return;
    }
    Vector3 toTarget = context.target->GetTargetPosition() - context.boss->GetBossPosition();
    toTarget.y = 0.0f; // 水平方向のみ（突進は地面沿い）
    if (toTarget.LengthSq() > 0.0001f) {
        dashDirection_ = toTarget.Normalize();
    }
}

void BossAttackSpin::DrawTelegraph(const BossAttackContext &context, float intensity) const {
    const Boss *boss = context.boss;
    const Vector3 origin = boss->GetBossPosition();
    const float distance = scaledDashSpeed_ * pParams_->dashTime;

    // 突進線は固定後にだけ出す。色を徐々に強めて発射タイミングを読ませる
    const Vector4 color{1.0f, 0.35f + 0.5f * intensity, 0.2f, 1.0f};
    LineRenderer *lineRenderer = LineRenderer::GetInstance();
    lineRenderer->AddLine(origin, origin + dashDirection_ * distance, color);

    const float reach = boss->GetBodyRadius() + pParams_->contactMargin;
    const Vector3 landing = origin + dashDirection_ * distance;
    lineRenderer->AddCircle({landing.x, 0.05f, landing.z}, {reach, 0.0f, 0.0f}, {0.0f, 0.0f, reach}, color, 20);
}
