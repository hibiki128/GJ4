#include "BossAttackSlam.h"
#include "src/Boss/Boss.h"
#include "src/Boss/Effect/BossParticles.h"
#include "src/Interface/ITargetLocator.h"
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

    // 動き出した時点で落下地点を決めてしまう。以降は追わないので、
    // 相手は「予告が出た場所から離れる」だけで確実に避けられる
    UpdateLandingPoint(context);
    UpdateMarker(true, pParams_ ? pParams_->impactRadius : 1.0f);
    UpdateFillRatio(0.0f);
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
    marker_.Draw(viewProjection);
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

    // 飛び上がっているあいだも、決まった落下地点の予告を出し続ける
    UpdateFillRatio(CalcFillRatio(timer_));
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


    // 落下地点は動き出しで決まっているのでもう動かさない。
    // 予告の塗りだけを着弾に向けて広げる
    const float duration = (std::max)(0.01f, scaledAimTime_);
    UpdateFillRatio(CalcFillRatio(pParams_->riseTime + timer_));

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

    // 落下中も塗りを広げ続け、着弾でちょうど外枠と同じ大きさになる
    UpdateFillRatio(CalcFillRatio(pParams_->riseTime + scaledAimTime_ + timer_));

    if (timer_ >= duration) {
        boss->SetBossPosition(landingPoint_);
        phase_ = Phase::Impact;
        timer_ = 0.0f;

        // 着弾。地面を叩いた土煙を出し、輪の内側にいればダメージ
        BossParticles::GetInstance()->BurstOnGround(BossParticles::Id::SlamDust, landingPoint_);
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
        // 次の跳躍も、動き出しの時点で落下地点を決める
        phase_ = Phase::Rise;
        UpdateLandingPoint(context);
        UpdateMarker(true, pParams_->impactRadius);
        UpdateFillRatio(0.0f);
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
    marker_.Ensure("BossSlam");
}

void BossAttackSlam::UpdateMarker(bool visible, float radius) {
    if (!visible) {
        marker_.Hide();
        return;
    }
    // 外枠は攻撃範囲そのもの。着弾までの進み具合は UpdateFillRatio が入れる
    marker_.Show(landingPoint_, radius);
}

void BossAttackSlam::UpdateFillRatio(float ratio) {
    marker_.SetFillRatio(ratio);
}

float BossAttackSlam::CalcFillRatio(float elapsed) const {
    // 狙い〜落下を1本の時間として見て、着弾でちょうど1になるようにする
    const float total = (std::max)(0.01f, scaledAimTime_ + pParams_->fallTime);
    return (std::min)(elapsed / total, 1.0f);
}
