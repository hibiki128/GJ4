#include "ClearBossRemains.h"
#include "MyMath.h"
#include "frame/Frame.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <random>

using namespace Hagine;

namespace {

/// <summary>Boss と同じコアのモデル。色をそのまま出すため白テクスチャを貼る</summary>
constexpr const char *kCoreModelPath = "boss/boss.obj";
constexpr const char *kWhiteTexturePath = "debug/white1x1.png";

/// <summary>Boss::Init と同じコアの色（ほとんど黒）</summary>
constexpr Vector4 kCoreColor = {0.16f, 0.16f, 0.20f, 1.0f};

/// <summary>どのボスの殻を散らかすか（見た目をゲーム中とそろえるため同じデータを読む）</summary>
constexpr const char *kBossId = "Boss01";

} // namespace

void ClearBossRemains::Init(const std::string objectName) {
    // 色と殻の見た目はゲーム中のボスと同じものを読む
    palette_.LoadMaster();
    BossParameters parameters{};
    parameters.Load(kBossId);
    palette_.SetUsedColors(parameters.GetUsedColors());
    metaBallParams_ = parameters.MetaBall();

    // --- コア（このオブジェクト自身）---
    BaseObject::Init(objectName);
    CreateModel(kCoreModelPath);
    SetTexture(kWhiteTexturePath);
    SetColor(kCoreColor);
    // 演出用の置物なので、シーンデータへは残さない
    SetShouldSave(false);

    groundPosition_ = transform_->translation_;
    groundPosition_.y = 0.0f;
    ApplyCoreSize();

    // --- 散らばった殻 ---
    // 親を渡さないので、破片はワールド座標にそのまま置かれる。
    // こうしておくとコアがぐらついても地面の破片は動かない
    const int maxBallCount = (std::max)(clumpCount_, 1) * (std::max)(spheresPerClump_, 1);
    debris_.Init(nullptr, palette_, metaBallParams_, maxBallCount);
    ScatterDebris();
}

void ClearBossRemains::RegisterParams() {
    // 大きさ・散らばり方を変えたら撒き直す（塊の数や広がりはここで詰められる）
    GameParamHub::Options scatterOptions{};
    scatterOptions.speed = 0.05f;
    scatterOptions.min = 0.1f;
    scatterOptions.max = 20.0f;
    scatterOptions.onChange = [this] { ScatterDebris(); };

    GameParamHub::Options countOptions{};
    countOptions.speed = 1.0f;
    countOptions.min = 0.0f;
    countOptions.max = 40.0f;
    countOptions.onChange = [this] { ScatterDebris(); };

    GameParamHub::Options coreOptions{};
    coreOptions.speed = 0.05f;
    coreOptions.min = 0.1f;
    coreOptions.max = 10.0f;
    coreOptions.onChange = [this] { ApplyCoreSize(); };

    params_.Register("CoreRadius", &coreRadius_, coreOptions);
    params_.Register("DebrisSphereRadius", &debrisSphereRadius_, scatterOptions);
    params_.Register("ClumpCount", &clumpCount_, countOptions);
    params_.Register("SpheresPerClump", &spheresPerClump_, countOptions);
    params_.Register("ScatterInner", &scatterInner_, scatterOptions);
    params_.Register("ScatterOuter", &scatterOuter_, scatterOptions);
    params_.Register("ClumpSpread", &clumpSpread_, scatterOptions);
    params_.Register("ScatterSeed", &scatterSeed_, {1.0f, 0.0f, 100000000.0f});
    params_.Register("RockAmplitude", &rockAmplitude_, {0.1f, 0.0f, 45.0f});
    params_.Register("RockPeriod", &rockPeriod_, {0.05f, 0.1f, 10.0f});
    params_.Register("SpinSpeed", &spinSpeed_, {0.5f, -180.0f, 180.0f});

    // 保存済みの値が書き戻されても onChange は呼ばれないので、ここで反映し直す
    ApplyCoreSize();
    ScatterDebris();
}

void ClearBossRemains::ApplyCoreSize() {
    coreRadius_ = (std::max)(0.1f, coreRadius_);
    transform_->scale_ = Vector3{coreRadius_, coreRadius_, coreRadius_};
}

void ClearBossRemains::ScatterDebris() {
    builtSeed_ = scatterSeed_;

    const int clumpCount = std::clamp(clumpCount_, 0, 40);
    const int spheresPerClump = std::clamp(spheresPerClump_, 1, 40);
    const float sphereRadius = (std::max)(0.05f, debrisSphereRadius_);
    const float inner = (std::min)(scatterInner_, scatterOuter_);
    const float outer = (std::max)(scatterInner_, scatterOuter_);

    const std::vector<Color> &usedColors = palette_.GetUsedColors();
    if (usedColors.empty()) {
        return;
    }

    // 色ごとの入れ物。使っていない色は空のまま渡して、前に描いた殻を消させる
    std::array<std::vector<Vector3>, kGameColorCount> byColor{};

    // シードを決め打ちにして、起動のたびに散らばり方が変わらないようにする
    std::mt19937 random(static_cast<uint32_t>(scatterSeed_));
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::uniform_real_distribution<float> signed01(-1.0f, 1.0f);

    for (int index = 0; index < clumpCount; ++index) {
        // 塊の中心。角度は等間隔から少しずらし、距離は内外の輪の間へ散らす。
        // 等間隔のままだと時計の文字盤のように並んで人工的に見える
        const float baseAngle = (static_cast<float>(index) / static_cast<float>(clumpCount)) *
                                2.0f * std::numbers::pi_v<float>;
        const float angle = baseAngle + signed01(random) * 0.35f;
        const float distance = inner + (outer - inner) * unit(random);
        const Vector3 clumpCenter = groundPosition_ + Vector3{std::cos(angle) * distance, 0.0f,
                                                              std::sin(angle) * distance};

        // 塊ごとに色を変える。使用色を順に配るので、どの色も必ず地面に残る
        const Color color = usedColors[static_cast<size_t>(index) % usedColors.size()];
        std::vector<Vector3> &positions = byColor[static_cast<size_t>(BossColorPalette::ToIndex(color))];

        for (int sphere = 0; sphere < spheresPerClump; ++sphere) {
            // 粒どうしが影響半径の内側で重なるよう、塊の広がりは球の直径ぶん程度に抑える。
            // 離れすぎると融合せず、粒がばらばらの球に見えてしまう
            const Vector3 offset{signed01(random) * clumpSpread_, 0.0f, signed01(random) * clumpSpread_};
            // 地面に転がっている見た目にしたいので、高さは球の半径ぶんだけ浮かせて
            // 重なったぶんを少しだけ持ち上げる
            const float lift = sphereRadius * (1.0f + unit(random) * 0.35f);
            positions.push_back(clumpCenter + offset + Vector3{0.0f, lift, 0.0f});
        }
    }

    for (int index = 0; index < kGameColorCount; ++index) {
        debris_.SetElements(BossColorPalette::FromIndex(index),
                            std::move(byColor[static_cast<size_t>(index)]), sphereRadius);
    }
}

void ClearBossRemains::Update() {
    const float deltaTime = Frame::DeltaTime();
    rockTime_ += deltaTime;

    // シードだけはドラッグで動かされても onChange が拾えないことがあるので、
    // 値が変わっていたらここで撒き直す
    if (builtSeed_ != scatterSeed_) {
        ScatterDebris();
    }

    // 転がって止まりきらず、まだゆっくり揺れている感じ。
    // 前後と左右で周期をずらすと、一方向の振り子ではなく「ぐらつき」に見える
    const float toRadian = std::numbers::pi_v<float> / 180.0f;
    const float rockPeriod = (std::max)(0.1f, rockPeriod_);
    const float phase = rockTime_ * (2.0f * std::numbers::pi_v<float> / rockPeriod);
    const float rockX = std::sin(phase) * rockAmplitude_ * toRadian;
    const float rockZ = std::sin(phase * 0.73f + 1.1f) * rockAmplitude_ * toRadian;
    const float spinY = rockTime_ * spinSpeed_ * toRadian;

    transform_->translation_ = groundPosition_ + Vector3{0.0f, coreRadius_, 0.0f};
    transform_->quaternionRotation_ = Quaternion::FromAxisAngle(kWorldUp, spinY) *
                                      Quaternion::FromAxisAngle(Vector3{1.0f, 0.0f, 0.0f}, rockX) *
                                      Quaternion::FromAxisAngle(Vector3{0.0f, 0.0f, 1.0f}, rockZ);

    BaseObject::Update();
}

void ClearBossRemains::DispatchShellCompute() {
    debris_.DispatchCompute(Frame::DeltaTime());
}

void ClearBossRemains::DrawDebris(const ViewProjection &viewProjection) {
    debris_.Draw(viewProjection);
}
