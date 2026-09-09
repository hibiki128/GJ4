#include "PlayerVoiceComponent.h"
#include "Utility/Debug/Param/GameParamHub.h"
#include "random.h"
#include "src/Audio/GameSounds.h"
#include <algorithm>
#include <string>

void PlayerVoiceComponent::RegisterParams() {
	const std::string paramOwnerLabel = "Player/Voice";
	Hagine::GameParamHub* hub = Hagine::GameParamHub::GetInstance();

	hub->Register(paramOwnerLabel, "IdleInterval", &kIdleInterval, {0.05f, 0.1f, 10.0f});
	hub->Register(paramOwnerLabel, "IdleIntervalRandom", &kIdleIntervalRandom, {0.05f, 0.0f, 5.0f});
	hub->Register(paramOwnerLabel, "MoveInterval", &kMoveInterval, {0.01f, 0.05f, 3.0f});
	hub->Register(paramOwnerLabel, "DashInterval", &kDashInterval, {0.01f, 0.05f, 3.0f});
}

float PlayerVoiceComponent::IntervalOf(Kind kind) const {
	switch (kind) {
	case Kind::Idle:
		// ぽよぽよだけ間隔を散らす。待機は同じ音が何度も続くので、
		// きっかり同じ間隔だと「鳴らしている」のが分かってしまう
		return (std::max)(0.05f, kIdleInterval + Hagine::Random::Range(0.0f, kIdleIntervalRandom));
	case Kind::Move:
		return (std::max)(0.05f, kMoveInterval);
	case Kind::Dash:
		return (std::max)(0.05f, kDashInterval);
	default:
		return 0.0f;
	}
}

void PlayerVoiceComponent::PlayOnce(Kind kind) {
	GameSounds* sounds = GameSounds::GetInstance();

	switch (kind) {
	case Kind::Idle:
		// 2種類を交互に鳴らして、同じ音が続かないようにする
		sounds->Play(useIdle2_ ? GameSounds::Id::PlayerIdle2 : GameSounds::Id::PlayerIdle1);
		useIdle2_ = !useIdle2_;
		break;
	case Kind::Move:
	case Kind::Dash:
		sounds->Play(GameSounds::Id::PlayerMove);
		break;
	default:
		break;
	}
}

void PlayerVoiceComponent::Update(float deltaTime) {
	const Kind request = request_;
	request_ = Kind::None; // 要求は1フレーム限り。出されなくなれば自然に止まる

	// 誰も要求していない（空中・被弾中・やられた後）。
	// 次に要求が来たときへ持ち越さないよう、数えている途中の間隔は捨てる
	if (request == Kind::None) {
		current_ = Kind::None;
		timer_ = 0.0f;
		return;
	}

	// 種類が変わったら数え直す。
	// 立ち止まった瞬間・走り出した瞬間にいきなり鳴らさないのは、
	// スティックを小刻みに倒したときに音が連射されるのを防ぐため
	if (request != current_) {
		current_ = request;
		timer_ = IntervalOf(current_);
		return;
	}

	timer_ -= deltaTime;
	if (timer_ > 0.0f) {
		return;
	}

	PlayOnce(current_);
	timer_ = IntervalOf(current_);
}
