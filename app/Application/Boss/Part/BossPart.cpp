#include "BossPart.h"

using namespace Hagine;

void BossPart::InitPart(const std::string &objectName, const BossPartDesc &desc,
                        float radius, float partScale, float thickness) {
    index_ = desc.index;
    localDirection_ = desc.localDirection.Normalize();

    BaseObject::Init(objectName);

    // CreatePrimitiveModel は内部で JSON を読み直してトランスフォームを上書きするため、
    // 位置・回転・スケールの設定は必ずこの後に行う
    CreatePrimitiveModel(PrimitiveType::Sphere);

    // 色をそのまま出したいので、既定の uvChecker から白テクスチャへ差し替える
    SetTexture(kBossTexturePath);

    // 保存対象から外す（シーン保存でパーツ数ぶんのJSONが増えるのを防ぐ）
    SetShouldSave(false);
    SetGizmoSelectable(false);

    // 球面上へ配置し、+Y が外向きになるよう回転させてから法線方向に潰す
    transform_->translation_ = localDirection_ * radius;
    Quaternion faceOutward{};
    faceOutward.SetFromTo(kWorldUp, localDirection_);
    transform_->quaternionRotation_ = faceOutward;
    transform_->scale_ = Vector3{partScale, partScale * thickness, partScale};
    transform_->UpdateMatrix();
}

void BossPart::SetupCollider(const std::string &tag, const std::string &collisionMask, float radiusScale) {
    collider_ = AddSphereCollider(objectName_ + "_Hit");
    collider_->SetTag(tag);
    collider_->AddCollisionMask(collisionMask);
    // パーツは接線方向の大きさで当たるようにする（潰した厚みは判定に使わない）
    collider_->SetRadius(transform_->scale_.x * radiusScale);
}

void BossPart::ApplyLayout(float radius, float partScale, float thickness, float colliderScale) {
    transform_->translation_ = localDirection_ * radius;
    transform_->scale_ = Vector3{partScale, partScale * thickness, partScale};
    transform_->UpdateMatrix();

    if (collider_) {
        collider_->SetRadius(partScale * colliderScale);
    }
}

void BossPart::SetPartColor(Color color, const Vector4 &rgba) {
    color_ = color;
    baseRgba_ = rgba;
    SetColor(isHighlighted_ ? Lerp(baseRgba_, Vector4{1.0f, 1.0f, 1.0f, 1.0f}, 0.5f) : baseRgba_);
}

void BossPart::Break() {
    if (!GetIsAlive()) {
        return;
    }
    SetIsAlive(false);
    SetIsModelDraw(false);
    if (collider_) {
        // 破棄せず無効化するだけ。衝突判定の走査中に呼ばれても安全
        collider_->SetEnabled(false);
    }
}

void BossPart::Restore() {
    SetIsAlive(true);
    SetIsModelDraw(true);
    if (collider_) {
        collider_->SetEnabled(true);
    }
    SetHighlight(false);
}

void BossPart::SetHighlight(bool highlight) {
    if (isHighlighted_ == highlight) {
        return;
    }
    isHighlighted_ = highlight;
    SetColor(highlight ? Lerp(baseRgba_, Vector4{1.0f, 1.0f, 1.0f, 1.0f}, 0.5f) : baseRgba_);
}

Vector3 BossPart::GetWorldNormal() {
    // ローカルの外向き（+Y）をワールドへ回す
    return GetWorldRotation().Rotate(kWorldUp).Normalize();
}
