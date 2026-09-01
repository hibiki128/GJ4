#include "Player.h"

void Player::Init(const std::string objectName) {
	BaseObject::Init(objectName);
	CreatePrimitiveModel(Hagine::PrimitiveType::Cube);
}

void Player::Update() {
	if (inputContext_.move) {
		Hagine::Vector3 translate = transform_->translation_;
		translate.x += inputContext_.dir.x * 0.1f;
		translate.z += inputContext_.dir.y * 0.1f;
		transform_->translation_ = translate;
	}

	if (inputContext_.jump) {
		Hagine::Vector3 translate = transform_->translation_;
		translate.y += 0.1f;
		transform_->translation_ = translate;
		isJumping_ = true;
	}

	if (!inputContext_.jump && isJumping_) {
		Hagine::Vector3 translate = transform_->translation_;
		translate.y -= 0.1f;
		transform_->translation_ = translate;
		if (translate.y <= 0.0f) {
			translate.y = 0.0f;
			isJumping_ = false;
		}
	}

	if (inputContext_.attack) {
		// 攻撃処理
	}

	if (inputContext_.dash) {
		Hagine::Vector3 translate = transform_->translation_;
		translate.x += inputContext_.dir.x * 0.5f;
		translate.z += inputContext_.dir.y * 0.5f;
		transform_->translation_ = translate;
	}

	BaseObject::Update();
}

void Player::Draw(const Hagine::ViewProjection& viewProjection) {
	BaseObject::Draw(viewProjection);
}
