#include "ClearPlayerActor.h"
#include "MyMath.h"
#include "frame/Frame.h"
#include <algorithm>
#include <cmath>
#include <numbers>

using namespace Hagine;

namespace {

/// <summary>Player と同じスライム。色をそのまま出すため白テクスチャを貼る</summary>
constexpr const char *kSlimeModelPath = "slime/slime.obj";
constexpr const char *kWhiteTexturePath = "debug/white1x1.png";

/// <summary>Player::Init と同じ描画オフセット（足元を地面に合わせるため）</summary>
constexpr float kModelOffsetY = -0.45f;

} // namespace

void ClearPlayerActor::Init(const std::string objectName) {
    BaseObject::Init(objectName);
    CreateModel(kSlimeModelPath);
    SetTexture(kWhiteTexturePath);
    SetOffset(Vector3{0.0f, kModelOffsetY, 0.0f});

    // 演出用の置物なので、シーンデータへは残さない
    SetShouldSave(false);

    baseScale_ = transform_->scale_;
    homePosition_ = transform_->translation_;
    hopTime_ = 0.0f;
}

void ClearPlayerActor::RegisterParams() {
    params_.Register("BaseScale", &baseScale_, {0.01f, 0.05f, 5.0f});
    params_.Register("HopHeight", &hopHeight_, {0.01f, 0.0f, 5.0f});
    params_.Register("HopDuration", &hopDuration_, {0.01f, 0.05f, 3.0f});
    params_.Register("HopInterval", &hopInterval_, {0.01f, 0.0f, 3.0f});
    params_.Register("LandStrength", &landStrength_, {0.01f, 0.0f, 1.0f});
    params_.Register("SpinPerHop", &spinPerHop_, {1.0f, -360.0f, 360.0f});
    params_.Register("IdleLoopAmp", &idleLoopAmp_, {0.005f, 0.0f, 0.5f});
    params_.Register("IdleLoopPeriod", &idleLoopPeriod_, {0.01f, 0.1f, 3.0f});
}

void ClearPlayerActor::Update() {
    const float deltaTime = Frame::DeltaTime();

    UpdateHop(deltaTime);

    // 跳ねていない間もゆっくり呼吸させる。SetLoop は毎フレーム出す要求なので、
    // 呼ばなくなれば振幅は勝手に0へ収束する（PlayerComponentReaction の作り）
    reaction_.SetLoop(idleLoopAmp_, idleLoopPeriod_, 0.0f, 0.0f);
    reaction_.Update();
    transform_->scale_ = reaction_.Apply(baseScale_);

    // 色はボスと同じマスタから引く（ゲーム中のプレイヤーと同じ赤に見せる）
    SetColor(palette_.GetRgba(colorId_));

    BaseObject::Update();
}

void ClearPlayerActor::UpdateHop(float deltaTime) {
    const float cycle = (std::max)(0.05f, hopDuration_) + (std::max)(0.0f, hopInterval_);
    hopTime_ += deltaTime;

    // ひと跳ねぶんを撃ち終えたら、着地のぷにっを入れて次の跳ねへ回す。
    // 高さの式が0へ戻るのと同じ瞬間に鳴らすので、潰れる瞬間と接地が必ず合う
    if (hopTime_ >= cycle) {
        hopTime_ -= cycle;
        reaction_.PlayLanding(landStrength_);
        faceYaw_ += spinPerHop_ * (std::numbers::pi_v<float> / 180.0f);
    }

    // 跳ねている間だけ放物線で持ち上げる（間（インターバル）は地面で待つ）
    float height = 0.0f;
    if (hopTime_ < hopDuration_) {
        const float t = hopTime_ / (std::max)(0.05f, hopDuration_);
        height = hopHeight_ * 4.0f * t * (1.0f - t); // t=0,1 で0・t=0.5 で最大
    }

    transform_->translation_ = homePosition_ + Vector3{0.0f, height, 0.0f};
    transform_->quaternionRotation_ = Quaternion::FromAxisAngle(kWorldUp, faceYaw_);
}
