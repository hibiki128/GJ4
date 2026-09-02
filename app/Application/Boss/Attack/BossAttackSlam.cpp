#include "BossAttackSlam.h"
#include "Application/Boss/Boss.h"
#include "Application/Interface/ITargetLocator.h"
#include "Easing.h"
#include "MyMath.h"
#include "camera/projection/ViewProjection.h"
#include <algorithm>
#include <cmath>
#include <numbers>

using namespace Hagine;

namespace {
/// <summary>着弾予告の輪を地面から少し浮かせる高さ（Zファイティング回避）</summary>
constexpr float kMarkerHeight = 0.05f;
} // namespace

void BossAttackSlam::Start(const BossAttackContext &context) {
    phase_ = Phase::Rise;
    timer_ = 0.0f;
    slamIndex_ = 0;

    // 露出度が上がるほど落下回数が増え、狙いの時間（逃げる猶予）が短くなる。
    // 攻撃の途中で値が揺れないよう、開始時に確定させる
    const float exposure = std::clamp(context.exposure, 0.0f, 1.0f);
    if (pParams_ && pExposureParams_) {
        const float count = Lerp(static_cast<float>(pExposureParams_->slamCountAtZero),
                                 static_cast<float>(pExposureParams_->slamCountAtFull), exposure);
        slamCount_ = (std::max)(1, static_cast<int>(std::lround(count)));
        scaledAimTime_ = pParams_->aimTime * Lerp(1.0f, pExposureParams_->slamAimScaleAtFull, exposure);
    }

    if (context.boss) {
        phaseStart_ = context.boss->GetBossPosition();
        landingPoint_ = phaseStart_;
    }
    EnsureMarker();
    UpdateMarker(false, 1.0f);
}

void BossAttackSlam::Update(const BossAttackContext &context) {
    if (!pParams_ || !context.boss) {
        phase_ = Phase::Finished;
        return;
    }

    timer_ += context.deltaTime;

    switch (phase_) {
    case Phase::Rise:
        UpdateRise(context);
        break;
    case Phase::Aim:
        UpdateAim(context);
        break;
    case Phase::Fall:
        UpdateFall(context);
        break;
    case Phase::Impact:
        UpdateImpact(context);
        break;
    case Phase::Recover:
        UpdateRecover(context);
        break;
    case Phase::Finished:
        break;
    }

    // 落下攻撃中も緩やかな自転は続ける（死角のパーツを見せ続けるため）
    context.boss->AddSpin(context.boss->GetParameters().Battle().idleSpinSpeed * context.deltaTime);
}

void BossAttackSlam::Cancel(const BossAttackContext &context) {
    // 空中で怯むと浮いたままになるので、地面の高さへ戻してから終了する
    if (context.boss) {
        Vector3 position = context.boss->GetBossPosition();
        position.y = context.boss->GetHomePosition().y;
        context.boss->SetBossPosition(position);
    }
    UpdateMarker(false, 1.0f);
    phase_ = Phase::Finished;
}

void BossAttackSlam::Draw(const ViewProjection &viewProjection) {
    if (marker_ && marker_->GetIsModelDraw()) {
        marker_->Draw(viewProjection);
    }
}

const char *BossAttackSlam::GetPhaseName() const {
    switch (phase_) {
    case Phase::Rise:
        return "飛び上がり";
    case Phase::Aim:
        return "狙い";
    case Phase::Fall:
        return "落下";
    case Phase::Impact:
        return "着弾";
    case Phase::Recover:
        return "硬直";
    default:
        return "終了";
    }
}

void BossAttackSlam::UpdateRise(const BossAttackContext &context) {
    Boss *boss = context.boss;
    const float duration = (std::max)(0.01f, pParams_->riseTime);
    const float progress = (std::min)(timer_ / duration, 1.0f);

    const float apexY = boss->GetHomePosition().y + pParams_->riseHeight;
    Vector3 position = boss->GetBossPosition();
    position.y = ApplyEasing(EasingType::OutQuad, phaseStart_.y, apexY, progress, 1.0f);
    boss->SetBossPosition(position);

    if (timer_ >= duration) {
        apexPosition_ = position;
        phase_ = Phase::Aim;
        timer_ = 0.0f;
    }
}

void BossAttackSlam::UpdateAim(const BossAttackContext &context) {
    UpdateLandingPoint(context);

    // 狙いの終盤ほど輪を小さく締めて、着弾の瞬間を読ませる
    const float duration = (std::max)(0.01f, scaledAimTime_);
    const float progress = (std::min)(timer_ / duration, 1.0f);
    UpdateMarker(true, pParams_->impactRadius * (1.0f - 0.25f * progress));

    if (timer_ >= duration) {
        phase_ = Phase::Fall;
        timer_ = 0.0f;
        phaseStart_ = context.boss->GetBossPosition();
    }
}

void BossAttackSlam::UpdateFall(const BossAttackContext &context) {
    Boss *boss = context.boss;
    const float duration = (std::max)(0.01f, pParams_->fallTime);
    const float progress = (std::min)(timer_ / duration, 1.0f);

    boss->SetBossPosition(ApplyEasing(EasingType::InQuad, phaseStart_, landingPoint_, progress, 1.0f));

    if (timer_ >= duration) {
        boss->SetBossPosition(landingPoint_);
        phase_ = Phase::Impact;
        timer_ = 0.0f;

        // 着弾。輪の内側にいればダメージ
        UpdateMarker(false, 1.0f);
        if (boss->IsTargetWithin(landingPoint_, pParams_->impactRadius)) {
            boss->DealDamageToTarget(pParams_->damage, landingPoint_);
        }
    }
}

void BossAttackSlam::UpdateImpact(const BossAttackContext &context) {
    if (timer_ < pParams_->impactTime) {
        return;
    }

    ++slamIndex_;
    timer_ = 0.0f;
    phaseStart_ = context.boss->GetBossPosition();

    if (slamIndex_ < slamCount_) {
        phase_ = Phase::Rise; // まだ回数が残っていれば再度飛び上がる
    } else {
        phase_ = Phase::Recover;
    }
}

void BossAttackSlam::UpdateRecover(const BossAttackContext &context) {
    (void)context;
    if (timer_ >= pParams_->recoverTime) {
        phase_ = Phase::Finished;
    }
}

void BossAttackSlam::UpdateLandingPoint(const BossAttackContext &context) {
    if (!context.target || !context.target->IsTargetValid()) {
        return;
    }
    const Vector3 targetPosition = context.target->GetTargetPosition();
    // 着弾時のボス中心は、地面に接する高さ（＝初期位置の高さ）に合わせる
    landingPoint_ = Vector3{targetPosition.x, context.boss->GetHomePosition().y, targetPosition.z};
}

void BossAttackSlam::EnsureMarker() {
    if (marker_) {
        return;
    }
    marker_ = std::make_unique<BaseObject>();
    marker_->Init("BossSlamMarker");
    marker_->CreatePrimitiveModel(PrimitiveType::Ring);
    marker_->SetShouldSave(false);
    marker_->SetGizmoSelectable(false);
    marker_->SetTexture(kBossTexturePath);
    marker_->SetColor({1.0f, 0.35f, 0.25f, 1.0f});
    marker_->GetLighting() = false;

    // Ring は XY 平面に作られるので、X軸まわりに90度倒して地面へ寝かせる
    marker_->GetWorldTransform()->quaternionRotation_ =
        Quaternion::FromAxisAngle({1.0f, 0.0f, 0.0f}, std::numbers::pi_v<float> * 0.5f);
    marker_->SetIsModelDraw(false);
}

void BossAttackSlam::UpdateMarker(bool visible, float scale) {
    if (!marker_) {
        return;
    }
    marker_->SetIsModelDraw(visible);
    if (!visible) {
        return;
    }

    WorldTransform *transform = marker_->GetWorldTransform();
    transform->translation_ = Vector3{landingPoint_.x, kMarkerHeight, landingPoint_.z};
    transform->scale_ = Vector3{scale, scale, scale};
    transform->UpdateMatrix();
}
