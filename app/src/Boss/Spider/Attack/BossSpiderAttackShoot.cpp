#include "BossSpiderAttackShoot.h"
#include "MyMath.h"
#include "Random.h"
#include "src/Boss/Spider/BossSpider.h"
#include "src/Interface/ITargetLocator.h"
#include <algorithm>
#include <cmath>
#include <numbers>

using namespace Hagine;

void BossSpiderAttackShoot::Start(const BossAttackContext &context) {
    if (!context.spider) {
        phase_ = Phase::Finished;
        return;
    }
    phase_ = Phase::Telegraph;
    timer_ = 0.0f;
    shotTimer_ = 0.0f;
    firedCount_ = 0;
}

void BossSpiderAttackShoot::Update(const BossAttackContext &context) {
    if (!context.spider || phase_ == Phase::Finished) {
        phase_ = Phase::Finished;
        return;
    }
    BossSpider *spider = context.spider;
    timer_ += context.deltaTime;

    // 撃つあいだは相手を向き続ける（弾は撃った瞬間の向きへ飛ぶ）
    if (context.target && context.target->IsTargetValid()) {
        spider->FaceTowards(context.target->GetTargetPosition());
    }

    switch (phase_) {
    case Phase::Telegraph: {
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
        ++firedCount_;
        if (firedCount_ >= (std::max)(1, pParams_->shotCount)) {
            phase_ = Phase::Recover;
            timer_ = 0.0f;
        }
        break;
    }
    case Phase::Recover: {
        if (timer_ >= pParams_->recoverTime) {
            phase_ = Phase::Finished;
        }
        break;
    }
    default:
        break;
    }
}

void BossSpiderAttackShoot::Cancel(const BossAttackContext &context) {
    (void)context;
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
