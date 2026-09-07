#include "PlayerStateJump.h"
#include "src/Character/Player/Player.h"
#include "src/Character/Player/Core/PlayerContext.h"
#include "src/Character/Player/Components/Jump/PlayerJumpComponent.h"
#include "src/Character/Player/Components/Move/PlayerMoveComponent.h"
#include "Utility/Debug/Param/GameParamHub.h"
#include "Frame/Frame.h"

void PlayerStateJump::Enter(Player& player, PlayerContext& context) {
	baseScale_ = player.GetWorldTransform()->scale_;
	time_ = 0.0f;
}

void PlayerStateJump::Update(Player& player, PlayerContext& context) {
	// オーナーは params_ が握っているので、ここでは名前と変数だけ渡せばよい。
	// 破棄時の Unregister も params_ が面倒を見る
	params_.Register("Duration", &kDuration);
	params_.Register("Amplitude", &kAmplitude);
	params_.Register("Period", &kPeriod);

	time_ += Hagine::Frame::DeltaTime();
	if (time_ >= kDuration) { time_ = 0.0f; } // ループさせる

	player.GetWorldTransform()->scale_ = context.reactionComponent_->SquashStretch(baseScale_, time_, kDuration, kAmplitude, kPeriod);

	if (!context.jumpComponent_->IsJumping()) {
		context.jumpComponent_->Jump(context, 8.0f);
	}
	context.jumpComponent_->UpdateJump(context);
	context.moveComponent_->Move(context, context.input_.dir, 0.2f);

	if (!context.jumpComponent_->IsJumping()) {
		player.ChangeState("Idle");
	}
}

void PlayerStateJump::Exit(Player& player, PlayerContext& context) {
	player.GetWorldTransform()->scale_ = baseScale_;
}
