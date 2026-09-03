#include "BossTestProjectile.h"
#include "src/Boss/Shell/BossSphere.h"
#include "frame/Frame.h"
#include <algorithm>

using namespace Hagine;

void BossTestProjectile::InitProjectile(const std::string &objectName, const Vector4 &rgba, float radius) {
    BaseObject::Init(objectName);
    CreatePrimitiveModel(PrimitiveType::Sphere);

    // 色をそのまま出したいので白テクスチャにする
    SetTexture(kBossTexturePath);

    // 弾はシーンデータに残さない
    SetShouldSave(false);
    SetGizmoSelectable(false);

    transform_->scale_ = Vector3{radius, radius, radius};
    transform_->UpdateMatrix();
    SetColor(rgba);

    isFinished_ = true;
    SetIsModelDraw(false);
}

void BossTestProjectile::Fire(const Vector3 &origin, const Vector3 &direction,
                              float speed, float lifeTime, float correctionRate) {
    transform_->translation_ = origin;
    transform_->UpdateMatrix();

    direction_ = (direction.LengthSq() > 0.0f) ? direction.Normalize() : Vector3{0.0f, 0.0f, 1.0f};
    speed_ = speed;
    lifeTime_ = lifeTime;
    correctionRate_ = correctionRate;
    isFinished_ = false;

    SetIsModelDraw(true);
}

void BossTestProjectile::Update() {
    if (isFinished_) {
        return;
    }

    const float deltaTime = Frame::DeltaTime();

    // --- ソフトロックオンの軌道補正（対象へ向きを寄せる）---
    if (targetPositionGetter_) {
        Vector3 targetPosition{};
        if (targetPositionGetter_(targetPosition)) {
            const Vector3 toTarget = targetPosition - transform_->translation_;
            if (toTarget.LengthSq() > 0.0001f) {
                const Vector3 desired = toTarget.Normalize();
                const float rate = std::clamp(correctionRate_ * deltaTime, 0.0f, 1.0f);
                const Vector3 blended = direction_ + (desired - direction_) * rate;
                if (blended.LengthSq() > 0.0001f) {
                    direction_ = blended.Normalize();
                }
            }
        } else {
            // 対象が消えた・見失った場合はまっすぐ飛ぶ
            targetPositionGetter_ = nullptr;
        }
    }

    // --- 移動 ---
    const Vector3 previousPosition = transform_->translation_;
    transform_->translation_ += direction_ * (speed_ * deltaTime);
    transform_->UpdateMatrix();

    // --- 着弾判定は「動いた線分」で行う（速い弾でもすり抜けない）---
    if (hitTester_ && hitTester_(previousPosition, transform_->translation_)) {
        Finish();
        return;
    }

    // --- 寿命 ---
    lifeTime_ -= deltaTime;
    if (lifeTime_ <= 0.0f) {
        Finish();
    }
}

void BossTestProjectile::Finish() {
    isFinished_ = true;
    SetIsModelDraw(false);
}
