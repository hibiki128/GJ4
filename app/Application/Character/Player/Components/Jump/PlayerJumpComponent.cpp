#include "PlayerJumpComponent.h"

void PlayerJumpComponent::Jump(PlayerContext& context, float jumpForce) {
	Hagine::Vector3 translate = context.transform_->translation_;
	velocity_ = jumpForce;
	context.transform_->translation_ = translate;
	isJumping_ = true;
}

void PlayerJumpComponent::UpdateJump(PlayerContext& context) {
	Hagine::Vector3 translate = context.transform_->translation_;
	translate.y += velocity_;
	context.transform_->translation_ = translate;
	velocity_ -= 0.1f;
	if (translate.y <= 0.0f) {
		translate.y = 0.0f;
		isJumping_ = false;
	}
}
