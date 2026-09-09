#include "PlayerStateMove.h"
#include "src/Character/Player/Player.h"
#include "src/Character/Player/Core/PlayerContext.h"
#include "src/Character/Player/Components/Move/PlayerMoveComponent.h"
#include "src/Character/Player/Components/Reaction/PlayerComponentReaction.h"
#include "src/Character/Player/Effect/PlayerParticles.h"
#include "Utility/Debug/Param/GameParamHub.h"
#include "Frame/Frame.h"

void PlayerStateMove::RegisterParams() {
	const std::string paramOwnerLabel = "Player/Move";
	Hagine::GameParamHub* hub = Hagine::GameParamHub::GetInstance();

	hub->Register(paramOwnerLabel, "Amplitude", &kAmplitude, {0.01f, 0.0f, 1.0f});
	hub->Register(paramOwnerLabel, "Period", &kPeriod, {0.01f, 0.05f, 5.0f});
	hub->Register(paramOwnerLabel, "Sharpness", &kSharpness, {0.01f, 0.0f, 1.0f});
	hub->Register(paramOwnerLabel, "Phase", &kPhase, {0.01f, 0.0f, 1.0f});
	hub->Register(paramOwnerLabel, "MoveSpeed", &kMoveSpeed, {0.01f, 0.0f, 10.0f});
}

void PlayerStateMove::Enter(Player& player, PlayerContext& context) {
}

void PlayerStateMove::Update(Player& player, PlayerContext& context) {
	context.moveComponent_->Move(context, context.input_.dir, kMoveSpeed * Hagine::Frame::DeltaTime());

	// 足元の粒。要求を出している間だけ出るので、止まれば自然に消える
	if (context.transform_) {
		PlayerParticles::GetInstance()->RequestWalk(context.transform_->translation_, player.GetDisplayColor());
	}

	// スケールを直接書かず、常時の呼吸だけを要求する
	context.reactionComponent_->SetLoop(kAmplitude, kPeriod, kSharpness, kPhase);

	if (!context.input_.move) {
		player.ChangeState("Idle");
		return;
	}

	// 回避は押した瞬間だけ、しかもクールタイムが明けているときだけ出せる
	if (context.input_.dodge && context.dodgeCooldown_ <= 0.0f) {
		player.ChangeState("Dodge");
		return;
	}

	// クールタイム中に押しっぱなしにされたときは、回避を挟まずダッシュへ入る。
	// こうしておくと「押している間は走る」がクールタイムに左右されない
	if (context.input_.dash) {
		player.ChangeState("Dash");
		return;
	}

	if (context.input_.jump) {
		player.ChangeState("Jump");
		return;
	}
}

void PlayerStateMove::Exit(Player& player, PlayerContext& context) {
}
