#include "TitlePlayerActor.h"
#include "MyMath.h"
#include "debug/param/GameParamHub.h"
#include <cmath>

using namespace Hagine;

namespace {

/// <summary>Player と同じスライム。色をそのまま出すため白テクスチャを貼る</summary>
constexpr const char *kSlimeModelPath = "slime/slime.obj";
constexpr const char *kWhiteTexturePath = "debug/white1x1.png";

/// <summary>Player::Init と同じ描画オフセット（足元を地面に合わせるため）</summary>
constexpr float kModelOffsetY = -0.45f;

/// <summary>GameParamHub での出所ラベル</summary>
constexpr const char *kParamOwnerLabel = "Title/Player";

} // namespace

void TitlePlayerActor::Init(const std::string objectName) {
    BaseObject::Init(objectName);
    CreateModel(kSlimeModelPath);
    SetTexture(kWhiteTexturePath);
    SetOffset(Vector3{0.0f, kModelOffsetY, 0.0f});

    // 演出用の置物なので、シーンデータへは残さない
    SetShouldSave(false);

    // 揺れはこの大きさを中心に上下する。以降 scale_ は毎フレーム上書きする
    baseScale_ = transform_->scale_;
    transform_->translation_ = groundPosition_;
    transform_->UpdateMatrix();
    SetColor(palette_.GetRgba(colorId_));
}

void TitlePlayerActor::RegisterParams() {
    GameParamHub *hub = GameParamHub::GetInstance();
    hub->Register(kParamOwnerLabel, "IdleAmplitude", &idleAmplitude_, {0.01f, 0.0f, 1.0f});
    hub->Register(kParamOwnerLabel, "IdlePeriod", &idlePeriod_, {0.01f, 0.05f, 5.0f});
    hub->Register(kParamOwnerLabel, "IdleSharpness", &idleSharpness_, {0.01f, 0.0f, 1.0f});
    hub->Register(kParamOwnerLabel, "IdlePhase", &idlePhase_, {0.01f, 0.0f, 1.0f});
}

void TitlePlayerActor::Update() {
    // ゲーム中の待機ステートと同じ要求の出し方。
    // 揺れの形を決めるのは reaction_ なので、タイトルでも同じ動きになる
    reaction_.SetLoop(idleAmplitude_, idlePeriod_, idleSharpness_, idlePhase_);
    reaction_.Update();

    transform_->translation_ = groundPosition_;
    transform_->scale_ = reaction_.Apply(baseScale_);
    SetColor(palette_.GetRgba(colorId_));

    BaseObject::Update();
}

void TitlePlayerActor::LookAt(const Vector3 &worldPoint) {
    Vector3 toTarget = worldPoint - groundPosition_;
    toTarget.y = 0.0f;
    if (toTarget.LengthSq() <= 0.0001f) {
        return;
    }
    // スライムに正面は無いが、向きを合わせておくと揺れの潰れる向きがそろう
    const float yaw = std::atan2(toTarget.x, toTarget.z);
    transform_->eulerRotation_.y = yaw;
    transform_->quaternionRotation_ = Quaternion::FromAxisAngle(Vector3{0.0f, 1.0f, 0.0f}, yaw);
    transform_->UpdateMatrix();
}
