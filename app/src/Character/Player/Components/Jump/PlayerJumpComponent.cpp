#include "PlayerJumpComponent.h"

void PlayerJumpComponent::Jump(PlayerContext& context, float jumpForce) {
	if (!context.rigidBody_) {
		return;
	}
	// 上向きの初速を与えるだけ。落下は BaseObject のリジッドボディ（重力）が計算する
	context.rigidBody_->velocity.y = jumpForce;
	isJumping_ = true;
}

void PlayerJumpComponent::UpdateJump(PlayerContext& context) {
	if (!isJumping_ || !context.rigidBody_) {
		return;
	}
	// 落下に転じたあとに床へ触れたら着地とする（接地は床コライダーとの衝突で決まる）
	if (context.isOnGround_ && context.rigidBody_->velocity.y <= 0.0f) {
		isJumping_ = false;
	}
}
