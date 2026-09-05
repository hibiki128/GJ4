#include "BossSpiderAttackWhirl.h"
#include "MyMath.h"
#include "src/Boss/Data/BossEasing.h"
#include "src/Boss/Spider/BossSpider.h"
#include "src/Interface/ITargetLocator.h"
#include <algorithm>
#include <cmath>
#include <numbers>

using namespace Hagine;

void BossSpiderAttackWhirl::Start(const BossAttackContext &context) {
    if (!context.spider) {
        phase_ = Phase::Finished;
        return;
    }
    standHeight_ = context.spider->GetStandHeight();
    phase_ = Phase::Telegraph;
    timer_ = 0.0f;
    reachRadius_ = context.spider->GetFootReach();
    // 予備動作のあいだに、脚を真横へ伸ばし切る
    context.spider->SetLegBend(0.0f, (std::max)(0.01f, pParams_->telegraphTime));
}

void BossSpiderAttackWhirl::Update(const BossAttackContext &context) {
    if (!context.spider || phase_ == Phase::Finished) {
        phase_ = Phase::Finished;
        return;
    }
    BossSpider *spider = context.spider;
    timer_ += context.deltaTime;

    // 脚が回る高さ。胴が地面へ潜らないところで止める
    const float spinBodyHeight =
        (std::max)(spider->GetParameters().bodyRadius,
                   pParams_->spinHeight - spider->GetParameters().bodyRadius * 0.35f);

    switch (phase_) {
    case Phase::Telegraph: {
        // ゆっくり脚を真横へ伸ばし切り、地面近くまで胴を下げる。
        // ここが遅いほど、相手は「外へ逃げる」判断をする余裕ができる
        const float progress = std::clamp(timer_ / (std::max)(0.01f, pParams_->telegraphTime), 0.0f, 1.0f);
        const float eased = SmoothInOut(progress);

        Vector3 position = spider->GetBodyPosition();
        position.y = Lerp(standHeight_, spinBodyHeight, eased);
        spider->SetBodyPosition(position);

        // まだ回らずに相手を向いておく（何が来るか読ませる）
        if (context.target && context.target->IsTargetValid()) {
            spider->FaceTowards(context.target->GetTargetPosition());
        }
        if (progress >= 1.0f) {
            phase_ = Phase::Spin;
            timer_ = 0.0f;
        }
        break;
    }
    case Phase::Spin: {
        // その場で回るだけ。歩かないので、位置は動かさない
        spider->AddBodyYaw(pParams_->spinSpeed * (std::numbers::pi_v<float> / 180.0f) * context.deltaTime);

        Vector3 position = spider->GetBodyPosition();
        position.y = spinBodyHeight;
        spider->SetBodyPosition(position);

        // 真横に伸び切った脚の先が届く範囲がそのまま攻撃範囲
        reachRadius_ = spider->GetFootReach();
        spider->ReportHit(spider->GetBodyPosition(), reachRadius_, pParams_->damage);

        if (timer_ >= pParams_->spinTime) {
            phase_ = Phase::Recover;
            timer_ = 0.0f;
            spider->SetLegBend(1.0f, pParams_->recoverTime); // 脚を通常の姿勢へ戻す
        }
        break;
    }
    case Phase::Recover: {
        // 高さを戻す（脚は SetLegBend が戻している最中）
        const float progress = std::clamp(timer_ / (std::max)(0.01f, pParams_->recoverTime), 0.0f, 1.0f);
        Vector3 position = spider->GetBodyPosition();
        position.y = Lerp(spinBodyHeight, standHeight_, SmoothInOut(progress));
        spider->SetBodyPosition(position);

        if (progress >= 1.0f) {
            phase_ = Phase::Finished;
        }
        break;
    }
    default:
        break;
    }
}

void BossSpiderAttackWhirl::Cancel(const BossAttackContext &context) {
    if (context.spider) {
        context.spider->SetLegBend(1.0f, 0.2f);
        Vector3 position = context.spider->GetBodyPosition();
        position.y = standHeight_;
        context.spider->SetBodyPosition(position);
    }
    phase_ = Phase::Finished;
}

const char *BossSpiderAttackWhirl::GetPhaseName() const {
    switch (phase_) {
    case Phase::Telegraph:
        return "脚を広げる";
    case Phase::Spin:
        return "その場で回転";
    case Phase::Recover:
        return "脚を戻す";
    default:
        return "終了";
    }
}
