#include "PlayerStateJump.h"
#include "src/Character/Player/Player.h"
#include "src/Character/Player/Core/PlayerContext.h"
#include "src/Character/Player/Components/Jump/PlayerJumpComponent.h"
#include "src/Character/Player/Components/Move/PlayerMoveComponent.h"
#include "src/Character/Player/Components/Reaction/PlayerComponentReaction.h"
#include "src/Character/Player/Effect/PlayerParticles.h"
#include "Utility/Debug/Param/GameParamHub.h"

void PlayerStateJump::RegisterParams() {
	const std::string paramOwnerLabel = "Player/Jump";
	Hagine::GameParamHub* hub = Hagine::GameParamHub::GetInstance();

	hub->Register(paramOwnerLabel, "JumpForce", &kJumpForce, {0.1f, 0.0f, 30.0f});
	hub->Register(paramOwnerLabel, "RefSpeed", &kRefSpeed, {0.1f, 0.1f, 30.0f});
	hub->Register(paramOwnerLabel, "RiseStretch", &kRiseStretch, {0.01f, 0.0f, 1.0f});
	hub->Register(paramOwnerLabel, "FallStretch", &kFallStretch, {0.01f, 0.0f, 1.0f});
}

void PlayerStateJump::Enter(Player& player, PlayerContext& context) {
	// 踏み切りはここで済ませる。Update の頭で跳ぶと、最初の1フレームだけ
	// 跳ぶ前の速度で空中ストレッチを計算してしまう
	context.jumpComponent_->Jump(context, kJumpForce);
	// 予備動作（縮んでから伸びる）。オフのときは中で何もしないので呼びっぱなしでよい
	context.reactionComponent_->PlayTakeoff();
}

void PlayerStateJump::Update(Player& player, PlayerContext& context) {
	// 空中の形は時間ではなく上下の速度で決める。
	// 頂点付近では自然に 0 に近づくので、丸くなって「タメ」が出る
	const float vy = context.rigidBody_->velocity.y;
	const float t = std::clamp(std::fabsf(vy) / kRefSpeed, 0.0f, 1.0f);
	const float stretch = (vy > 0.0f) ? kRiseStretch : kFallStretch;
	context.reactionComponent_->SetAirStretch(stretch * t); // 正の値 = 縦に伸びて細い

	context.jumpComponent_->UpdateJump(context);
	context.moveComponent_->Move(context, context.input_.dir, 0.2f);

	if (context.jumpComponent_->ConsumeLanded()) {
		// 高いところから落ちたときほど大きく潰れる
		const float strength = std::clamp(context.jumpComponent_->GetLandingSpeed() / kRefSpeed, 0.0f, 1.0f);
		context.reactionComponent_->PlayLanding(strength);
		// 足元から上へ跳ね上がる粒。潰れ具合と同じ強さを渡すので、高いところから落ちたときほど大きく飛ぶ
		if (context.transform_) {
			PlayerParticles::GetInstance()->BurstLanding(
				context.transform_->translation_, strength, player.GetDisplayColor());
		}
		// 演出は reactionComponent_ が持ち越すので、その場で Idle に戻してよい
		player.ChangeState("Idle");
		return;
	}

	// 着地フラグを取り逃してもステートに閉じ込められないようにする保険
	if (!context.jumpComponent_->IsJumping()) {
		player.ChangeState("Idle");
		return;
	}
}

void PlayerStateJump::Exit(Player& player, PlayerContext& context) {
}
