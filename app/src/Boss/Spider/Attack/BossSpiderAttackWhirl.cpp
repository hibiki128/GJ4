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
        // その場で回るだけ。歩かないので、位置は動かさない。
        // 等速で回して急に止めると機械のように見えるので、回りながら少しずつ遅くする。
        // 遅くなりきったところが「そろそろ隙ができる」の合図になる
        const float progress = std::clamp(timer_ / (std::max)(0.01f, pParams_->spinTime), 0.0f, 1.0f);
        const float endSpeed = pParams_->spinSpeed * std::clamp(pParams_->spinEndSpeedRatio, 0.0f, 1.0f);
        const float speed = Lerp(pParams_->spinSpeed, endSpeed, SmoothInOut(progress));
        spider->AddBodyYaw(speed * (std::numbers::pi_v<float> / 180.0f) * context.deltaTime);

        Vector3 position = spider->GetBodyPosition();
        position.y = spinBodyHeight;
        spider->SetBodyPosition(position);

        // 真横に伸び切った脚の先が届く範囲がそのまま攻撃範囲
        reachRadius_ = spider->GetFootReach();
        spider->ReportHit(spider->GetBodyPosition(), reachRadius_, pParams_->damage);

        if (timer_ >= pParams_->spinTime) {
            // 立ち上がる前に、広げ切った脚のまま止まって隙をさらす。
            // 脚を戻すのはこのあと（Stagger → Recover）
            phase_ = Phase::Stagger;
            timer_ = 0.0f;
            spider->BeginStagger(pParams_->staggerTime);
        }
        break;
    }
    case Phase::Stagger: {
        // 回り終わりで目が回っている。脚は真横に伸びたまま、胴も低いままで静止する。
        // 当たり判定はもう出していないので、近づいて球を撃ち込める
        Vector3 position = spider->GetBodyPosition();
        position.y = spinBodyHeight;
        spider->SetBodyPosition(position);

        if (timer_ >= pParams_->staggerTime) {
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
        // 隙はこの攻撃が作っているものなので、中断したらそこで終わりにする
        context.spider->ClearStagger();
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
    case Phase::Stagger:
        return "脚を広げたまま静止";
    case Phase::Recover:
        return "脚を戻して立ち上がる";
    default:
        return "終了";
    }
}
