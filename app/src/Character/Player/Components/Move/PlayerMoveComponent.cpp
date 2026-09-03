#include "PlayerMoveComponent.h"
#include "3d/Transform/WorldTransform.h"

void PlayerMoveComponent::Move(PlayerContext& context, const Hagine::Vector2& input, float moveSpeed) {
	if (context.transform_) {
		Hagine::Vector3 translation = context.transform_->translation_;
		translation.x += input.x * moveSpeed;
		translation.z += input.y * moveSpeed;
		context.transform_->translation_ = translation;
	}
}
