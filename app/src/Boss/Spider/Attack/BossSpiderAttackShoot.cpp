#include "BossSpiderAttackShoot.h"
#include "MyMath.h"
#include "src/Boss/Data/BossEasing.h"
#include "Random.h"
#include "src/Boss/Spider/BossSpider.h"
#include "src/Interface/ITargetLocator.h"
#include <algorithm>
#include <cmath>
#include <numbers>

using namespace Hagine;

namespace {

/// <summary>反動のうち、沈み込みに使う割合（残りは戻りに使う）</summary>
constexpr float kRecoilDropRatio = 0.18f;

} // namespace

void BossSpiderAttackShoot::Start(const BossAttackContext &context) {
    if (!context.spider) {
        phase_ = Phase::Finished;
        return;
    }
    phase_ = Phase::Telegraph;
    timer_ = 0.0f;
    shotTimer_ = 0.0f;
    firedCount_ = 0;
    recoilTimer_ = 999.0f;

    standHeight_ = context.spider->GetStandHeight();
    basePosition_ = context.spider->GetBodyPosition();
    basePosition_.y = standHeight_;
}

void BossSpiderAttackShoot::Update(const BossAttackContext &context) {
    if (!context.spider || phase_ == Phase::Finished) {
        phase_ = Phase::Finished;
        return;
    }
    BossSpider *spider = context.spider;
    timer_ += context.deltaTime;
    recoilTimer_ += context.deltaTime;

    // 撃つあいだは相手を向き続ける（弾は撃った瞬間の向きへ飛ぶ）
    if (context.target && context.target->IsTargetValid()) {
        spider->FaceTowards(context.target->GetTargetPosition());
    }

    const float raised = standHeight_ + pParams_->telegraphRise;
    float baseHeight = raised;

    switch (phase_) {
    case Phase::Telegraph: {
        // 撃つ前に胴を持ち上げる。脚は接地したままなので、伸び上がる形になる
        const float progress = std::clamp(timer_ / (std::max)(0.01f, pParams_->telegraphTime), 0.0f, 1.0f);
        baseHeight = Lerp(standHeight_, raised, SmoothInOut(progress));
        if (timer_ >= pParams_->telegraphTime) {
            phase_ = Phase::Fire;
            timer_ = 0.0f;
            shotTimer_ = pParams_->shotInterval; // 溜め終わりに1発目
        }
        break;
    }
    case Phase::Fire: {
        shotTimer_ += context.deltaTime;
        if (shotTimer_ < pParams_->shotInterval) {
            break;
        }
        shotTimer_ = 0.0f;

        Vector3 direction{std::cos(spider->GetBodyYaw()), 0.0f, std::sin(spider->GetBodyYaw())};
        if (context.target && context.target->IsTargetValid()) {
            Vector3 toTarget = context.target->GetTargetPosition() - spider->GetBodyPosition();
            if (toTarget.LengthSq() > 0.0001f) {
                direction = toTarget.Normalize();
            }
        }
        // 1発ごとに左右へ少しばらけさせる（同じ線に並ばないように）
        const float spread = pParams_->spreadDegrees * (std::numbers::pi_v<float> / 180.0f);
        const float angle = Random::Range(-spread, spread);
        const float cosAngle = std::cos(angle);
        const float sinAngle = std::sin(angle);
        const Vector3 spreadDirection{direction.x * cosAngle - direction.z * sinAngle, direction.y,
                                      direction.x * sinAngle + direction.z * cosAngle};

        spider->FireBullet(spreadDirection, *pParams_);
        recoilTimer_ = 0.0f; // 撃った反動をここから始める
        ++firedCount_;
        if (firedCount_ >= (std::max)(1, pParams_->shotCount)) {
            phase_ = Phase::Recover;
            timer_ = 0.0f;
        }
        break;
    }
    case Phase::Recover: {
        // 伸ばした体を元の高さへ戻す
        const float progress = std::clamp(timer_ / (std::max)(0.01f, pParams_->recoverTime), 0.0f, 1.0f);
        baseHeight = Lerp(raised, standHeight_, SmoothInOut(progress));
        if (progress >= 1.0f) {
            phase_ = Phase::Finished;
        }
        break;
    }
    default:
        break;
    }

    // --- 1発ごとの反動と震え ---
    // 撃った瞬間にすとんと沈め、戻りながら細かく震わせる。
    // 震えは反動が収まるにつれて小さくなるので、撃つたびに揺れて見える
    Vector3 offset{0.0f, 0.0f, 0.0f};
    const float recoilDuration = (std::max)(0.01f, pParams_->recoilTime);
    if (recoilTimer_ < recoilDuration) {
        const float progress = recoilTimer_ / recoilDuration;
        const float dip = (progress < kRecoilDropRatio)
                              ? ApplyEasing(EasingType::OutQuad, 0.0f, 1.0f, progress / kRecoilDropRatio, 1.0f)
                              : 1.0f - SmoothInOut((progress - kRecoilDropRatio) / (1.0f - kRecoilDropRatio));
        offset.y -= pParams_->recoilDepth * dip;

        const float decay = 1.0f - progress;
        const float amount = pParams_->shakeAmount * decay;
        offset.y += std::sin(recoilTimer_ * pParams_->shakeSpeed) * amount;
        // 横にも少しずらすと「震えている」感じが出る（上下だけだと弾んで見える）
        const float sideYaw = spider->GetBodyYaw() + std::numbers::pi_v<float> * 0.5f;
        const float side = std::sin(recoilTimer_ * pParams_->shakeSpeed * 1.7f) * amount;
        offset.x += std::cos(sideYaw) * side;
        offset.z += std::sin(sideYaw) * side;
    }

    spider->SetBodyPosition(Vector3{basePosition_.x + offset.x, baseHeight + offset.y,
                                    basePosition_.z + offset.z});
}

void BossSpiderAttackShoot::Cancel(const BossAttackContext &context) {
    if (context.spider) {
        // 伸び上がったままにしない
        Vector3 position = context.spider->GetBodyPosition();
        position.y = standHeight_;
        context.spider->SetBodyPosition(position);
    }
    // 撃った弾はそのまま飛ばしておく（途中で消すと理不尽に見えるため）
    phase_ = Phase::Finished;
}

const char *BossSpiderAttackShoot::GetPhaseName() const {
    switch (phase_) {
    case Phase::Telegraph:
        return "溜め";
    case Phase::Fire:
        return "発射";
    case Phase::Recover:
        return "硬直";
    default:
        return "終了";
    }
}
