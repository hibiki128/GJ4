#include "pch.h"
#include "PlayerComponentReaction.h"
#include "Math/Easing.h"
#include "Frame/Frame.h"
#include "Utility/Debug/Param/GameParamHub.h"
#include <numbers>

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

	// ダッシュ（回避）の3段階。仕様の「1F潰す→2F伸びる→12Fから揺り戻し→18Fで戻る」を秒で持つ
	hub->Register(paramOwnerLabel, "DashCompress", &kDashCompress, {0.01f, 0.0f, 0.6f});
	hub->Register(paramOwnerLabel, "DashCompressTime", &kDashCompressTime, {0.01f, 0.0f, 0.5f});
	hub->Register(paramOwnerLabel, "DashStretch", &kDashStretch, {0.01f, 0.0f, 1.5f});
	hub->Register(paramOwnerLabel, "DashSideRatio", &kDashSideRatio, {0.01f, 0.0f, 1.0f});
	hub->Register(paramOwnerLabel, "DashRiseTime", &kDashRiseTime, {0.01f, 0.0f, 0.5f});
	hub->Register(paramOwnerLabel, "DashStretchTime", &kDashStretchTime, {0.01f, 0.01f, 1.0f});
	hub->Register(paramOwnerLabel, "DashRecovery", &kDashRecovery, {0.01f, 0.0f, 1.0f});
	hub->Register(paramOwnerLabel, "DashDuration", &kDashDuration, {0.01f, 0.05f, 2.0f});

	// ジャスト回避は同じ形の倍率違い。伸びの大きさと速さだけをここで変える
	hub->Register(paramOwnerLabel, "PerfectScale", &kPerfectScale, {0.01f, 0.0f, 4.0f});
	hub->Register(paramOwnerLabel, "PerfectTimeScale", &kPerfectTimeScale, {0.01f, 0.1f, 5.0f});
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

	if (dashTime_ >= 0.0f) {
		dashTime_ += deltaTime * dashTimeScale_;
		if (dashTime_ >= kDashDuration) {
			dashTime_ = -1.0f;
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
	// 踏み切りやダッシュの伸びが残っていても着地が優先
	takeoffTime_ = -1.0f;
	dashTime_ = -1.0f;
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

	// ダッシュの溜めは縦の潰れなので、他の演出と同じ offset へ足してから形にする
	offset -= CalcDashCompress();

	// 重ね掛けでスケールが負に反転するのを防ぐ
	offset = std::clamp(offset, -kMaxOffset, kMaxOffset);

	Hagine::Vector3 newScale = {
		baseScale.x - offset,
		baseScale.y + offset,
		baseScale.z - offset,
	};

	// ダッシュの伸びは進行方向（体を向けたローカルZ）へ効かせる。仕様の倍率をそのまま使いたいので、
	// 縦の潰れと違って足し算ではなく掛け算にしてある（基準の大きさを変えても 1.5倍 は 1.5倍のまま）
	const float dashStretch = std::clamp(CalcDashStretch(), -kMaxOffset, kMaxOffset);
	newScale.z *= (1.0f + dashStretch);
	newScale.x *= (1.0f - dashStretch * kDashSideRatio);
	newScale.y *= (1.0f - dashStretch * kDashSideRatio);

	// 着地の「大きく」ぶんは一様倍率で乗せる
	return newScale * (1.0f + sizePop_);
}

void PlayerComponentReaction::PlayDashBurst(const Hagine::Vector3& worldDirection) {
	StartDashBurst(worldDirection, 1.0f, 1.0f);
}

void PlayerComponentReaction::PlayPerfectDodge(const Hagine::Vector3& awayDirection) {
	// 受け流しはダッシュと同じ「潰れて伸びて揺り戻す」形。大きさと速さだけ変える
	StartDashBurst(awayDirection, kPerfectScale, kPerfectTimeScale);
}

void PlayerComponentReaction::StartDashBurst(const Hagine::Vector3& worldDirection, float stretchScale, float timeScale) {
	dashTime_ = 0.0f;
	dashScale_ = stretchScale;
	dashTimeScale_ = (timeScale > 0.0f) ? timeScale : 1.0f;

	// 伸ばす向き。水平成分が無いときは前の向きを使い回す（体が横倒しに伸びるのを防ぐ）
	const float horizontalLengthSq =
		worldDirection.x * worldDirection.x + worldDirection.z * worldDirection.z;
	if (horizontalLengthSq > 0.0001f) {
		dashYaw_ = std::atan2f(worldDirection.x, worldDirection.z);
	}

	// 空中の細長さは飛び出した瞬間に捨てる（着地と同じ理由で、形の落差を勢いに見せる）
	airTarget_ = 0.0f;
	airStretch_ = 0.0f;
}

float PlayerComponentReaction::CalcDashCompress() const {
	if (dashTime_ < 0.0f || kDashCompressTime <= 0.0f || dashTime_ >= kDashCompressTime) {
		return 0.0f;
	}

	// ①溜め: 飛び出した瞬間が一番潰れていて、1〜2フレームで抜ける。
	// ここで止めすぎると「溜めてから走る」ように見えてしまうので短く切り上げる
	return kDashCompress * dashScale_ * (1.0f - dashTime_ / kDashCompressTime);
}

float PlayerComponentReaction::CalcDashStretch() const {
	if (dashTime_ < 0.0f) {
		return 0.0f;
	}

	if (dashTime_ < kDashStretchTime) {
		// ②発射: 数フレームで一気に伸びきり、そこからゆっくり戻る
		if (dashTime_ < kDashRiseTime && kDashRiseTime > 0.0f) {
			return kDashStretch * dashScale_ * (dashTime_ / kDashRiseTime);
		}
		const float t = std::clamp(
			(dashTime_ - kDashRiseTime) / (std::max)(0.0001f, kDashStretchTime - kDashRiseTime), 0.0f, 1.0f);
		// 戻り始めが一番速く、元の形に近づくほど緩やか
		return kDashStretch * dashScale_ * (1.0f - t) * (1.0f - t);
	}

	// ③揺り戻し: 逆向きへ一度だけ潰れて戻る。
	// 正弦波の半周ぶんなので山はひとつしか出ない（何度も揺らさない）
	const float t = std::clamp(
		(dashTime_ - kDashStretchTime) / (std::max)(0.0001f, kDashDuration - kDashStretchTime), 0.0f, 1.0f);
	return -kDashRecovery * dashScale_ * std::sinf(t * std::numbers::pi_v<float>);
}
