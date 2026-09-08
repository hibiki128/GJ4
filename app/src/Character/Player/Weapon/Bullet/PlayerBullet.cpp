#include "PlayerBullet.h"
#include "Frame/Frame.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace {
// 色をそのまま出したいので白テクスチャを貼る（色は SetColor 側で決める）
constexpr const char* kBulletTexturePath = "debug/white1x1.png";

/// <summary>
/// from から to の向きへ、最大 maxDegrees だけ回した向きを返す。
/// 割合だけの補正は的が近いほど強く効いてしまうので、曲がれる角度に上限をつけて
/// 「弱いホーミング」の効き方を距離によらず一定に保つ
/// </summary>
Hagine::Vector3 LimitTurn(const Hagine::Vector3& from, const Hagine::Vector3& to, float maxDegrees) {
	if (maxDegrees <= 0.0f) {
		return from;
	}

	const float cosAngle = std::clamp(from.Dot(to), -1.0f, 1.0f);
	const float angle = std::acos(cosAngle);
	const float maxAngle = maxDegrees * (std::numbers::pi_v<float> / 180.0f);
	if (angle <= maxAngle) {
		return to; // 上限より小さい曲がりなので、そのまま向けてよい
	}

	// from に直交する成分を軸にして、maxAngle ぶんだけ回した向きを直接作る
	Hagine::Vector3 orthogonal = to - from * cosAngle;
	if (orthogonal.LengthSq() <= 0.0001f) {
		return from; // ほぼ真後ろ。回す向きが決まらないので曲げない
	}
	orthogonal = orthogonal.Normalize();

	return (from * std::cos(maxAngle) + orthogonal * std::sin(maxAngle)).Normalize();
}
} // namespace

void PlayerBullet::Init(const std::string objectName) {
	BaseObject::Init(objectName);
	CreatePrimitiveModel(Hagine::PrimitiveType::Sphere);
	SetTexture(kBulletTexturePath);

	// 弾はシーンのjsonに保存しない / ギズモの選択対象にもしない
	SetShouldSave(false);
	SetGizmoSelectable(false);

	// 生成直後は待機状態にしておく
	Deactivate();
}

void PlayerBullet::Fire(const Shot& shot) {
	transform_->translation_ = shot.position;
	transform_->scale_ = Hagine::Vector3{shot.radius, shot.radius, shot.radius};

	direction_ = (shot.direction.LengthSq() > 0.0f) ? shot.direction.Normalize()
	                                                : Hagine::Vector3{0.0f, 0.0f, 1.0f};
	speed_ = shot.speed;
	lifeTime_ = shot.lifeTime;
	correctionRate_ = shot.correctionRate;
	maxTurnDegreesPerSecond_ = shot.maxTurnDegreesPerSecond;
	targetPositionGetter_ = shot.targetPositionGetter;
	hitTester_ = shot.hitTester;

	SetColor(shot.rgba);

	isActive_ = true;
	SetIsModelDraw(true);
}

void PlayerBullet::Update() {
	// 待機中の弾は動かさない（マネージャーには登録されたままなので毎フレーム呼ばれる）
	if (!isActive_) {
		return;
	}

	const float deltaTime = Hagine::Frame::DeltaTime();

	ApplyTrajectoryCorrection(deltaTime);

	const Hagine::Vector3 previousPosition = transform_->translation_;
	transform_->translation_ += direction_ * (speed_ * deltaTime);

	// 着弾判定は「動いた線分」で行う（速い弾でもすり抜けない）
	if (hitTester_ && hitTester_(previousPosition, transform_->translation_)) {
		Deactivate();
		return;
	}

	lifeTime_ -= deltaTime;
	if (lifeTime_ <= 0.0f) {
		Deactivate();
		return;
	}

	BaseObject::Update();
}

void PlayerBullet::ApplyTrajectoryCorrection(float deltaTime) {
	if (!targetPositionGetter_) {
		return;
	}

	Hagine::Vector3 targetPosition{};
	if (!targetPositionGetter_(targetPosition)) {
		// 対象が消えた・見失った場合はまっすぐ飛ぶ
		targetPositionGetter_ = nullptr;
		return;
	}

	const Hagine::Vector3 toTarget = targetPosition - transform_->translation_;
	if (toTarget.LengthSq() <= 0.0001f) {
		return;
	}

	const Hagine::Vector3 desired = toTarget.Normalize();

	// 追う先を通り過ぎたら補正をやめる。
	// 狙う先が動かない一点なので、これが無いと外した弾がUターンして戻ってくる
	if (desired.Dot(direction_) <= 0.0f) {
		targetPositionGetter_ = nullptr;
		return;
	}

	const float rate = std::clamp(correctionRate_ * deltaTime, 0.0f, 1.0f);
	const Hagine::Vector3 blended = direction_ + (desired - direction_) * rate;
	if (blended.LengthSq() <= 0.0001f) {
		return;
	}

	direction_ = LimitTurn(direction_, blended.Normalize(), maxTurnDegreesPerSecond_ * deltaTime);
}

void PlayerBullet::Draw(const Hagine::ViewProjection& viewProjection) {
	BaseObject::Draw(viewProjection);
}

void PlayerBullet::Deactivate() {
	isActive_ = false;
	speed_ = 0.0f;
	maxTurnDegreesPerSecond_ = 0.0f;

	// 撃った相手を掴んだままにしない（相手が消えても弾が握り続けてしまう）
	targetPositionGetter_ = nullptr;
	hitTester_ = nullptr;

	// 破棄はせず、描画だけ止めて次の発射まで待つ
	SetIsModelDraw(false);
}
