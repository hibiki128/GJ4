#include "BossTestProjectile.h"
#include "frame/Frame.h"
#include <algorithm>

using namespace Hagine;

void BossTestProjectile::InitProjectile(const std::string &objectName, const Vector4 &rgba,
                                        float radius, const std::string &tag, const std::string &collisionMask) {
    BaseObject::Init(objectName);
    CreatePrimitiveModel(PrimitiveType::Sphere);

    // 弾はシーンデータに残さない
    SetShouldSave(false);
    SetGizmoSelectable(false);

    transform_->scale_ = Vector3{radius, radius, radius};
    transform_->UpdateMatrix();
    SetColor(rgba);

    collider_ = AddSphereCollider(objectName + "_Hit");
    collider_->SetTag(tag);
    collider_->AddCollisionMask(collisionMask);
    collider_->SetRadius(radius);
    collider_->SetOnCollisionEnter([this](ColliderBase *other) {
        if (isFinished_) {
            return; // 1発で複数パーツを巻き込まない
        }
        if (hitHandler_) {
            hitHandler_(other);
        }
        Finish();
    });
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

    // 使い終わった弾を再利用する場合に備え、描画と当たり判定を戻す
    SetIsModelDraw(true);
    if (collider_) {
        collider_->SetEnabled(true);
    }
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
            // 対象が壊れた・見失った場合はまっすぐ飛ぶ
            targetPositionGetter_ = nullptr;
        }
    }

    // --- 移動 ---
    transform_->translation_ += direction_ * (speed_ * deltaTime);
    transform_->UpdateMatrix();

    // --- 寿命 ---
    lifeTime_ -= deltaTime;
    if (lifeTime_ <= 0.0f) {
        Finish();
    }
}

void BossTestProjectile::Finish() {
    isFinished_ = true;
    SetIsModelDraw(false);
    if (collider_) {
        // 実体の破棄は所有者（BossTestDriver）が衝突判定の外で行う
        collider_->SetEnabled(false);
    }
}
