#include "PlayerStateDamaged.h"
#include "Frame/Frame.h"
#include "Utility/Debug/Param/GameParamHub.h"
#include "src/Character/Player/Components/Health/PlayerHealthComponent.h"
#include "src/Character/Player/Components/Jump/PlayerJumpComponent.h"
#include "src/Character/Player/Components/Reaction/PlayerComponentReaction.h"
#include "src/Character/Player/Core/PlayerContext.h"
#include "src/Character/Player/Player.h"
#include <algorithm>

void PlayerStateDamaged::RegisterParams() {
	const std::string paramOwnerLabel = "Player/Damaged";
	Hagine::GameParamHub* hub = Hagine::GameParamHub::GetInstance();

	hub->Register(paramOwnerLabel, "StunDuration", &kStunDuration, {0.01f, 0.0f, 3.0f});
	hub->Register(paramOwnerLabel, "SquashStrength", &kSquashStrength, {0.01f, 0.0f, 1.0f});
	hub->Register(paramOwnerLabel, "KnockbackDistance", &kKnockbackDistance, {0.05f, 0.0f, 10.0f});
	hub->Register(paramOwnerLabel, "KnockbackTime", &kKnockbackTime, {0.01f, 0.01f, 1.0f});
}

void PlayerStateDamaged::Enter(Player& player, PlayerContext& context) {
	elapsed_ = 0.0f;
	knockbackMoved_ = 0.0f;
	knockbackDirection_ = CalcKnockbackDirection(context);

	// 被弾の手応え。スケールは直接書かず、reactionComponent_ へ要求を出すだけにする
	// （合成と適用は Player::Update が1か所で行う）
	context.reactionComponent_->PlayLanding(kSquashStrength);

	// ここへ足していく演出:
	//   ・無敵中の点滅（PlayerColorComponent へ「点滅させる」要求を足して、色の書き手は1か所のまま保つ）
	//   ・ヒットストップ
}

void PlayerStateDamaged::Update(Player& player, PlayerContext& context) {
	elapsed_ += Hagine::Frame::DeltaTime();

	UpdateKnockback(context);

	// 空中で被弾しても落下は進める。着地の演出は出さずに捨てる
	// （被弾の潰れを着地の潰れで上書きしないため）
	context.jumpComponent_->UpdateJump(context);
	context.jumpComponent_->ConsumeLanded();

	// 倒れたらやられステートへ渡す。震えてはじけるところまではあちらの仕事で、
	// ゲームオーバーへ送る間合いはシーンが Player::IsDefeatFinished() を見て決める
	if (context.healthComponent_ && context.healthComponent_->IsDead()) {
		player.ChangeState("Defeated");
		return;
	}

	if (elapsed_ < kStunDuration) {
		return;
	}

	player.ChangeState("Idle");
}

void PlayerStateDamaged::Exit(Player& player, PlayerContext& context) {
}

Hagine::Vector3 PlayerStateDamaged::CalcKnockbackDirection(const PlayerContext& context) const {
	// 当たった位置から見て反対側へ下がる。高さは無視して水平にだけ押される
	Hagine::Vector3 away{};
	if (context.healthComponent_ && context.transform_) {
		away = context.transform_->translation_ - context.healthComponent_->GetLastHitPoint();
		away.y = 0.0f;
	}

	if (away.LengthSq() > 0.0001f) {
		return away.Normalize();
	}

	// 当たった位置が真上・真下で水平の向きが出ないときは、見ている向きの後ろへ下がる
	Hagine::Vector3 back = {-context.aimDirection_.x, 0.0f, -context.aimDirection_.z};
	if (back.LengthSq() <= 0.0001f) {
		return Hagine::Vector3{0.0f, 0.0f, -1.0f};
	}
	return back.Normalize();
}

void PlayerStateDamaged::UpdateKnockback(PlayerContext& context) {
	if (!context.transform_ || knockbackMoved_ >= kKnockbackDistance) {
		return;
	}

	const float t = std::clamp(elapsed_ / (std::max)(0.0001f, kKnockbackTime), 0.0f, 1.0f);
	// 押され始めが一番速く、止まるときはゆっくり
	const float eased = 1.0f - (1.0f - t) * (1.0f - t);
	const float moved = kKnockbackDistance * eased;

	// 進んだぶんの差だけ動かすので、フレームレートが変わっても押される総量は変わらない
	context.transform_->translation_ =
		context.transform_->translation_ + knockbackDirection_ * (moved - knockbackMoved_);
	knockbackMoved_ = moved;
}
