#include "PlayerBullet.h"
#include "Frame/Frame.h"

void PlayerBullet::Init(const std::string objectName) {
	BaseObject::Init(objectName);
	CreatePrimitiveModel(Hagine::PrimitiveType::Sphere);

	// 弾はシーンのjsonに保存しない / ギズモの選択対象にもしない
	SetShouldSave(false);
	SetGizmoSelectable(false);

	// 生成直後は待機状態にしておく
	Deactivate();
}

void PlayerBullet::Fire(const Hagine::Vector3& position, const Hagine::Vector3& direction, float speed) {
	transform_->translation_ = position;
	velocity_ = direction.Normalize() * speed;
	lifeTime_ = kLifeTime;

	isActive_ = true;
	SetIsModelDraw(true);
}

void PlayerBullet::Update() {
	// 待機中の弾は動かさない（マネージャーには登録されたままなので毎フレーム呼ばれる）
	if (!isActive_) {
		return;
	}

	transform_->translation_ += velocity_ * Hagine::Frame::DeltaTime();

	lifeTime_ -= Hagine::Frame::DeltaTime();
	if (lifeTime_ <= 0.0f) {
		Deactivate();
	}

	BaseObject::Update();
}

void PlayerBullet::Draw(const Hagine::ViewProjection& viewProjection) {
	BaseObject::Draw(viewProjection);
}

void PlayerBullet::Deactivate() {
	isActive_ = false;
	velocity_ = {0.0f, 0.0f, 0.0f};

	// 破棄はせず、描画だけ止めて次の発射まで待つ
	SetIsModelDraw(false);
}
