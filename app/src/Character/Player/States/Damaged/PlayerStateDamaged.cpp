#include "PlayerStateDamaged.h"
#include "Frame/Frame.h"
#include "Utility/Debug/Param/GameParamHub.h"
#include "src/Character/Player/Components/Health/PlayerHealthComponent.h"
#include "src/Character/Player/Components/Jump/PlayerJumpComponent.h"
#include "src/Character/Player/Components/Reaction/PlayerComponentReaction.h"
#include "src/Character/Player/Core/PlayerContext.h"
#include "src/Character/Player/Player.h"

void PlayerStateDamaged::RegisterParams() {
	const std::string paramOwnerLabel = "Player/Damaged";
	Hagine::GameParamHub* hub = Hagine::GameParamHub::GetInstance();

	hub->Register(paramOwnerLabel, "StunDuration", &kStunDuration, {0.01f, 0.0f, 3.0f});
	hub->Register(paramOwnerLabel, "SquashStrength", &kSquashStrength, {0.01f, 0.0f, 1.0f});
}

void PlayerStateDamaged::Enter(Player& player, PlayerContext& context) {
	elapsed_ = 0.0f;

	// 被弾の手応え。スケールは直接書かず、reactionComponent_ へ要求を出すだけにする
	// （合成と適用は Player::Update が1か所で行う）
	context.reactionComponent_->PlayLanding(kSquashStrength);

	// ここへ足していく演出:
	//   ・被弾方向へのノックバック（context.healthComponent_->GetLastHitPoint() から向きを出す）
	//   ・無敵中の点滅（PlayerColorComponent へ「点滅させる」要求を足して、色の書き手は1か所のまま保つ）
	//   ・振動やヒットストップ（GameSettings::RequestVibration など）
}

void PlayerStateDamaged::Update(Player& player, PlayerContext& context) {
	elapsed_ += Hagine::Frame::DeltaTime();

	// 空中で被弾しても落下は進める。着地の演出は出さずに捨てる
	// （被弾の潰れを着地の潰れで上書きしないため）
	context.jumpComponent_->UpdateJump(context);
	context.jumpComponent_->ConsumeLanded();

	// 倒れたときはこのステートに留める。ダウン演出やゲームオーバーへの導線を
	// 足すならここから分岐させる（シーン側は Player::IsDead() を見ればよい）
	if (context.healthComponent_ && context.healthComponent_->IsDead()) {
		return;
	}

	if (elapsed_ < kStunDuration) {
		return;
	}

	player.ChangeState("Idle");
}

void PlayerStateDamaged::Exit(Player& player, PlayerContext& context) {
}
