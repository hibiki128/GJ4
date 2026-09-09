#include "PlayerStateDefeated.h"
#include "Frame/Frame.h"
#include "Utility/Debug/Param/GameParamHub.h"
#include "src/Character/Player/Components/Jump/PlayerJumpComponent.h"
#include "src/Character/Player/Components/Reaction/PlayerComponentReaction.h"
#include "src/Character/Player/Core/PlayerContext.h"
#include "src/Character/Player/Player.h"
#include <algorithm>
#include <cmath>

namespace {
// 3軸を別々の速さで揺らすための倍率。同じ速さで揺らすと斜めに往復するだけで、
// 「震えている」ではなく「滑っている」ように見えてしまう
constexpr float kShakeFreqY = 1.37f;
constexpr float kShakeFreqZ = 0.83f;
constexpr float kShakePhaseY = 1.1f;
constexpr float kShakePhaseZ = 2.3f;

// 縦の震えは横より控えめにする（地面へめり込んだり浮いたりして見えないように）
constexpr float kShakeVerticalRatio = 0.45f;

// 潰れの鋭さ。震えに合わせて短く鋭く伸び縮みさせたいので、待機の呼吸より大きくする
constexpr float kSquashSharpness = 2.0f;
} // namespace

void PlayerStateDefeated::RegisterParams() {
	const std::string paramOwnerLabel = "Player/Defeated";
	Hagine::GameParamHub* hub = Hagine::GameParamHub::GetInstance();

	hub->Register(paramOwnerLabel, "ShakeDuration", &kShakeDuration, {0.01f, 0.05f, 5.0f});
	hub->Register(paramOwnerLabel, "ShakeAmplitude", &kShakeAmplitude, {0.01f, 0.0f, 2.0f});
	hub->Register(paramOwnerLabel, "ShakeSpeed", &kShakeSpeed, {0.5f, 1.0f, 200.0f});
	hub->Register(paramOwnerLabel, "ShakeSquash", &kShakeSquash, {0.01f, 0.0f, 1.0f});
	hub->Register(paramOwnerLabel, "ShakePeriod", &kShakePeriod, {0.01f, 0.02f, 1.0f});
	hub->Register(paramOwnerLabel, "Afterglow", &kAfterglow, {0.01f, 0.0f, 5.0f});
}

void PlayerStateDefeated::Enter(Player& player, PlayerContext& context) {
	elapsed_ = 0.0f;
	hasBurst_ = false;
	isFinished_ = false;

	// 倒れた場所から動かさない。押されていた勢いが残っていると、
	// 震えているあいだに滑っていってしまう
	if (context.rigidBody_) {
		context.rigidBody_->velocity = Hagine::Vector3{0.0f, 0.0f, 0.0f};
	}

	player.SetRenderShake(Hagine::Vector3{0.0f, 0.0f, 0.0f});
}

void PlayerStateDefeated::Update(Player& player, PlayerContext& context) {
	elapsed_ += Hagine::Frame::DeltaTime();

	// 弾けた後は何も出さずに余韻を数えるだけ
	if (hasBurst_) {
		if (!isFinished_ && elapsed_ >= kShakeDuration + kAfterglow) {
			isFinished_ = true;
		}
		return;
	}

	if (elapsed_ < kShakeDuration) {
		UpdateShake(player, context);
		return;
	}

	Burst(player, context);
}

void PlayerStateDefeated::UpdateShake(Player& player, PlayerContext& context) {
	// 空中で倒れたときは落ちきらせる。震えは描画オフセットなので落下と喧嘩しない
	if (context.jumpComponent_) {
		context.jumpComponent_->UpdateJump(context);
		context.jumpComponent_->ConsumeLanded();
	}

	// 終わりに近いほど大きく震える。溜めてから弾ける形にしたいので、
	// 線形ではなく二乗で立ち上げる（前半は小刻み、後半で一気に暴れる）
	const float progress = std::clamp(elapsed_ / (std::max)(kShakeDuration, 0.0001f), 0.0f, 1.0f);
	const float growth = progress * progress;

	const float amplitude = kShakeAmplitude * growth;
	const float phase = elapsed_ * kShakeSpeed;
	const Hagine::Vector3 shake{
		std::sin(phase) * amplitude,
		std::sin(phase * kShakeFreqY + kShakePhaseY) * amplitude * kShakeVerticalRatio,
		std::sin(phase * kShakeFreqZ + kShakePhaseZ) * amplitude,
	};
	player.SetRenderShake(shake);

	// 潰れも同じだけ強めていく。スケールを直接書かず要求だけ出すので、
	// 着地の潰れなどが残っていても形が喧嘩しない
	if (context.reactionComponent_) {
		context.reactionComponent_->SetLoop(kShakeSquash * growth, kShakePeriod, kSquashSharpness, 0.0f);
	}
}

void PlayerStateDefeated::Burst(Player& player, PlayerContext& context) {
	(void)context;

	hasBurst_ = true;

	// 揺らしたぶんを戻してから消す。消えた位置がずれて見えないように
	player.SetRenderShake(Hagine::Vector3{0.0f, 0.0f, 0.0f});

	// 体を消して粒を撒くのはプレイヤー側の仕事（色も音もあちらが持っている）
	player.PlayDefeatBurst();
}

void PlayerStateDefeated::Exit(Player& player, PlayerContext& context) {
	(void)context;

	// 抜けない終端のステートだが、デバッグでやり直したときに
	// 揺れたまま・消えたままにならないよう戻しておく
	player.SetRenderShake(Hagine::Vector3{0.0f, 0.0f, 0.0f});
	player.SetIsModelDraw(true);
}
