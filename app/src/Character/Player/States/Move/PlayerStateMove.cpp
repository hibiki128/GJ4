#include "PlayerStateMove.h"
#include "src/Character/Player/Player.h"
#include "src/Character/Player/Core/PlayerContext.h"
#include "src/Character/Player/Components/Move/PlayerMoveComponent.h"
#include "src/Character/Player/Components/Reaction/PlayerComponentReaction.h"
#include "Utility/Debug/Param/GameParamHub.h"

void PlayerStateMove::RegisterParams() {
	const std::string paramOwnerLabel = "Player/Move";
	Hagine::GameParamHub* hub = Hagine::GameParamHub::GetInstance();

	hub->Register(paramOwnerLabel, "Amplitude", &kAmplitude, {0.01f, 0.0f, 1.0f});
	hub->Register(paramOwnerLabel, "Period", &kPeriod, {0.01f, 0.05f, 5.0f});
	hub->Register(paramOwnerLabel, "Sharpness", &kSharpness, {0.01f, 0.0f, 1.0f});
	hub->Register(paramOwnerLabel, "Phase", &kPhase, {0.01f, 0.0f, 1.0f});
}

void PlayerStateMove::Enter(Player& player, PlayerContext& context) {
}

void PlayerStateMove::Update(Player& player, PlayerContext& context) {
	context.moveComponent_->Move(context, context.input_.dir, 0.2f);

	// スケールを直接書かず、常時の呼吸だけを要求する
	context.reactionComponent_->SetLoop(kAmplitude, kPeriod, kSharpness, kPhase);

	if (!context.input_.move) {
		player.ChangeState("Idle");
		return;
	}

	if (context.input_.dash) {
		player.ChangeState("Dodge");
		return;
	}

	if (context.input_.jump) {
		player.ChangeState("Jump");
		return;
	}
}

void PlayerStateMove::Exit(Player& player, PlayerContext& context) {
}
