#include "GameOverPlayerActor.h"
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

void GameOverPlayerActor::Init(const std::string objectName) {
    BaseObject::Init(objectName);
    CreateModel(kSlimeModelPath);
    SetTexture(kWhiteTexturePath);
    SetOffset(Vector3{0.0f, kModelOffsetY, 0.0f});

    // 演出用の置物なので、シーンデータへは残さない
    SetShouldSave(false);

    baseScale_ = transform_->scale_;
    groundPosition_ = transform_->translation_;
    groundPosition_.y = 0.0f;
}

void GameOverPlayerActor::RegisterParams() {
    params_.Register("BaseScale", &baseScale_, {0.01f, 0.05f, 5.0f});
    params_.Register("FlattenDeep", &flattenDeep_, {0.01f, 0.0f, 0.9f});
    params_.Register("FlattenEase", &flattenEase_, {0.01f, 0.0f, 0.9f});
    params_.Register("PuniPeriod", &puniPeriod_, {0.05f, 0.2f, 15.0f});
    params_.Register("PuniJiggle", &puniJiggle_, {0.01f, 0.0f, 0.5f});
    params_.Register("TwitchIntervalMin", &twitchIntervalMin_, {0.1f, 0.2f, 20.0f});
    params_.Register("TwitchIntervalMax", &twitchIntervalMax_, {0.1f, 0.2f, 20.0f});
    params_.Register("TwitchDuration", &twitchDuration_, {0.01f, 0.05f, 2.0f});
    params_.Register("TwitchAmount", &twitchAmount_, {0.01f, 0.0f, 1.0f});
}

void GameOverPlayerActor::Update() {
    const float deltaTime = Frame::DeltaTime();
    puniTime_ += deltaTime;

    // ときどき、思い出したように力なくぴくりと動く。
    // 間隔をばらしておかないと機械仕掛けに見える
    float twitch = 0.0f;
    if (twitchTime_ >= 0.0f) {
        twitchTime_ += deltaTime;
        if (twitchTime_ >= twitchDuration_) {
            twitchTime_ = -1.0f;
        } else {
            // 立ち上がりが速く、すぐ落ちる。跳ねきらずに力尽きる感じにする
            const float ratio = twitchTime_ / (std::max)(0.05f, twitchDuration_);
            twitch = std::sin(ratio * std::numbers::pi_v<float>) * (1.0f - ratio) * twitchAmount_;
        }
    } else {
        twitchTimer_ -= deltaTime;
        if (twitchTimer_ <= 0.0f) {
            const float minInterval = (std::min)(twitchIntervalMin_, twitchIntervalMax_);
            const float maxInterval = (std::max)(twitchIntervalMin_, twitchIntervalMax_);
            std::uniform_real_distribution<float> interval(minInterval, maxInterval);
            twitchTimer_ = interval(random_);
            twitchTime_ = 0.0f;
        }
    }

    // --- 潰れきり ⇔ 少し戻る を行き来する ---
    // 0 で潰れきり・1 でいちばん戻ったところ。両端で速度が0になる曲線なので、
    // ゴムが押し戻されて、また力尽きて潰れる、という往復になる
    const float puniPeriod = (std::max)(0.2f, puniPeriod_);
    const float phase = puniTime_ * 2.0f * std::numbers::pi_v<float> / puniPeriod;
    const float ease = 0.5f - 0.5f * std::cos(phase);
    // 折り返しぎわに小さく揺り返す。これを足すと、ただ伸び縮みするだけの動きが
    // 「ぷにっ」と戻る動きになる
    const float jiggle = std::sin(phase * 2.0f) * puniJiggle_;
    const float release = std::clamp(ease + jiggle + twitch, 0.0f, 1.0f);

    // 潰れた形。横に広がって縦に縮む（ぷにぷにの符号の決まりと同じ向き）
    const float flatten =
        std::clamp(flattenDeep_ + (flattenEase_ - flattenDeep_) * release, 0.0f, 0.9f);
    transform_->scale_ = Vector3{baseScale_.x * (1.0f + flatten), baseScale_.y * (1.0f - flatten),
                                 baseScale_.z * (1.0f + flatten)};

    // ひくつきのぶんだけ、わずかに地面から浮く
    transform_->translation_ = groundPosition_ + Vector3{0.0f, twitch * 0.5f, 0.0f};

    // 傾けない。斜めにすると力尽きたというより転がった見た目になるので、
    // 倒れているのは潰れた形だけで見せる
    transform_->quaternionRotation_ = Quaternion::IdentityQuaternion();

    SetColor(palette_.GetRgba(colorId_));

    BaseObject::Update();
}
