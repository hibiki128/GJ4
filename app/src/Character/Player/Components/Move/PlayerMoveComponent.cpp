#include "PlayerMoveComponent.h"
#include "3d/Transform/WorldTransform.h"
#include <MyMath.h>

void PlayerMoveComponent::Move(PlayerContext& context, const Hagine::Vector2& input, float moveSpeed) {
	if (!context.transform_) {
		return;
	}

	// 入力は「画面の上下左右」なので、カメラの向きに合わせてワールドの向きへ回してから進める。
	// 追従カメラが自分の位置を決めるときと同じ回し方をしているので、
	// 奥へ倒せば必ず画面の奥へ、右へ倒せば必ず画面の右へ進む
	const Hagine::Vector3 inputDirection = { input.x, 0.0f, input.y };
	const Hagine::Vector3 moveDirection =
		Hagine::TransformNormal(inputDirection, Hagine::MakeRotateYMatrix(context.cameraYaw_));

	context.transform_->translation_ = context.transform_->translation_ + moveDirection * moveSpeed;
}
