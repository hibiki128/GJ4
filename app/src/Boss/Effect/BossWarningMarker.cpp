#include "BossWarningMarker.h"
#include <algorithm>

using namespace Hagine;

namespace {

/// <summary>地面と重ならないよう、ほんの少しだけ浮かせる高さ</summary>
constexpr float kGroundOffset = 0.02f;

/// <summary>塗りは外枠のわずかに下に置く（同じ高さだと面が取り合いになる）</summary>
constexpr float kFillOffset = 0.01f;

/// <summary>大きさ0の行列は潰れて法線が壊れるので、ごく小さい値で止める</summary>
constexpr float kMinScale = 0.001f;

/// <summary>色をそのまま出すための白テクスチャ（モデル側に材質が無いため）</summary>
constexpr const char *kWhiteTexturePath = "debug/white1x1.png";

/// <summary>攻撃範囲の外枠の色</summary>
constexpr Vector4 kOutlineColor{1.0f, 0.12f, 0.10f, 1.0f};

/// <summary>命中タイミングを示す塗りの色</summary>
constexpr Vector4 kFillColor{1.0f, 0.12f, 0.10f, 1.0f};

} // namespace

void BossWarningMarker::Ensure(const std::string &namePrefix) {
    if (outline_) {
        return;
    }

    outline_ = std::make_unique<BaseObject>();
    outline_->Init(namePrefix + "WarnOutline");
    outline_->CreateModel("boss/effect/warningOutLine.obj");
    outline_->SetShouldSave(false);
    outline_->SetGizmoSelectable(false);
    // 地面に寝ているので光の当たり方で暗くならないよう、陰影を切って赤をそのまま出す
    outline_->SetTexture(kWhiteTexturePath);
    outline_->SetColor(kOutlineColor);
    outline_->GetLighting() = false;
    outline_->SetIsAlive(false);
    outline_->SetIsModelDraw(false);

    fill_ = std::make_unique<BaseObject>();
    fill_->Init(namePrefix + "WarnFill");
    fill_->CreateModel("boss/effect/warningFill.obj");
    fill_->SetShouldSave(false);
    fill_->SetGizmoSelectable(false);
    fill_->SetTexture(kWhiteTexturePath);
    fill_->SetColor(kFillColor);
    fill_->GetLighting() = false;
    fill_->SetIsAlive(false);
    fill_->SetIsModelDraw(false);
}

void BossWarningMarker::Show(const Vector3 &center, float radius) {
    if (!outline_) {
        return;
    }
    center_ = center;
    radius_ = (std::max)(kMinScale, radius);
    isVisible_ = true;

    // 平板なので、そのまま地面へ寝かせて置くだけでよい
    outline_->GetWorldTransform()->translation_ = Vector3{center_.x, kGroundOffset, center_.z};
    outline_->GetWorldTransform()->scale_ = Vector3{radius_, 1.0f, radius_};
    outline_->GetWorldTransform()->UpdateMatrix();
    outline_->SetIsAlive(true);
    outline_->SetIsModelDraw(true);

    fill_->GetWorldTransform()->translation_ = Vector3{center_.x, kFillOffset, center_.z};
    fill_->SetIsAlive(true);
    fill_->SetIsModelDraw(true);
    SetFillRatio(0.0f);
}

void BossWarningMarker::SetFillRatio(float ratio) {
    if (!fill_ || !isVisible_) {
        return;
    }
    const float scale = (std::max)(kMinScale, radius_ * std::clamp(ratio, 0.0f, 1.0f));
    fill_->GetWorldTransform()->translation_ = Vector3{center_.x, kFillOffset, center_.z};
    fill_->GetWorldTransform()->scale_ = Vector3{scale, 1.0f, scale};
    fill_->GetWorldTransform()->UpdateMatrix();
}

void BossWarningMarker::Hide() {
    isVisible_ = false;
    if (!outline_) {
        return;
    }
    outline_->SetIsAlive(false);
    outline_->SetIsModelDraw(false);
    fill_->SetIsAlive(false);
    fill_->SetIsModelDraw(false);
}

void BossWarningMarker::Draw(const ViewProjection &viewProjection) {
    if (!isVisible_ || !outline_) {
        return;
    }
    outline_->Draw(viewProjection);
    fill_->Draw(viewProjection);
}
