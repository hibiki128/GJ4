#include "PlayerStateIdle.h"
#include "src/Character/Player/Player.h"
#include "src/Character/Player/Core/PlayerContext.h"
#include "src/Character/Player/Components/Reaction/PlayerComponentReaction.h"
#include "Utility/Debug/Param/GameParamHub.h"

void PlayerStateIdle::RegisterParams() {
	const std::string paramOwnerLabel = "Player/Idle";
	Hagine::GameParamHub* hub = Hagine::GameParamHub::GetInstance();

	hub->Register(paramOwnerLabel, "Amplitude", &kAmplitude, {0.01f, 0.0f, 1.0f});
	hub->Register(paramOwnerLabel, "Period", &kPeriod, {0.01f, 0.05f, 5.0f});
	hub->Register(paramOwnerLabel, "Sharpness", &kSharpness, {0.01f, 0.0f, 1.0f});
	hub->Register(paramOwnerLabel, "Phase", &kPhase, {0.01f, 0.0f, 1.0f});
}

void PlayerStateIdle::Enter(Player& player, PlayerContext& context) {
}

void PlayerStateIdle::Update(Player& player, PlayerContext& context) {
	// スケールを直接書かず、常時の呼吸だけを要求する。
	// 実際の合成と適用は Player::Update が reactionComponent_ 経由で行う
	context.reactionComponent_->SetLoop(kAmplitude, kPeriod, kSharpness, kPhase);

	if (context.input_.move) {
		player.ChangeState("Move");
		return;
	}

	if (context.input_.jump) {
		player.ChangeState("Jump");
		return;
	}
}

void PlayerStateIdle::Exit(Player& player, PlayerContext& context) {
}
