#include "pch.h"
#include "PlayerComponentReaction.h"
#include "Math/Easing.h"
#include "Frame/Frame.h"
#include "Utility/Debug/Param/GameParamHub.h"

Hagine::Vector3 PlayerComponentReaction::SquashStretch(const Hagine::Vector3& scale, float easeT, float time, float amplitude, float period) {
	return Hagine::EaseAmplitudeScale(scale, easeT, time, amplitude, period);
}

Hagine::Vector3 PlayerComponentReaction::LoopSquashStretch(const Hagine::Vector3& scale, float time, float amplitude, float period, float sharpness, float phase) {
	return Hagine::LoopAmplitudeScale(scale, time, amplitude, period, sharpness, phase);
}

void PlayerComponentReaction::RegisterParams() {
	const std::string paramOwnerLabel = "Player/Reaction";
	Hagine::GameParamHub* hub = Hagine::GameParamHub::GetInstance();

	hub->Register(paramOwnerLabel, "LoopFollow", &kLoopFollow, {0.1f, 0.0f, 60.0f});
	hub->Register(paramOwnerLabel, "AirFollow", &kAirFollow, {0.1f, 0.0f, 60.0f});
	hub->Register(paramOwnerLabel, "LandAmpMin", &kLandAmpMin, {0.01f, 0.0f, 1.0f});
	hub->Register(paramOwnerLabel, "LandAmpMax", &kLandAmpMax, {0.01f, 0.0f, 1.0f});
	hub->Register(paramOwnerLabel, "LandDuration", &kLandDuration, {0.01f, 0.05f, 3.0f});
	hub->Register(paramOwnerLabel, "LandPeriod", &kLandPeriod, {0.01f, 0.05f, 2.0f});
	hub->Register(paramOwnerLabel, "LandPop", &kLandPop, {0.01f, 0.0f, 1.0f});
	hub->Register(paramOwnerLabel, "PopDecay", &kPopDecay, {0.1f, 0.1f, 60.0f});
	hub->Register(paramOwnerLabel, "MaxOffset", &kMaxOffset, {0.01f, 0.0f, 0.95f});

	// 踏み切りの予備動作はチェックボックスで丸ごとオン/オフできる
	hub->Register(paramOwnerLabel, "UseTakeoff", &kUseTakeoff_);
	hub->Register(paramOwnerLabel, "TakeoffAmp", &kTakeoffAmp, {0.01f, 0.0f, 1.0f});
	hub->Register(paramOwnerLabel, "TakeoffDuration", &kTakeoffDuration, {0.01f, 0.02f, 1.0f});
}

void PlayerComponentReaction::Update() {
	const float deltaTime = Hagine::Frame::DeltaTime();

	loopTime_ += deltaTime;

	// 目標値へ指数追従（フレームレート非依存・カクつき防止）
	loopAmp_ += (loopAmpRequest_ - loopAmp_) * (1.0f - std::expf(-kLoopFollow * deltaTime));
	// 要求は1フレーム限り。誰も SetLoop を呼ばなくなれば振幅は自然に0へ収束する
	loopAmpRequest_ = 0.0f;

	airStretch_ += (airTarget_ - airStretch_) * (1.0f - std::expf(-kAirFollow * deltaTime));
	// 空中ストレッチも同様。ジャンプステートを抜ければ勝手に丸い形へ戻る
	airTarget_ = 0.0f;

	if (landTime_ >= 0.0f) {
		landTime_ += deltaTime;
		if (landTime_ >= kLandDuration) { 
			landTime_ = -1.0f;
		}
	}

	if (takeoffTime_ >= 0.0f) {
		takeoffTime_ += deltaTime;
		// 実行中に kUseTakeoff_ を切られたら、その場で打ち切る
		if (takeoffTime_ >= kTakeoffDuration || !kUseTakeoff_) {
			takeoffTime_ = -1.0f;
		}
	}

	sizePop_ += (0.0f - sizePop_) * (1.0f - std::expf(-kPopDecay * deltaTime));
}

void PlayerComponentReaction::SetLoop(float amp, float period, float sharpness, float phase) {
	loopAmpRequest_ = amp;
	loopSharpness_ = sharpness;
	loopPhase_ = phase;
	if (period > 0.0f) {
		loopPeriod_ = period;
	}
}

void PlayerComponentReaction::SetAirStretch(float target) {
	airTarget_ = target;
}

void PlayerComponentReaction::PlayLanding(float strength01) {
	const float strength = std::clamp(strength01, 0.0f, 1.0f);

	landTime_ = 0.0f;
	landAmp_ = Hagine::LerpE(kLandAmpMin, kLandAmpMax, strength);
	sizePop_ = kLandPop * strength;

	// 空中の細長さは着地した瞬間に捨てる。
	// 伸びた形から一気に潰れた形へ飛ぶこの1フレームの落差が「衝撃」に見える
	airTarget_ = 0.0f;
	airStretch_ = 0.0f;
	// 踏み切りが残っていても着地が優先
	takeoffTime_ = -1.0f;
}

void PlayerComponentReaction::PlayTakeoff() {
	if (!kUseTakeoff_) {
		return;
	}
	takeoffTime_ = 0.0f;
}

Hagine::Vector3 PlayerComponentReaction::Apply(const Hagine::Vector3& baseScale) const {
	// offset > 0 : 縦に伸びて横が細い / offset < 0 : 平べったい
	float offset = Hagine::LoopElasticAmplitude(loopTime_, loopAmp_, loopPeriod_, loopSharpness_, loopPhase_);

	offset += airStretch_;

	if (landTime_ >= 0.0f) {
		// EaseOutElasticAmplitude は t≈0 で -amplitude から始まる。
		// つまり呼んだ瞬間が一番平べったく、そこから弾性で元へ戻っていく
		offset += Hagine::EaseOutElasticAmplitude(landTime_, kLandDuration, landAmp_, kLandPeriod);
	}

	if (takeoffTime_ >= 0.0f && kTakeoffDuration > 0.0f) {
		// kTakeoffDuration で正弦波1周ぶん。符号を反転して「前半で縮み、後半で伸びる」にする
		offset -= Hagine::LoopElasticAmplitude(takeoffTime_, kTakeoffAmp, kTakeoffDuration);
	}

	// 重ね掛けでスケールが負に反転するのを防ぐ
	offset = std::clamp(offset, -kMaxOffset, kMaxOffset);

	Hagine::Vector3 newScale = {
		baseScale.x - offset,
		baseScale.y + offset,
		baseScale.z - offset,
	};

	// 着地の「大きく」ぶんは一様倍率で乗せる
	return newScale * (1.0f + sizePop_);
}
