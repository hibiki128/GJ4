#include "BossSpiderAttackLeap.h"
#include "MyMath.h"
#include "Random.h"
#include "src/Boss/Data/BossEasing.h"
#include "camera/projection/ViewProjection.h"
#include "src/Boss/Effect/BossParticles.h"
#include "src/Boss/Spider/BossSpider.h"
#include "src/Interface/ITargetLocator.h"
#include <algorithm>
#include <cmath>
#include <numbers>

using namespace Hagine;

namespace {

/// <summary>着地の沈み込みに使う割合（残りは立ち上がりに使う）</summary>
constexpr float kAbsorbRatio = 0.35f;

} // namespace

void BossSpiderAttackLeap::Start(const BossAttackContext &context) {
    if (!context.spider) {
        phase_ = Phase::Finished;
        return;
    }
    standHeight_ = context.spider->GetStandHeight();
    hopIndex_ = 0;
    timer_ = 0.0f;
    marker_.Ensure("BossSpiderLeap");
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

    // 落ちてくる場所と範囲を地面に出す。塗りは着地に向けてここから広がる
    hopElapsed_ = 0.0f;
    marker_.Show(landingPoint_, pParams_->impactRadius);
}

void BossSpiderAttackLeap::Update(const BossAttackContext &context) {
    if (!context.spider || phase_ == Phase::Finished) {
        phase_ = Phase::Finished;
        return;
    }
    BossSpider *spider = context.spider;
    timer_ += context.deltaTime;

    // 着地予告の塗りを進める。外枠に追いついた瞬間が着地＝当たる瞬間
    if (phase_ == Phase::Crouch || phase_ == Phase::Rise || phase_ == Phase::Fall) {
        hopElapsed_ += context.deltaTime;
        marker_.SetFillRatio(CalcFillRatio(hopElapsed_));
    }

    switch (phase_) {
    case Phase::Crouch: {
        // 助走のように胴だけ沈める。足は地面に着いたまま＝脚が縮んで溜めて見える
        const float progress = std::clamp(timer_ / (std::max)(0.01f, pParams_->crouchTime), 0.0f, 1.0f);
        Vector3 position = phaseStart_;
        position.y = standHeight_ - pParams_->crouchDepth * SmoothInOut(progress);
        spider->SetBodyPosition(position);
        spider->FaceTowards(landingPoint_);
        if (progress >= 1.0f) {
            // 踏み切りで足元の土を蹴り上げる
            BossParticles::GetInstance()->BurstOnGround(BossParticles::Id::JumpDust, phaseStart_);
            phase_ = Phase::Rise;
            timer_ = 0.0f;
            phaseStart_ = spider->GetBodyPosition();
            // 浮いているあいだ脚を畳む。一気に切り替えると足がワープするので時間をかける
            spider->SetLegTuck(pParams_->legTuck, pParams_->legFoldTime);
        }
        break;
    }
    case Phase::Rise: {
        // 着地点の真上まで飛び上がる。
        // 水平はなめらかに寄せ、上下は「蹴り出しが最も速く、頂点で止まる」形にする。
        // 沈み込みの底では速度が0なので、そこから一気に伸び上がる＝屈伸の反動になる
        const float progress = std::clamp(timer_ / (std::max)(0.01f, pParams_->riseTime), 0.0f, 1.0f);
        Vector3 position = Lerp(phaseStart_, apexPosition_, SmoothInOut(progress));
        position.y = ApplyEasing(EasingType::OutQuad, phaseStart_.y, apexPosition_.y, progress, 1.0f);
        spider->SetBodyPosition(position);
        if (progress >= 1.0f) {
            phase_ = Phase::Fall;
            timer_ = 0.0f;
            phaseStart_ = apexPosition_;
            // 落ちながら脚を伸ばし、接地する前に着地姿勢を作り終える。
            // 着地してから戻すと、脚だけ遅れて動いて「ぬるっと」見える
            spider->SetLegTuck(0.0f, (std::max)(0.01f, pParams_->fallTime * 0.9f));
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
            // 着地。踏み潰した土煙を出し、予告の塗りが外枠に追いついたところなので消す
            BossParticles::GetInstance()->BurstOnGround(BossParticles::Id::LandDust, landingPoint_);
            marker_.Hide();
            spider->ReportHit(Vector3{landingPoint_.x, 0.0f, landingPoint_.z},
                              pParams_->impactRadius, pParams_->damage);
        }
        break;
    }
    case Phase::Impact: {
        // 着地の衝撃を殺すように、いったん沈んでから押し返して立ち上がる。
        // 足は地面に着いたままなので、胴が下がったぶんだけ脚が畳まれる（人の屈伸と同じ）
        const float duration = (std::max)(0.01f, pParams_->impactTime);
        const float progress = std::clamp(timer_ / duration, 0.0f, 1.0f);
        float dip = 0.0f;
        if (progress < kAbsorbRatio) {
            // 沈み込み: 落下の勢いがそのまま体を沈めるので、入りが速い
            dip = ApplyEasing(EasingType::OutQuad, 0.0f, 1.0f, progress / kAbsorbRatio, 1.0f);
        } else {
            // 押し返し: 沈み切ってからゆっくり立ち上がる
            dip = 1.0f - SmoothInOut((progress - kAbsorbRatio) / (1.0f - kAbsorbRatio));
        }
        spider->SetBodyPosition(Vector3{landingPoint_.x,
                                        standHeight_ - pParams_->landAbsorbDepth * dip,
                                        landingPoint_.z});

        if (progress < 1.0f) {
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
    marker_.Hide();
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

float BossSpiderAttackLeap::CalcFillRatio(float elapsed) const {
    // 沈み込み〜飛び上がり〜落下を1本の時間として見て、着地でちょうど1になるようにする
    const float total =
        (std::max)(0.01f, pParams_->crouchTime + pParams_->riseTime + pParams_->fallTime);
    return (std::min)(elapsed / total, 1.0f);
}

void BossSpiderAttackLeap::Draw(const ViewProjection &viewProjection) {
    marker_.Draw(viewProjection);
}
