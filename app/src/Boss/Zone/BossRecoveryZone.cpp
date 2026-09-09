#include "BossRecoveryZone.h"
#include "src/Boss/Data/BossEasing.h"
#include "src/Boss/Effect/BossParticles.h"
#include "src/Interface/IAmmoRecoverySink.h"
#include "src/Interface/ITargetLocator.h"
#include "Easing.h"
#include "MyMath.h"
#include "Random.h"
#include "camera/projection/ViewProjection.h"
#include <algorithm>
#include <cmath>
#include <numbers>

using namespace Hagine;

namespace {

/// <summary>色をそのまま出すための白テクスチャ（モデル側に材質が無いため）</summary>
constexpr const char *kWhiteTexturePath = "debug/white1x1.png";

/// <summary>大きさ0の行列は潰れて法線が壊れるので、ごく小さい値で止める</summary>
constexpr float kMinScale = 0.001f;

/// <summary>外枠は濃く、内側の塗りは薄く。塗りだけを脈打たせて「効いている」感じを出す</summary>
constexpr float kRingAlpha = 0.9f;
constexpr float kFillAlpha = 0.28f;

/// <summary>乗っているあいだは塗りを濃くして、効いていることを分かるようにする</summary>
constexpr float kOccupiedFillAlpha = 0.5f;

} // namespace

void BossRecoveryZone::Init(const std::string &namePrefix) {
    if (ring_) {
        return;
    }

    // 警告表示と同じ2枚の円盤を使い回す。どちらも半径1のXZ平面なので、
    // スケールをそのまま半径として扱える
    ring_ = std::make_unique<BaseObject>();
    ring_->Init(namePrefix + "Ring");
    ring_->CreateModel("boss/effect/warningOutLine.obj");
    ring_->SetShouldSave(false);
    ring_->SetGizmoSelectable(false);
    ring_->SetTexture(kWhiteTexturePath);
    // 地面に寝ているので、光の当たり方で暗くならないよう陰影を切って色をそのまま出す
    ring_->GetLighting() = false;
    ring_->SetIsAlive(false);
    ring_->SetIsModelDraw(false);

    fill_ = std::make_unique<BaseObject>();
    fill_->Init(namePrefix + "Fill");
    fill_->CreateModel("boss/effect/warningFill.obj");
    fill_->SetShouldSave(false);
    fill_->SetGizmoSelectable(false);
    fill_->SetTexture(kWhiteTexturePath);
    fill_->GetLighting() = false;
    fill_->SetIsAlive(false);
    fill_->SetIsModelDraw(false);
}

void BossRecoveryZone::Spawn(const Vector3 &from, const Vector3 &landing, Color color,
                             const BossColorPalette &palette, const BossRecoveryZoneParams &params) {
    if (!ring_) {
        return;
    }

    color_ = color;
    rgba_ = palette.GetRgba(color);
    spawnFrom_ = from;
    center_ = Vector3{landing.x, 0.0f, landing.z};

    phase_ = Phase::Pop;
    timer_ = 0.0f;
    age_ = 0.0f;
    pulsePhase_ = 0.0f;
    auraTimer_ = 0.0f;
    auraAngle_ = 0.0f;
    isOccupied_ = false;

    ring_->SetColor(Vector4{rgba_.x, rgba_.y, rgba_.z, kRingAlpha});
    fill_->SetColor(Vector4{rgba_.x, rgba_.y, rgba_.z, kFillAlpha});

    // 飛んでいるあいだは輪を出さない。着地して初めて開く
    SetVisible(false);
    Place(0.0f, params);

    // ボスから弾け出た瞬間
    BossParticles *particles = BossParticles::GetInstance();
    particles->SetNextColor(BossParticles::Id::ZonePop, rgba_);
    particles->Burst(BossParticles::Id::ZonePop, spawnFrom_);
}

void BossRecoveryZone::Update(float deltaTime, const BossRecoveryZoneParams &params,
                              const ITargetLocator *target, IAmmoRecoverySink *sink) {
    if (phase_ == Phase::Idle) {
        return;
    }

    timer_ += deltaTime;
    age_ += deltaTime;
    pulsePhase_ += deltaTime * params.fillPulseSpeed;
    isOccupied_ = false;

    BossParticles *particles = BossParticles::GetInstance();

    switch (phase_) {
    case Phase::Pop: {
        // ボスから放物線を描いて落ちる。粒だけが飛ぶので、
        // 「敵から何かが弾け出て、あそこへ落ちた」と読める
        const float duration = (std::max)(0.01f, params.popTime);
        const float progress = std::clamp(timer_ / duration, 0.0f, 1.0f);
        Vector3 position = Lerp(spawnFrom_, center_, progress);
        // 4x(1-x) は 0 と 1 で 0、真ん中で 1。弧の高さがそのまま頂点になる
        position.y += params.popArcHeight * 4.0f * progress * (1.0f - progress);

        particles->SetNextColor(BossParticles::Id::ZonePop, rgba_);
        particles->Burst(BossParticles::Id::ZonePop, position);

        if (progress >= 1.0f) {
            phase_ = Phase::Open;
            timer_ = 0.0f;
            SetVisible(true);
            // 着地の弾け
            particles->SetNextColor(BossParticles::Id::ZonePop, rgba_);
            particles->Burst(BossParticles::Id::ZonePop, center_);
        }
        break;
    }
    case Phase::Open: {
        // 0から一気に開く。行き過ぎてから戻ると「ぽんと開いた」感じになる
        const float duration = (std::max)(0.01f, params.openTime);
        const float progress = std::clamp(timer_ / duration, 0.0f, 1.0f);
        Place(ApplyEasing(EasingType::OutBack, 0.0f, 1.0f, progress, 1.0f), params);
        if (progress >= 1.0f) {
            phase_ = Phase::Active;
            timer_ = 0.0f;
        }
        break;
    }
    case Phase::Active: {
        Place(1.0f, params);
        UpdateAura(deltaTime, params);

        // 乗っているあいだ、この色の回復だけを早める。
        // 毎フレーム要求するので、離れれば何もしなくても元へ戻る
        if (target && target->IsTargetValid() && sink) {
            const Vector3 targetPosition = target->GetTargetPosition();
            const float dx = targetPosition.x - center_.x;
            const float dz = targetPosition.z - center_.z;
            const float reach = params.radius + target->GetTargetRadius();
            if (dx * dx + dz * dz <= reach * reach) {
                sink->RequestAmmoRegen(color_, params.regenScale);
                isOccupied_ = true;
            }
        }

        // 乗っている相手を満タンにし切ったら役目は終わり。
        // 「乗っている」を条件に入れないと、相手がもともと満タンのときに
        // 開いた瞬間そのまま閉じてしまい、開いている時間の設定が効かなくなる
        if (params.closeWhenFull && isOccupied_ && sink && sink->IsAmmoFull(color_)) {
            BeginClose(params);
            break;
        }
        if (timer_ >= params.activeTime) {
            BeginClose(params);
        }
        break;
    }
    case Phase::Close: {
        const float duration = (std::max)(0.01f, params.closeTime);
        const float progress = std::clamp(timer_ / duration, 0.0f, 1.0f);
        // すぼみながら薄くなる
        Place(1.0f - SmoothInOut(progress), params);
        const float fade = 1.0f - progress;
        ring_->SetColor(Vector4{rgba_.x, rgba_.y, rgba_.z, kRingAlpha * fade});
        fill_->SetColor(Vector4{rgba_.x, rgba_.y, rgba_.z, kFillAlpha * fade});
        if (progress >= 1.0f) {
            Clear();
        }
        break;
    }
    default:
        break;
    }
}

void BossRecoveryZone::BeginClose(const BossRecoveryZoneParams &params) {
    if (phase_ == Phase::Idle || phase_ == Phase::Close) {
        return;
    }
    (void)params;
    phase_ = Phase::Close;
    timer_ = 0.0f;

    // 消えるときにもうひと弾け。役目を終えたことが分かる
    BossParticles *particles = BossParticles::GetInstance();
    particles->SetNextColor(BossParticles::Id::ZonePop, rgba_);
    particles->Burst(BossParticles::Id::ZonePop, center_);
}

void BossRecoveryZone::Draw(const ViewProjection &viewProjection) {
    if (phase_ == Phase::Idle || !ring_) {
        return;
    }
    ring_->Draw(viewProjection);
    fill_->Draw(viewProjection);
}

void BossRecoveryZone::Clear() {
    phase_ = Phase::Idle;
    timer_ = 0.0f;
    age_ = 0.0f;
    isOccupied_ = false;
    SetVisible(false);
}

void BossRecoveryZone::Place(float openRatio, const BossRecoveryZoneParams &params) {
    if (!ring_) {
        return;
    }

    const float ratio = (std::max)(0.0f, openRatio);
    const float radius = (std::max)(kMinScale, params.radius * ratio);

    ring_->GetWorldTransform()->translation_ = Vector3{center_.x, params.ringHeight, center_.z};
    ring_->GetWorldTransform()->scale_ = Vector3{radius, 1.0f, radius};
    ring_->GetWorldTransform()->UpdateMatrix();

    // 内側の塗りだけを脈打たせる。外枠は範囲を示すので大きさを変えない
    const float pulse = 1.0f + std::sin(pulsePhase_) * params.fillPulseAmount;
    const float fillRadius = (std::max)(kMinScale, radius * pulse);
    fill_->GetWorldTransform()->translation_ = Vector3{center_.x, params.fillHeight, center_.z};
    fill_->GetWorldTransform()->scale_ = Vector3{fillRadius, 1.0f, fillRadius};
    fill_->GetWorldTransform()->UpdateMatrix();

    // 乗っているあいだは濃くする（効いているかが足元で分かる）
    if (phase_ == Phase::Active) {
        const float alpha = isOccupied_ ? kOccupiedFillAlpha : kFillAlpha;
        fill_->SetColor(Vector4{rgba_.x, rgba_.y, rgba_.z, alpha});
    }
}

void BossRecoveryZone::UpdateAura(float deltaTime, const BossRecoveryZoneParams &params) {
    auraTimer_ += deltaTime;
    const float interval = (std::max)(0.01f, params.auraInterval);
    if (auraTimer_ < interval) {
        return;
    }
    auraTimer_ = 0.0f;

    // 湯気は面のあちこちから立つので、円の内側全体へ散らす。
    // 黄金角ぶん回すと、少ない数でも角度が偏らずに埋まっていく
    constexpr float kGoldenAngle = 2.39996323f;
    auraAngle_ += kGoldenAngle;

    // 半径は毎回引き直す。そのままの乱数だと中心へ寄って見えるので、
    // 平方根を取って面積あたりで一様になるようにする
    const float radius = params.radius * std::sqrt(Random::Range(0.0f, 1.0f));
    const Vector3 position{center_.x + std::cos(auraAngle_) * radius, params.ringHeight,
                           center_.z + std::sin(auraAngle_) * radius};

    BossParticles *particles = BossParticles::GetInstance();
    particles->SetNextColor(BossParticles::Id::ZoneAura, rgba_);
    particles->Burst(BossParticles::Id::ZoneAura, position);
}

void BossRecoveryZone::SetVisible(bool visible) {
    if (!ring_) {
        return;
    }
    ring_->SetIsAlive(visible);
    ring_->SetIsModelDraw(visible);
    fill_->SetIsAlive(visible);
    fill_->SetIsModelDraw(visible);
}

const char *BossRecoveryZone::GetPhaseName() const {
    switch (phase_) {
    case Phase::Pop:
        return "飛び出し中";
    case Phase::Open:
        return "開いている途中";
    case Phase::Active:
        return "回復中";
    case Phase::Close:
        return "閉じている途中";
    default:
        return "なし";
    }
}
