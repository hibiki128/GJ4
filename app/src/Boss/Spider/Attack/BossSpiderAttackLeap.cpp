#include "BossSpiderAttackLeap.h"
#include "MyMath.h"
#include "Random.h"
#include "src/Boss/Data/BossEasing.h"
#include "src/Boss/Spider/BossSpider.h"
#include "src/Interface/ITargetLocator.h"
#include <algorithm>
#include <cmath>
#include <numbers>

using namespace Hagine;

void BossSpiderAttackLeap::Start(const BossAttackContext &context) {
    if (!context.spider) {
        phase_ = Phase::Finished;
        return;
    }
    standHeight_ = context.spider->GetStandHeight();
    hopIndex_ = 0;
    timer_ = 0.0f;
    phase_ = Phase::Crouch;
    phaseStart_ = context.spider->GetBodyPosition();
    PickLandingPoint(context);
}

void BossSpiderAttackLeap::PickLandingPoint(const BossAttackContext &context) {
    const Vector3 body = context.spider->GetBodyPosition();

    // 相手の「付近」へ落ちる。ぴったり真上を狙わないので、
    // 相手は範囲の外へ逃げられるが、まぐれでは避けられない
    Vector3 center = body;
    if (context.target && context.target->IsTargetValid()) {
        center = context.target->GetTargetPosition();
    }
    const float spread = (std::max)(0.0f, pParams_->landSpread);
    const float angle = Random::Range(0.0f, 2.0f * std::numbers::pi_v<float>);
    // 面積で一様になるよう半径は平方根で散らす（中心に寄りすぎない）
    const float distance = spread * std::sqrt(Random::Range(0.0f, 1.0f));
    Vector3 landing{center.x + std::cos(angle) * distance, 0.0f, center.z + std::sin(angle) * distance};

    // 1回で跳べる距離には上限を設ける（遠すぎると瞬間移動に見える）
    Vector3 offset = landing - body;
    offset.y = 0.0f;
    const float length = offset.Length();
    const float limit = (std::max)(0.1f, pParams_->maxLeapRange);
    if (length > limit) {
        offset = offset / length * limit;
    }
    landingPoint_ = Vector3{body.x + offset.x, 0.0f, body.z + offset.z};
    apexPosition_ = Vector3{landingPoint_.x, standHeight_ + pParams_->apexHeight, landingPoint_.z};
}

void BossSpiderAttackLeap::Update(const BossAttackContext &context) {
    if (!context.spider || phase_ == Phase::Finished) {
        phase_ = Phase::Finished;
        return;
    }
    BossSpider *spider = context.spider;
    timer_ += context.deltaTime;

    switch (phase_) {
    case Phase::Crouch: {
        // 助走のように胴だけ沈める。足は地面に着いたまま＝脚が縮んで溜めて見える
        const float progress = std::clamp(timer_ / (std::max)(0.01f, pParams_->crouchTime), 0.0f, 1.0f);
        Vector3 position = phaseStart_;
        position.y = standHeight_ - pParams_->crouchDepth * SmoothInOut(progress);
        spider->SetBodyPosition(position);
        spider->FaceTowards(landingPoint_);
        if (progress >= 1.0f) {
            phase_ = Phase::Rise;
            timer_ = 0.0f;
            phaseStart_ = spider->GetBodyPosition();
            // 浮いているあいだ脚を畳む。一気に切り替えると足がワープするので時間をかける
            spider->SetLegTuck(pParams_->legTuck, pParams_->legFoldTime);
        }
        break;
    }
    case Phase::Rise: {
        // 着地点の真上まで飛び上がる
        const float progress = std::clamp(timer_ / (std::max)(0.01f, pParams_->riseTime), 0.0f, 1.0f);
        spider->SetBodyPosition(Lerp(phaseStart_, apexPosition_, SmoothInOut(progress)));
        if (progress >= 1.0f) {
            phase_ = Phase::Fall;
            timer_ = 0.0f;
            phaseStart_ = apexPosition_;
        }
        break;
    }
    case Phase::Fall: {
        // 真上から真下へ。落ちるほど速くする
        const float progress = std::clamp(timer_ / (std::max)(0.01f, pParams_->fallTime), 0.0f, 1.0f);
        const Vector3 ground{landingPoint_.x, standHeight_, landingPoint_.z};
        spider->SetBodyPosition(ApplyEasing(EasingType::InQuad, phaseStart_, ground, progress, 1.0f));
        if (progress >= 1.0f) {
            phase_ = Phase::Impact;
            timer_ = 0.0f;
            // 足を地面へ戻す。ここも補間する（置き直すと着地の瞬間に足が飛ぶ）
            spider->SetLegTuck(0.0f, pParams_->legFoldTime);
            spider->ReportHit(Vector3{landingPoint_.x, 0.0f, landingPoint_.z},
                              pParams_->impactRadius, pParams_->damage);
        }
        break;
    }
    case Phase::Impact: {
        if (timer_ < pParams_->impactTime) {
            break;
        }
        ++hopIndex_;
        timer_ = 0.0f;
        if (hopIndex_ >= (std::max)(1, pParams_->hopCount)) {
            phase_ = Phase::Recover;
            break;
        }
        // 続けてもう一度跳ぶ
        phase_ = Phase::Crouch;
        phaseStart_ = spider->GetBodyPosition();
        PickLandingPoint(context);
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

void BossSpiderAttackLeap::Cancel(const BossAttackContext &context) {
    if (context.spider) {
        // 空中で止められても、脚と高さは立っている状態へ戻す
        context.spider->SetLegTuck(0.0f, pParams_->legFoldTime);
        Vector3 position = context.spider->GetBodyPosition();
        position.y = standHeight_;
        context.spider->SetBodyPosition(position);
    }
    phase_ = Phase::Finished;
}

const char *BossSpiderAttackLeap::GetPhaseName() const {
    switch (phase_) {
    case Phase::Crouch:
        return "沈み込み";
    case Phase::Rise:
        return "飛び上がり";
    case Phase::Fall:
        return "落下";
    case Phase::Impact:
        return "着地";
    case Phase::Recover:
        return "硬直";
    default:
        return "終了";
    }
}
