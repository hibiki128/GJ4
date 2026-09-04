#include "BossSphere.h"
#include "Easing.h"
#include <algorithm>

using namespace Hagine;

void BossSphere::InitSphere(const std::string &objectName, float radius) {
    BaseObject::Init(objectName);

    // CreatePrimitiveModel は内部で JSON を読み直してトランスフォームを上書きするため、
    // 大きさや色の設定は必ずこの後に行う。
    // このモデルは殻の描画には使わない（融合メッシュが描く）。ロックオン強調の表示用
    CreatePrimitiveModel(PrimitiveType::Sphere);

    // 色をそのまま出したいので、既定の uvChecker から白テクスチャへ差し替える
    SetTexture(kBossTexturePath);

    // 殻の球はシーンデータに残さない（数が多く、実行時に増減するため）
    SetShouldSave(false);
    SetGizmoSelectable(false);

    SetSphereRadius(radius);
    Deactivate();
}

void BossSphere::Place(const ShellCell &cell, const Vector3 &localPosition,
                       Color color, const Vector4 &rgba) {
    cell_ = cell;
    localPosition_ = localPosition;
    color_ = color;
    baseRgba_ = rgba;
    isHighlighted_ = false;

    transform_->translation_ = localPosition;
    transform_->scale_ = Vector3{radius_, radius_, radius_};
    transform_->UpdateMatrix();

    SetColor(baseRgba_);
    SetIsAlive(true);
    // 殻の見た目は BossShellMetaBall が色ごとにまとめて描くので、球そのものは出さない
    SetIsModelDraw(false);
}

void BossSphere::Deactivate() {
    SetIsAlive(false);
    SetIsModelDraw(false);
    isHighlighted_ = false;
}

void BossSphere::SetSphereRadius(float radius) {
    radius_ = radius;
    // 格子は「半径 radius の球が接する」前提なので、必ず等方スケールにする
    transform_->scale_ = Vector3{radius, radius, radius};
    transform_->UpdateMatrix();
}

void BossSphere::SetLocalPosition(const Vector3 &localPosition) {
    localPosition_ = localPosition;
    transform_->translation_ = localPosition;
    transform_->UpdateMatrix();
}

void BossSphere::SetHighlight(bool highlight, float scale) {
    if (isHighlighted_ == highlight) {
        return;
    }
    isHighlighted_ = highlight;

    // 融合メッシュは色を1つしか持てないので、強調中だけ実体の球を重ねて出す。
    // 融合面はおよそ球の半径ぶん膨らむため、少し大きくしないと埋もれて見えない
    const float drawScale = highlight ? radius_ * (std::max)(scale, 1.0f) : radius_;
    transform_->scale_ = Vector3{drawScale, drawScale, drawScale};
    transform_->UpdateMatrix();

    SetIsModelDraw(highlight);
    SetColor(highlight ? Lerp(baseRgba_, Vector4{1.0f, 1.0f, 1.0f, 1.0f}, 0.5f) : baseRgba_);
}

void BossSphere::BeginAttach(const Vector3 &fromLocal, float duration, float startScale) {
    motion_ = Motion::Attach;
    motionTime_ = 0.0f;
    motionDuration_ = (std::max)(0.01f, duration);
    motionFrom_ = fromLocal;
    motionStartScale_ = std::clamp(startScale, 0.01f, 1.0f);
}

void BossSphere::BeginVanish(float duration, float drift, float delay) {
    motion_ = Motion::Vanish;
    // 遅れているあいだは負の時間で待たせる（塊が順に消えていくように見せる）
    motionTime_ = -(std::max)(0.0f, delay);
    motionDuration_ = (std::max)(0.01f, duration);
    motionFrom_ = localPosition_;
    motionDrift_ = drift;
}

bool BossSphere::UpdateMotion(float deltaTime, float fullRadius) {
    if (motion_ == Motion::None) {
        return false;
    }

    motionTime_ += deltaTime;
    if (motionTime_ < 0.0f) {
        return true; // 消え始めるまで待機中
    }

    const float progress = std::clamp(motionTime_ / motionDuration_, 0.0f, 1.0f);

    if (motion_ == Motion::Attach) {
        // 着弾点から定位置へ吸い寄せられ、少し行き過ぎてから収まる
        transform_->translation_ = ApplyEasing(EasingType::OutCubic, motionFrom_, localPosition_, progress, 1.0f);
        transform_->scale_ = Vector3{1.0f, 1.0f, 1.0f} *
                             ApplyEasing(EasingType::OutBack, fullRadius * motionStartScale_, fullRadius, progress, 1.0f);
    } else {
        // 外へ押し出されながら、少し膨らんでから縮んで消える
        Vector3 outward = motionFrom_;
        outward = (outward.LengthSq() > 0.0001f) ? outward.Normalize() : kWorldUp;
        transform_->translation_ =
            motionFrom_ + outward * ApplyEasing(EasingType::OutCubic, 0.0f, motionDrift_, progress, 1.0f);

        // 完全な0スケールは行列が潰れるので、ごく小さい値で止めてから消す
        const float radius = (std::max)(fullRadius * 0.001f,
                                        ApplyEasing(EasingType::InBack, fullRadius, 0.0f, progress, 1.0f));
        transform_->scale_ = Vector3{radius, radius, radius};
    }
    transform_->UpdateMatrix();

    if (progress >= 1.0f) {
        motion_ = Motion::None;
        return false;
    }
    return true;
}

void BossSphere::ClearMotion(float fullRadius) {
    motion_ = Motion::None;
    motionTime_ = 0.0f;
    transform_->translation_ = localPosition_;
    transform_->scale_ = Vector3{fullRadius, fullRadius, fullRadius};
    transform_->UpdateMatrix();
}

Vector3 BossSphere::GetWorldNormal() {
    // 中心から球へ向かう向きが、そのまま殻の外向きになる
    if (localPosition_.LengthSq() <= 0.0001f) {
        return kWorldUp;
    }
    return GetWorldRotation().Rotate(localPosition_.Normalize()).Normalize();
}
