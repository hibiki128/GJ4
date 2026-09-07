#include "PlayerStateIdle.h"
#include "src/Character/Player/Player.h"
#include "src/Character/Player/Core/PlayerContext.h"
#include "Math/Easing.h"
#include "Frame/Frame.h"
#include "Utility/Debug/Param/GameParamHub.h"

void PlayerStateIdle::Enter(Player& player, PlayerContext& context) {
	baseScale_ = player.GetWorldTransform()->scale_;
	time_ = 0.0f;
}

void PlayerStateIdle::Update(Player& player, PlayerContext& context) {
	// オーナーは params_ が握っているので、ここでは名前と変数だけ渡せばよい。
	// 破棄時の Unregister も params_ が面倒を見る
	params_.Register("Duration", &kDuration);
	params_.Register("Amplitude", &kAmplitude);
	params_.Register("Period", &kPeriod);

	time_ += Hagine::Frame::DeltaTime();
	if (time_ >= kDuration) { time_ = 0.0f; } // ループさせる
	
	player.GetWorldTransform()->scale_ = context.reactionComponent_->SquashStretch(baseScale_, time_, kDuration, kAmplitude, kPeriod);

	if (context.input_.move) {
		player.ChangeState("Move");
	}

	if (context.input_.jump) {
		player.ChangeState("Jump");
	}
}

void PlayerStateIdle::Exit(Player& player, PlayerContext& context) {
	player.GetWorldTransform()->scale_ = baseScale_;
}
