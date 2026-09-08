#include "PlayerBullet.h"
#include "Frame/Frame.h"
#include <algorithm>

namespace {
// 色をそのまま出したいので白テクスチャを貼る（色は SetColor 側で決める）
constexpr const char* kBulletTexturePath = "debug/white1x1.png";
} // namespace

void PlayerBullet::Init(const std::string objectName) {
	BaseObject::Init(objectName);
	CreatePrimitiveModel(Hagine::PrimitiveType::Sphere);
	SetTexture(kBulletTexturePath);

	// 弾はシーンのjsonに保存しない / ギズモの選択対象にもしない
	SetShouldSave(false);
	SetGizmoSelectable(false);

	// 生成直後は待機状態にしておく
	Deactivate();
}

void PlayerBullet::Fire(const Shot& shot) {
	transform_->translation_ = shot.position;
	transform_->scale_ = Hagine::Vector3{shot.radius, shot.radius, shot.radius};

	direction_ = (shot.direction.LengthSq() > 0.0f) ? shot.direction.Normalize()
	                                                : Hagine::Vector3{0.0f, 0.0f, 1.0f};
	speed_ = shot.speed;
	lifeTime_ = shot.lifeTime;
	correctionRate_ = shot.correctionRate;
	targetPositionGetter_ = shot.targetPositionGetter;
	hitTester_ = shot.hitTester;

	SetColor(shot.rgba);

	isActive_ = true;
	SetIsModelDraw(true);
}

void PlayerBullet::Update() {
	// 待機中の弾は動かさない（マネージャーには登録されたままなので毎フレーム呼ばれる）
	if (!isActive_) {
		return;
	}

	const float deltaTime = Hagine::Frame::DeltaTime();

	ApplyTrajectoryCorrection(deltaTime);

	const Hagine::Vector3 previousPosition = transform_->translation_;
	transform_->translation_ += direction_ * (speed_ * deltaTime);

	// 着弾判定は「動いた線分」で行う（速い弾でもすり抜けない）
	if (hitTester_ && hitTester_(previousPosition, transform_->translation_)) {
		Deactivate();
		return;
	}

	lifeTime_ -= deltaTime;
	if (lifeTime_ <= 0.0f) {
		Deactivate();
		return;
	}

	BaseObject::Update();
}

void PlayerBullet::ApplyTrajectoryCorrection(float deltaTime) {
	if (!targetPositionGetter_) {
		return;
	}

	Hagine::Vector3 targetPosition{};
	if (!targetPositionGetter_(targetPosition)) {
		// 対象が消えた・見失った場合はまっすぐ飛ぶ
		targetPositionGetter_ = nullptr;
		return;
	}

	const Hagine::Vector3 toTarget = targetPosition - transform_->translation_;
	if (toTarget.LengthSq() <= 0.0001f) {
		return;
	}

	const Hagine::Vector3 desired = toTarget.Normalize();
	const float rate = std::clamp(correctionRate_ * deltaTime, 0.0f, 1.0f);
	const Hagine::Vector3 blended = direction_ + (desired - direction_) * rate;
	if (blended.LengthSq() > 0.0001f) {
		direction_ = blended.Normalize();
	}
}

void PlayerBullet::Draw(const Hagine::ViewProjection& viewProjection) {
	BaseObject::Draw(viewProjection);
}

void PlayerBullet::Deactivate() {
	isActive_ = false;
	speed_ = 0.0f;

	// 撃った相手を掴んだままにしない（相手が消えても弾が握り続けてしまう）
	targetPositionGetter_ = nullptr;
	hitTester_ = nullptr;

	// 破棄はせず、描画だけ止めて次の発射まで待つ
	SetIsModelDraw(false);
}
