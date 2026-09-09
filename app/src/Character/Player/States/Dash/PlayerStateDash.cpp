#include "PlayerStateDash.h"
#include "src/Character/Player/Player.h"
#include "src/Character/Player/Core/PlayerContext.h"
#include "src/Character/Player/Components/Move/PlayerMoveComponent.h"
#include "src/Character/Player/Components/Reaction/PlayerComponentReaction.h"
#include "src/Character/Player/Effect/PlayerParticles.h"
#include "Utility/Debug/Param/GameParamHub.h"
#include "Frame/Frame.h"

void PlayerStateDash::RegisterParams() {
	const std::string paramOwnerLabel = "Player/Dash";
	Hagine::GameParamHub* hub = Hagine::GameParamHub::GetInstance();

	hub->Register(paramOwnerLabel, "MoveSpeed", &kMoveSpeed, {0.01f, 0.0f, 10.0f});
	hub->Register(paramOwnerLabel, "Amplitude", &kAmplitude, {0.01f, 0.0f, 1.0f});
	hub->Register(paramOwnerLabel, "Period", &kPeriod, {0.01f, 0.05f, 5.0f});
	hub->Register(paramOwnerLabel, "Sharpness", &kSharpness, {0.01f, 0.0f, 1.0f});
	hub->Register(paramOwnerLabel, "Phase", &kPhase, {0.01f, 0.0f, 1.0f});
}

void PlayerStateDash::Enter(Player& player, PlayerContext& context) {
}

void PlayerStateDash::Update(Player& player, PlayerContext& context) {

	context.moveComponent_->Move(context, context.input_.dir, kMoveSpeed * Hagine::Frame::DeltaTime());

	// ダッシュ中も体は揺れ続ける。スケールを直接書かず、常時の呼吸だけを要求する
	context.reactionComponent_->SetLoop(kAmplitude, kPeriod, kSharpness, kPhase);

	// 足元の粒（移動中と同じもの）。要求を出している間だけ出る
	if (context.transform_) {
		PlayerParticles::GetInstance()->RequestWalk(context.transform_->translation_, player.GetDisplayColor());
	}

	if (!context.input_.dash) {
		player.ChangeState("Move");
	}

	if (context.input_.jump) {
		player.ChangeState("Jump");
	}
}

void PlayerStateDash::Exit(Player& player, PlayerContext& context) {
}
