#include "BossSphere.h"

using namespace Hagine;

void BossSphere::InitSphere(const std::string &objectName, float radius) {
    BaseObject::Init(objectName);

    // CreatePrimitiveModel は内部で JSON を読み直してトランスフォームを上書きするため、
    // 大きさや色の設定は必ずこの後に行う
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
    transform_->UpdateMatrix();

    SetColor(baseRgba_);
    SetIsAlive(true);
    SetIsModelDraw(true);
}

void BossSphere::Deactivate() {
    SetIsAlive(false);
    SetIsModelDraw(false);
    isHighlighted_ = false;
}

void BossSphere::SetSphereRadius(float radius) {
    // 格子は「半径 radius の球が接する」前提なので、必ず等方スケールにする
    transform_->scale_ = Vector3{radius, radius, radius};
    transform_->UpdateMatrix();
}

void BossSphere::SetLocalPosition(const Vector3 &localPosition) {
    localPosition_ = localPosition;
    transform_->translation_ = localPosition;
    transform_->UpdateMatrix();
}

void BossSphere::SetHighlight(bool highlight) {
    if (isHighlighted_ == highlight) {
        return;
    }
    isHighlighted_ = highlight;
    SetColor(highlight ? Lerp(baseRgba_, Vector4{1.0f, 1.0f, 1.0f, 1.0f}, 0.5f) : baseRgba_);
}

Vector3 BossSphere::GetWorldNormal() {
    // 中心から球へ向かう向きが、そのまま殻の外向きになる
    if (localPosition_.LengthSq() <= 0.0001f) {
        return kWorldUp;
    }
    return GetWorldRotation().Rotate(localPosition_.Normalize()).Normalize();
}
