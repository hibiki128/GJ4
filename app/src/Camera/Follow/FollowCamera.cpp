#include "FollowCamera.h"
#include <Input.h>
#include <MyMath.h>
#include <algorithm>
#include <camera/CameraManager.h>
#include <cmath>
#include <data/DataHandler.h>
#include <frame/Frame.h>
#include <line/LineRenderer.h>
#include <object/base/BaseObject.h>
#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

using namespace Hagine;

namespace
{
	constexpr float kPi = 3.141592654f;

	// デバッグ線の色
	constexpr Vector4 kColorPlayer = { 0.3f, 0.9f, 0.4f, 1.0f };  // プレイヤーの注視点
	constexpr Vector4 kColorBoss = { 0.9f, 0.4f, 0.3f, 1.0f };    // ボスの注視点
	constexpr Vector4 kColorTarget = { 1.0f, 0.9f, 0.3f, 1.0f };  // カメラの注視点
	constexpr Vector4 kColorCamera = { 0.4f, 0.7f, 1.0f, 1.0f };  // カメラ位置
	constexpr Vector4 kColorBlocked = { 1.0f, 0.3f, 0.3f, 1.0f }; // 遮蔽で押し戻された分

	/// <summary>
	/// 目標値へ滑らかに近づける（仕様書 §13 の SmoothDamp）。
	/// バネの動きを解いたもので、smoothTime は「だいたい目標へ届くまでの秒数」。
	/// velocity は呼び出し側が持ち越すので、目標が急に変わっても速度が繋がって滑らかに曲がる
	/// </summary>
	/// <param name="current">今の値</param>
	/// <param name="target">目標値</param>
	/// <param name="velocity">持ち越す速度（呼び出しごとに更新される）</param>
	/// <param name="smoothTime">追いつくまでのおおよその秒数</param>
	/// <param name="deltaTime">前フレームからの経過秒</param>
	/// <returns>float: 近づけた後の値</returns>
	float SmoothDamp(float current, float target, float& velocity, float smoothTime, float deltaTime)
	{
		// 0秒を指定されたらスムージング無しとして即座に目標値にする
		if (smoothTime <= 0.0f || deltaTime <= 0.0f)
		{
			velocity = 0.0f;
			return target;
		}

		const float omega = 2.0f / smoothTime;
		const float x = omega * deltaTime;
		// exp(-x) の近似。毎フレーム exp を呼ばずに済ませるための定番の式
		const float decay = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);

		const float change = current - target;
		const float temp = (velocity + omega * change) * deltaTime;
		velocity = (velocity - omega * temp) * decay;
		return target + (change + temp) * decay;
	}

	/// <summary>
	/// SmoothDamp の Vector3 版（軸ごとに同じ計算をする）
	/// </summary>
	Vector3 SmoothDamp(const Vector3& current, const Vector3& target, Vector3& velocity,
		float smoothTime, float deltaTime)
	{
		return Vector3{
			SmoothDamp(current.x, target.x, velocity.x, smoothTime, deltaTime),
			SmoothDamp(current.y, target.y, velocity.y, smoothTime, deltaTime),
			SmoothDamp(current.z, target.z, velocity.z, smoothTime, deltaTime),
		};
	}

	/// <summary>
	/// スムージングの補間率を求める。
	/// フレームレートが変わっても同じ速さで目標へ追いつくように、経過時間から指数的に求める
	/// </summary>
	/// <param name="rate">追いつく速さ(1/秒)。0以下ならスムージング無し</param>
	/// <param name="deltaTime">前フレームからの経過秒</param>
	/// <returns>float: 補間率[0,1]</returns>
	float SmoothFactor(float rate, float deltaTime)
	{
		if (rate <= 0.0f)
		{
			return 1.0f; // スムージング無し（その場で目標値になる）
		}
		return 1.0f - std::exp(-rate * deltaTime);
	}

	/// <summary>
	/// 角度を [-π, π] に収める。回し続けても値が際限なく増えないようにするため
	/// </summary>
	/// <param name="radian">角度(ラジアン)</param>
	/// <returns>float: [-π, π] に収めた角度</returns>
	float WrapAngle(float radian)
	{
		radian = std::fmod(radian + kPi, 2.0f * kPi);
		if (radian < 0.0f)
		{
			radian += 2.0f * kPi;
		}
		return radian - kPi;
	}

	/// <summary>
	/// XZ平面での向き（ヨー角）を求める。真上・真下を向いていて向きが決まらない場合は false
	/// </summary>
	bool TryCalcYawXZ(const Vector3& direction, float& outYaw)
	{
		const float lengthSq = direction.x * direction.x + direction.z * direction.z;
		if (lengthSq < 0.0001f)
		{
			return false;
		}
		outYaw = std::atan2(direction.x, direction.z);
		return true;
	}
} // namespace

void FollowCamera::Init(const std::string& cameraName)
{
	// カメラ本体は CameraManager が所有する（名前で切り替えられるようにするため）
	pCamera_ = CameraManager::GetInstance()->Create(cameraName);
	pCamera_->SetClipRange(0.1f, 1100.0f);

	// ImGui で調整して保存した値があればそれを使う
	Load();

	// 実行時調整の口を開ける。ハブに保存済みの値があればここで上書きされるので、
	// 自前のJSON → ハブの調整値 の順で反映されることになる
	RegisterTuningParameters(cameraName);

	// 画角を反映してから初期の構図を作る（仕様書 §10）
	pCamera_->SetFovYDegrees(fovDegrees_);
	ResetView();
	SnapToTarget();
}

void FollowCamera::RegisterTuningParameters(const std::string& cameraName)
{
	params_.SetOwner("Camera/" + cameraName);

	// --- 画角（仕様書 §10。§24 の調整順でも最初に見る項目）---
	GameParamHub::Options fovOptions{};
	fovOptions.speed = 0.5f;
	fovOptions.min = 30.0f;
	fovOptions.max = 110.0f;
	// 画角はカメラ本体が持っているので、変えたその場で渡し直す
	fovOptions.onChange = [this] {
		if (pCamera_)
		{
			pCamera_->SetFovYDegrees(fovDegrees_);
		}
		};
	params_.Register("画角:FOV(度)", &fovDegrees_, fovOptions);

	// --- 注視点（仕様書 §5）---
	params_.Register("注視点:プレイヤーのずらし", &playerTargetOffset_, { 0.05f, -10.0f, 10.0f });

	// --- 通常モード（仕様書 §23 Phase1）---
	params_.Register("通常:カメラ距離", &normalDistance_, { 0.1f, 1.0f, 50.0f });
	params_.Register("通常:注視点の追従時間(秒)", &normalTargetSmoothTime_, { 0.01f, 0.0f, 2.0f });
	params_.Register("通常:距離の追従時間(秒)", &normalDistanceSmoothTime_, { 0.01f, 0.0f, 2.0f });

	// --- やられたときの寄り ---
	params_.Register("やられ:カメラ距離", &defeatDistance_, { 0.1f, 0.5f, 30.0f });
	params_.Register("やられ:注視点の高さ", &defeatTargetHeight_, { 0.05f, -5.0f, 10.0f });
	params_.Register("やられ:注視点の追従時間(秒)", &defeatTargetSmoothTime_, { 0.01f, 0.0f, 3.0f });
	params_.Register("やられ:距離の追従時間(秒)", &defeatDistanceSmoothTime_, { 0.01f, 0.0f, 3.0f });

	// --- ボス戦のフレーミング（仕様書 §6〜§9・§19）---
	params_.Register("ボス戦:プレイヤー寄せ", &bossConfig_.playerWeight, { 0.01f, 0.0f, 1.0f });
	params_.Register("ボス戦:ボスの注視点ずらし", &bossConfig_.bossTargetOffset, { 0.05f, -20.0f, 20.0f });
	params_.Register("ボス戦:距離倍率", &bossConfig_.distanceScale, { 0.01f, 0.0f, 3.0f });
	params_.Register("ボス戦:ボスの大きさ倍率", &bossConfig_.bossRadiusScale, { 0.01f, 0.0f, 3.0f });
	params_.Register("ボス戦:画面の余白", &bossConfig_.framingOffset, { 0.1f, 0.0f, 20.0f });
	params_.Register("ボス戦:距離の下限", &bossConfig_.minDistance, { 0.1f, 0.5f, 100.0f });
	params_.Register("ボス戦:距離の上限", &bossConfig_.maxDistance, { 0.1f, 0.5f, 100.0f });
	params_.Register("ボス戦:注視点の追従時間(秒)", &bossConfig_.targetSmoothTime, { 0.01f, 0.0f, 2.0f });
	params_.Register("ボス戦:距離の追従時間(秒)", &bossConfig_.distanceSmoothTime, { 0.01f, 0.0f, 2.0f });

	// --- 視点操作（仕様書 §11・§12）---
	params_.Register("視点:左右の速さ(度/秒)", &yawSpeedDegrees_, { 1.0f, 0.0f, 720.0f });
	params_.Register("視点:上下の速さ(度/秒)", &pitchSpeedDegrees_, { 1.0f, 0.0f, 720.0f });
	params_.Register("視点:見上げる限界(度)", &pitchMinDegrees_, { 1.0f, -89.0f, 89.0f });
	params_.Register("視点:見下ろす限界(度)", &pitchMaxDegrees_, { 1.0f, -89.0f, 89.0f });
	params_.Register("視点:初期の見下ろし角(度)", &defaultPitchDegrees_, { 0.5f, -89.0f, 89.0f });
	params_.Register("視点:追いつく速さ", &rotateSmoothRate_, { 0.1f, 0.0f, 60.0f });

	// --- ボス方向への自動補正（仕様書 §14）---
	params_.Register("自動補正:使う", &autoAlignEnabled_);
	params_.Register("自動補正:補正しない角度(度)", &autoAlignDeadDegrees_, { 1.0f, 0.0f, 180.0f });
	params_.Register("自動補正:最大に補正する角度(度)", &autoAlignFullDegrees_, { 1.0f, 0.0f, 180.0f });
	params_.Register("自動補正:速さ(度/秒)", &autoAlignSpeedDegrees_, { 1.0f, 0.0f, 360.0f });

	// --- 被弾の衝撃（AddImpact）---
	params_.Register("衝撃:収まるまでの時間(秒)", &impactDuration_, { 0.01f, 0.01f, 2.0f });
	params_.Register("衝撃:後ろへ引く距離", &impactPullBack_, { 0.05f, 0.0f, 10.0f });
	params_.Register("衝撃:揺れの大きさ", &impactShakeAmount_, { 0.01f, 0.0f, 2.0f });
	params_.Register("衝撃:揺れの速さ", &impactShakeSpeed_, { 0.5f, 0.0f, 120.0f });

	// --- ダッシュの押し出し（AddDashPush）---
	params_.Register("ダッシュ:前へ押し出す距離", &dashPushDistance_, { 0.01f, 0.0f, 2.0f });
	params_.Register("ダッシュ:戻るまでの時間(秒)", &dashPushDuration_, { 0.01f, 0.01f, 1.0f });

	// --- カメラ衝突（仕様書 §15）---
	params_.Register("衝突:処理する", &collisionEnabled_);
	params_.Register("衝突:カメラの太さ", &collisionRadius_, { 0.01f, 0.0f, 2.0f });
	params_.Register("衝突:余裕", &collisionMargin_, { 0.01f, 0.0f, 2.0f });
	params_.Register("衝突:最小の寄り", &minCollisionDistance_, { 0.05f, 0.1f, 10.0f });
	params_.Register("衝突:地面の高さ", &groundHeight_, { 0.05f, -50.0f, 50.0f });

	// --- デバッグ表示（仕様書 §22）---
	params_.Register("デバッグ:線を表示", &debugDraw_);
}

void FollowCamera::Update(const CameraInput& input)
{
	// 追従対象が存在する場合のみ処理
	if (!pCamera_ || !pTarget_)
	{
		return;
	}

	// スムージングはフレームレートに左右されないよう経過時間で進める
	const float deltaTime = Frame::DeltaTime();

	// 被弾の衝撃を収めていく（構図そのものは動かさないので、収まれば元の絵に戻る）
	if (impactTimer_ > 0.0f)
	{
		impactTimer_ -= deltaTime;
		if (impactTimer_ <= 0.0f)
		{
			impactTimer_ = 0.0f;
			impactStrength_ = 0.0f;
		}
	}

	// ダッシュの押し出しも同じように戻していく（距離だけの効果なので構図は動かない）
	if (dashPushTimer_ > 0.0f)
	{
		dashPushTimer_ -= deltaTime;
		if (dashPushTimer_ <= 0.0f)
		{
			dashPushTimer_ = 0.0f;
			dashPushStrength_ = 0.0f;
		}
	}

	// 1〜4. プレイヤーとボスの注視点を集める（仕様書 §20）
	const Vector3 playerTarget = CalcPlayerTarget();
	const CameraFrameTarget frame = CalcFrameTarget();

	// 5〜7. 理想の注視点とカメラ距離を求める
	Vector3 desiredTarget{};
	float desiredDistance = 0.0f;
	CalcDesired(playerTarget, frame, desiredTarget, desiredDistance);

	// 8. 視点の向きを決める（入力は即時、ボスからの自動補正だけ滑らかに）
	UpdateAngle(input, deltaTime);
	UpdateAutoAlign(desiredTarget, frame, deltaTime);

	// 11. 注視点と距離を補間する（仕様書 §13）
	if (!hasState_)
	{
		// 追従を始めた最初のフレームは、遠くから飛んでこないようにその場で合わせる
		target_ = desiredTarget;
		distance_ = desiredDistance;
		targetVelocity_ = Vector3{};
		distanceVelocity_ = 0.0f;
		hasState_ = true;
	}
	else
	{
		target_ = SmoothDamp(target_, desiredTarget, targetVelocity_, GetTargetSmoothTime(), deltaTime);
		distance_ = SmoothDamp(distance_, desiredDistance, distanceVelocity_, GetDistanceSmoothTime(), deltaTime);
	}

	// 9〜13. カメラ位置を決めて反映する（衝突の解決は ApplyToCamera の中）
	ApplyToCamera();

	// 調整用のデバッグ線（仕様書 §22）
	DrawDebugLines(playerTarget, frame);
}

void FollowCamera::Activate()
{
	if (!pCamera_)
	{
		return;
	}

	// 切り替えた瞬間から追従した構図になるように、先に位置を合わせておく
	SnapToTarget();

	// 以降はこのカメラで描画される
	CameraManager::GetInstance()->SetActive(pCamera_);
}

void FollowCamera::ResetView()
{
	// ターゲットの真後ろから、少し見下ろす向きへ戻す（仕様書 §12）
	yaw_ = 0.0f;
	pitch_ = degreesToRadians(
		std::clamp(defaultPitchDegrees_, (std::min)(pitchMinDegrees_, pitchMaxDegrees_),
			(std::max)(pitchMinDegrees_, pitchMaxDegrees_)));
}

void FollowCamera::SnapToTarget()
{
	// 次の更新で理想の構図へ瞬間移動させる
	hasState_ = false;
	targetVelocity_ = Vector3{};
	distanceVelocity_ = 0.0f;

	if (!pCamera_ || !pTarget_)
	{
		// ターゲットが未設定なら、設定された後の最初の更新で合わせ直す
		return;
	}

	const Vector3 playerTarget = CalcPlayerTarget();
	const CameraFrameTarget frame = CalcFrameTarget();
	CalcDesired(playerTarget, frame, target_, distance_);
	hasState_ = true;

	ApplyToCamera();
}

Vector3 FollowCamera::CalcPlayerTarget() const
{
	// 足元ではなく胸〜頭あたりを見る（仕様書 §5）
	return pTarget_->translation_ + playerTargetOffset_;
}

CameraFrameTarget FollowCamera::CalcFrameTarget() const
{
	// Normal モードでは相手を気にせずプレイヤーだけを追う（仕様書 §16）
	if (mode_ != CameraMode::BossBattle || !frameTargetProvider_)
	{
		return CameraFrameTarget{};
	}

	CameraFrameTarget frame = frameTargetProvider_();
	if (!frame.valid)
	{
		return CameraFrameTarget{};
	}

	// ボスの中心から少し上を見るなどの微調整はカメラ側で持つ（仕様書 §5）
	frame.position = frame.position + bossConfig_.bossTargetOffset;
	frame.radius = (std::max)(frame.radius, 0.0f);
	return frame;
}

void FollowCamera::CalcDesired(const Vector3& playerTarget, const CameraFrameTarget& frame,
	Vector3& outTarget, float& outDistance) const
{
	// やられたときはプレイヤーだけへ寄る。
	// 注視点の高さも通常より下げて、寄っても体が画面の中心に残るようにする
	if (mode_ == CameraMode::Defeat)
	{
		outTarget = pTarget_->translation_ + Vector3{ 0.0f, defeatTargetHeight_, 0.0f };
		outDistance = defeatDistance_;
		return;
	}

	// 収める相手がいなければ、プレイヤーだけを追う普通のTPS（仕様書 §23 Phase 1）
	if (!frame.valid)
	{
		outTarget = playerTarget;
		outDistance = normalDistance_;
		return;
	}

	// 注視点はプレイヤーとボスの加重中間点（仕様書 §6）。
	// playerWeight を大きくするほどプレイヤー寄りになり、画面下側にプレイヤーが残る
	const float bossWeight = std::clamp(1.0f - bossConfig_.playerWeight, 0.0f, 1.0f);
	outTarget = Lerp(playerTarget, frame.position, bossWeight);

	// 2人が離れるほど、そしてボスが大きいほどカメラを引く（仕様書 §7〜§9）
	const float playerBossDistance = (frame.position - playerTarget).Length();
	outDistance = playerBossDistance * bossConfig_.distanceScale
		+ frame.radius * bossConfig_.bossRadiusScale
		+ bossConfig_.framingOffset;

	// 寄りすぎ・引きすぎを防ぐ（仕様書 §8）
	const float minDistance = (std::min)(bossConfig_.minDistance, bossConfig_.maxDistance);
	const float maxDistance = (std::max)(bossConfig_.minDistance, bossConfig_.maxDistance);
	outDistance = std::clamp(outDistance, minDistance, maxDistance);
}

void FollowCamera::UpdateAngle(const CameraInput& input, float deltaTime)
{
	// 仕様書 §13.3：手動のカメラ操作を補間すると操作が重くなるので、入力は既定で即時に効かせる。
	// rotateSmoothRate_ を上げたときだけ、追いつくまでに遅れが出る
	float desiredYaw = yaw_ + input.look.x * degreesToRadians(yawSpeedDegrees_) * deltaTime;
	// スティックを上に倒す（↑キーを押す）と上を向く＝見下ろし角が減る
	float desiredPitch = pitch_ - input.look.y * degreesToRadians(pitchSpeedDegrees_) * deltaTime;

	// 真上・真下を通り越すと構図が破綻するので、上下の向きは制限する（仕様書 §11）
	const float pitchMin = degreesToRadians((std::min)(pitchMinDegrees_, pitchMaxDegrees_));
	const float pitchMax = degreesToRadians((std::max)(pitchMinDegrees_, pitchMaxDegrees_));
	desiredPitch = std::clamp(desiredPitch, pitchMin, pitchMax);

	const float rotateRate = SmoothFactor(rotateSmoothRate_, deltaTime);
	yaw_ = WrapAngle(LerpShortAngle(yaw_, desiredYaw, rotateRate));
	pitch_ = Lerp(pitch_, desiredPitch, rotateRate);
}

void FollowCamera::UpdateAutoAlign(const Vector3& target, const CameraFrameTarget& frame, float deltaTime)
{
	// 仕様書 §14：常にボスへ固定するロックオンにはせず、
	// 画面中央付近なら何もせず、外れているときだけ弱く引き戻す
	if (!autoAlignEnabled_ || !frame.valid)
	{
		return;
	}

	float bossYaw = 0.0f;
	if (!TryCalcYawXZ(frame.position - target, bossYaw))
	{
		return; // 注視点の真上・真下にボスがいる場合は向きが決まらないので何もしない
	}

	// 今の視線とボスの方向のずれ。カメラは yaw_ の方向を向いている
	const float diff = WrapAngle(bossYaw - yaw_);
	const float offDegrees = std::abs(radiansToDegrees(diff));

	const float deadDegrees = (std::min)(autoAlignDeadDegrees_, autoAlignFullDegrees_);
	const float fullDegrees = (std::max)(autoAlignDeadDegrees_, autoAlignFullDegrees_);
	if (offDegrees <= deadDegrees)
	{
		return; // 画面中央付近なので補正しない
	}

	// 画面端で弱く、画面外で強く効くように 0→1 で強さを作る
	const float range = (std::max)(fullDegrees - deadDegrees, 0.001f);
	const float strength = std::clamp((offDegrees - deadDegrees) / range, 0.0f, 1.0f);

	// 行き過ぎないよう、残りのずれを超えては回さない
	const float step = (std::min)(
		degreesToRadians(autoAlignSpeedDegrees_) * strength * deltaTime, std::abs(diff));
	yaw_ = WrapAngle(yaw_ + (diff > 0.0f ? step : -step));
}

Vector3 FollowCamera::CalcForward(float yaw, float pitch)
{
	// ピッチ→ヨーの順にワールド前方(+Z)を回す（仕様書 §3）。
	// ピッチが正だと前方が下を向くので、カメラは注視点より上へ下がることになる
	const Matrix4x4 rotateMatrix = MakeRotateXYZMatrix(Vector3{ pitch, yaw, 0.0f });
	return TransformNormal(kWorldForward, rotateMatrix);
}

Vector3 FollowCamera::ResolveCameraCollision(const Vector3& target, const Vector3& desiredPosition) const
{
	Vector3 position = desiredPosition;
	if (!collisionEnabled_)
	{
		return position;
	}

	// 注視点からカメラへ向かって飛ばし、途中に壁があればその手前まで詰める（仕様書 §15）
	const Vector3 toCamera = position - target;
	const float desiredDistance = toCamera.Length();
	if (desiredDistance > 0.0001f && obstacleProvider_)
	{
		const std::vector<BaseObject*> obstacles = obstacleProvider_();

		// カメラの太さぶん手前で当てたいので、レイは半径ぶん伸ばして飛ばす
		Ray ray{};
		ray.origin = target;
		ray.direction = toCamera / desiredDistance;
		ray.length = desiredDistance + collisionRadius_;

		// 一番手前の遮蔽物を探す。
		// Input::RaycastMultipleAABB は当たり判定に単位立方体を使うので、
		// ここではオブジェクトごとの実際のローカル境界を渡して判定する
		float nearestDistance = ray.length;
		bool blocked = false;
		for (BaseObject* pObstacle : obstacles)
		{
			if (!pObstacle)
			{
				continue;
			}
			RayHitInfo hit{};
			if (Input::RayIntersectAABB(ray, pObstacle, hit, pObstacle->GetLocalBounds())
				&& hit.distance < nearestDistance)
			{
				nearestDistance = hit.distance;
				blocked = true;
			}
		}

		if (blocked)
		{
			// 当たった位置から半径と余裕のぶんだけ手前へ寄せる
			const float blockedDistance = nearestDistance - collisionRadius_ - collisionMargin_;
			const float actualDistance =
				std::clamp(blockedDistance, minCollisionDistance_, desiredDistance);
			position = target + ray.direction * actualDistance;
		}
	}

	// 地面へめり込むと床の裏が見えてしまうので、下限の高さでも止める
	const float minHeight = groundHeight_ + collisionRadius_ + collisionMargin_;
	if (position.y < minHeight)
	{
		position.y = minHeight;
	}

	return position;
}

void FollowCamera::ApplyToCamera()
{
	if (!pCamera_)
	{
		return;
	}

	// Forward を求めて、注視点からその逆方向へ距離ぶん下がった所がカメラ位置（仕様書 §3）
	const Vector3 forward = CalcForward(yaw_, pitch_);

	// 被弾の衝撃は「距離への上乗せ」と「位置のズレ」として足す。
	// 注視点は動かさないので、揺れているあいだも画面の中心はプレイヤーに残る
	float impactPullBack = 0.0f;
	Vector3 impactShake{};
	CalcImpact(impactPullBack, impactShake);

	// ダッシュの押し出しは逆に距離を詰める。衝撃と足し合わせるので、
	// 回避の直後に被弾しても打ち消し合うだけで暴れない
	const float dashPush = CalcDashPush();

	const Vector3 desiredPosition = target_ - forward * (distance_ + impactPullBack - dashPush) + impactShake;

	// 壁や地面にめり込むなら手前へ寄せる（仕様書 §15）
	const Vector3 position = ResolveCameraCollision(target_, desiredPosition);

	// 位置を決めて注視点を向く（行列の計算はカメラ側が行う）
	pCamera_->SetPosition(position);
	pCamera_->SetTarget(target_);
}

void FollowCamera::AddImpact(float strength)
{
	if (strength <= 0.0f)
	{
		return;
	}

	// 収まりかけているところへ次の被弾が来ても弱くならないよう、強いほうを採る。
	// 時間は必ず入れ直すので、連続で被弾すれば揺れは続く
	impactStrength_ = (std::max)(impactStrength_, strength);
	impactTimer_ = impactDuration_;
}

void FollowCamera::CalcImpact(float& outPullBack, Vector3& outShake) const
{
	outPullBack = 0.0f;
	outShake = Vector3{};

	if (impactTimer_ <= 0.0f || impactStrength_ <= 0.0f)
	{
		return;
	}

	const float duration = (std::max)(0.01f, impactDuration_);
	const float remain = std::clamp(impactTimer_ / duration, 0.0f, 1.0f);
	// 当たった瞬間が一番強く、後半ほど素早く収まる（二乗ぶんだけ余韻を短くする）
	const float scale = impactStrength_ * remain * remain;

	outPullBack = impactPullBack_ * scale;

	// 3軸で周期をずらして、規則的な往復に見えないようにする
	const float elapsed = duration - impactTimer_;
	const float amplitude = impactShakeAmount_ * scale;
	outShake = Vector3{
		std::sin(elapsed * impactShakeSpeed_) * amplitude,
		std::sin(elapsed * impactShakeSpeed_ * 1.3f + 1.7f) * amplitude * 0.7f,
		std::cos(elapsed * impactShakeSpeed_ * 0.9f) * amplitude * 0.5f,
	};
}

void FollowCamera::DrawDebugLines(const Vector3& playerTarget, const CameraFrameTarget& frame) const
{
	if (!debugDraw_ || !pCamera_)
	{
		return;
	}

	// 仕様書 §22 の「PlayerTarget ─ BossTarget ─ CameraTarget ─ Camera」を線で繋いで見せる
	LineRenderer* pLine = LineRenderer::GetInstance();
	const Vector3 cameraPosition = pCamera_->GetPosition();

	pLine->AddSphere(playerTarget, 0.25f, kColorPlayer);
	pLine->AddSphere(target_, 0.3f, kColorTarget);
	pLine->AddLine(target_, cameraPosition, kColorCamera);
	pLine->AddSphere(cameraPosition, collisionRadius_, kColorCamera);

	if (frame.valid)
	{
		pLine->AddSphere(frame.position, 0.35f, kColorBoss);
		// 収めたい大きさ。この球が画面に入っていれば、はみ出していない
		pLine->AddSphere(frame.position, frame.radius, kColorBoss);
		pLine->AddLine(playerTarget, frame.position, kColorPlayer);
		pLine->AddLine(playerTarget, target_, kColorTarget);
		pLine->AddLine(frame.position, target_, kColorTarget);
	}
	else
	{
		pLine->AddLine(playerTarget, target_, kColorTarget);
	}

	// 衝突で押し戻されたぶんを、遮蔽が無かった場合の位置まで赤い線で見せる
	const Vector3 forward = CalcForward(yaw_, pitch_);
	const Vector3 desiredPosition = target_ - forward * distance_;
	if ((desiredPosition - cameraPosition).LengthSq() > 0.0001f)
	{
		pLine->AddLine(cameraPosition, desiredPosition, kColorBlocked);
	}
}

float FollowCamera::GetTargetSmoothTime() const
{
	switch (mode_)
	{
	case CameraMode::BossBattle:
		return bossConfig_.targetSmoothTime;
	case CameraMode::Defeat:
		return defeatTargetSmoothTime_;
	default:
		return normalTargetSmoothTime_;
	}
}

float FollowCamera::GetDistanceSmoothTime() const
{
	switch (mode_)
	{
	case CameraMode::BossBattle:
		return bossConfig_.distanceSmoothTime;
	case CameraMode::Defeat:
		return defeatDistanceSmoothTime_;
	default:
		return normalDistanceSmoothTime_;
	}
}

void FollowCamera::DrawImGui()
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("追従カメラ##followcamera"))
	{
		return; // 折りたたみ中は中身を描かない
	}

	// 別のカメラを見ている状態から戻ってくる用
	if (ImGui::Button("このカメラに切り替え##followactivate"))
	{
		Activate();
	}
	ImGui::SameLine();
	if (ImGui::Button("正面に戻す##followresetview"))
	{
		ResetView();
	}
	ImGui::SameLine();
	if (ImGui::Button("構図を合わせ直す##followsnap"))
	{
		SnapToTarget();
	}

	// 仕様書 §16 のカメラモード
	ImGui::SeparatorText("カメラモード");
	int modeIndex = static_cast<int>(mode_);
	if (ImGui::Combo("モード##followmode", &modeIndex,
		"通常(プレイヤー追従)\0ボス戦(2人を画面に収める)\0やられ(プレイヤーへ寄る)\0"))
	{
		SetMode(static_cast<CameraMode>(modeIndex));
	}

	// 仕様書 §24 の調整順に沿って並べてある
	ImGui::SeparatorText("1. 画角 (§10)");
	if (ImGui::DragFloat("FOV(度)##followfov", &fovDegrees_, 0.5f, 30.0f, 110.0f, "%.1f") && pCamera_)
	{
		pCamera_->SetFovYDegrees(fovDegrees_);
	}
	ImGui::SetItemTooltip("まず極端な広角・望遠になっていないか確認する。基本は距離で合わせる");

	ImGui::SeparatorText("2. 注視点 (§5)");
	ImGui::DragFloat3("プレイヤーの注視点ずらし##followplayeroffset", &playerTargetOffset_.x, 0.05f, -10.0f, 10.0f, "%.2f");
	ImGui::SetItemTooltip("足元ではなく胸〜頭あたり(1.0〜1.5)を見るためのずらし");

	ImGui::SeparatorText("3. 通常モードの距離 (§23 Phase1)");
	ImGui::DragFloat("カメラ距離##follownormaldistance", &normalDistance_, 0.1f, 1.0f, 50.0f, "%.2f");
	ImGui::DragFloat("注視点の追従時間(秒)##follownormaltargetsmooth", &normalTargetSmoothTime_, 0.01f, 0.0f, 2.0f, "%.3f");
	ImGui::DragFloat("距離の追従時間(秒)##follownormaldistsmooth", &normalDistanceSmoothTime_, 0.01f, 0.0f, 2.0f, "%.3f");

	ImGui::SeparatorText("やられたときの寄り");
	ImGui::DragFloat("カメラ距離##followdefeatdistance", &defeatDistance_, 0.1f, 0.5f, 30.0f, "%.2f");
	ImGui::SetItemTooltip("やられたプレイヤーへ寄りきったときの距離。震えとはじけがはっきり見える近さにする");
	ImGui::DragFloat("注視点の高さ##followdefeatheight", &defeatTargetHeight_, 0.05f, -5.0f, 10.0f, "%.2f");
	ImGui::DragFloat("注視点の追従時間(秒)##followdefeattargetsmooth", &defeatTargetSmoothTime_, 0.01f, 0.0f, 3.0f, "%.3f");
	ImGui::DragFloat("距離の追従時間(秒)##followdefeatdistsmooth", &defeatDistanceSmoothTime_, 0.01f, 0.0f, 3.0f, "%.3f");
	ImGui::SetItemTooltip("長いほどゆっくり寄る。短くすると画面が飛んだように見える");

	if (ImGui::TreeNodeEx("4. ボス戦のフレーミング (§6〜§9,§19)##followbossconfig", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat("プレイヤー寄せ##followplayerweight", &bossConfig_.playerWeight, 0.01f, 0.0f, 1.0f, "%.2f");
		ImGui::SetItemTooltip("1.0でプレイヤーの真上、0.5で完全な中間点。0.6前後が目安（ボス側の重みは自動で 1-この値）");
		ImGui::DragFloat3("ボスの注視点ずらし##followbossoffset", &bossConfig_.bossTargetOffset.x, 0.05f, -20.0f, 20.0f, "%.2f");
		ImGui::SetItemTooltip("ボスの中心から少し上を見たいときに使う");

		ImGui::DragFloat("距離倍率##followdistancescale", &bossConfig_.distanceScale, 0.01f, 0.0f, 3.0f, "%.2f");
		ImGui::SetItemTooltip("プレイヤーとボスの距離を、そのままカメラ距離へどれだけ反映するか(0.8〜1.2)");
		ImGui::DragFloat("ボスの大きさ倍率##followbossradiusscale", &bossConfig_.bossRadiusScale, 0.01f, 0.0f, 3.0f, "%.2f");
		ImGui::SetItemTooltip("大きいボスほどカメラを引く。脚が広がる形態では上げる");
		ImGui::DragFloat("画面の余白##followframingoffset", &bossConfig_.framingOffset, 0.1f, 0.0f, 20.0f, "%.2f");
		ImGui::SetItemTooltip("2人の周りに残す余白(2.0〜4.0)");

		float distanceRange[2] = { bossConfig_.minDistance, bossConfig_.maxDistance };
		if (ImGui::DragFloat2("距離の下限・上限##followdistancerange", distanceRange, 0.1f, 0.5f, 100.0f, "%.2f"))
		{
			bossConfig_.minDistance = (std::min)(distanceRange[0], distanceRange[1]);
			bossConfig_.maxDistance = (std::max)(distanceRange[0], distanceRange[1]);
		}

		ImGui::DragFloat("注視点の追従時間(秒)##followbosstargetsmooth", &bossConfig_.targetSmoothTime, 0.01f, 0.0f, 2.0f, "%.3f");
		ImGui::SetItemTooltip("0.10〜0.20 が目安。大きいほどカメラが重くなる");
		ImGui::DragFloat("距離の追従時間(秒)##followbossdistsmooth", &bossConfig_.distanceSmoothTime, 0.01f, 0.0f, 2.0f, "%.3f");
		ImGui::SetItemTooltip("0.15〜0.30 が目安。小さすぎると距離変化がガタつく");
		ImGui::TreePop();
	}

	if (ImGui::TreeNodeEx("5. 視点操作 (§11,§12)##followrotate"))
	{
		ImGui::DragFloat("左右の速さ(度/秒)##followyawspeed", &yawSpeedDegrees_, 1.0f, 0.0f, 720.0f, "%.1f");
		ImGui::DragFloat("上下の速さ(度/秒)##followpitchspeed", &pitchSpeedDegrees_, 1.0f, 0.0f, 720.0f, "%.1f");

		float pitchLimitDegrees[2] = { pitchMinDegrees_, pitchMaxDegrees_ };
		if (ImGui::DragFloat2("上下の限界(度)##followpitchlimit", pitchLimitDegrees, 1.0f, -89.0f, 89.0f, "%.1f"))
		{
			pitchMinDegrees_ = (std::min)(pitchLimitDegrees[0], pitchLimitDegrees[1]);
			pitchMaxDegrees_ = (std::max)(pitchLimitDegrees[0], pitchLimitDegrees[1]);
		}
		ImGui::SetItemTooltip("左が見上げる限界(-20度)、右が見下ろす限界(45度)");

		ImGui::DragFloat("初期の見下ろし角(度)##followdefaultpitch", &defaultPitchDegrees_, 0.5f, -89.0f, 89.0f, "%.1f");
		ImGui::SetItemTooltip("10〜20度が目安。「正面に戻す」で戻る向き");

		ImGui::DragFloat("視点の追いつく速さ##followrotatesmooth", &rotateSmoothRate_, 0.1f, 0.0f, 60.0f, "%.2f");
		ImGui::SetItemTooltip("0 で入力に即時追従（推奨）。上げると視点操作に遅れが出る");
		ImGui::TreePop();
	}

	if (ImGui::TreeNodeEx("6. ボス方向への自動補正 (§14)##followautoalign"))
	{
		ImGui::Checkbox("自動補正を使う##followautoalignenabled", &autoAlignEnabled_);
		ImGui::DragFloat("補正しない角度(度)##followautoaligndead", &autoAlignDeadDegrees_, 1.0f, 0.0f, 180.0f, "%.1f");
		ImGui::SetItemTooltip("ボスがこの角度以内なら画面中央付近とみなして何もしない");
		ImGui::DragFloat("最大に補正する角度(度)##followautoalignfull", &autoAlignFullDegrees_, 1.0f, 0.0f, 180.0f, "%.1f");
		ImGui::SetItemTooltip("これを超えたら画面外とみなして一番強く引き戻す");
		ImGui::DragFloat("補正の速さ(度/秒)##followautoalignspeed", &autoAlignSpeedDegrees_, 1.0f, 0.0f, 360.0f, "%.1f");
		ImGui::SetItemTooltip("上げすぎるとロックオンのようになり、手動操作を奪ってしまう");
		ImGui::TreePop();
	}

	if (ImGui::TreeNodeEx("7. カメラ衝突 (§15)##followcollision"))
	{
		ImGui::Checkbox("衝突を処理する##followcollisionenabled", &collisionEnabled_);
		ImGui::DragFloat("カメラの太さ##followcollisionradius", &collisionRadius_, 0.01f, 0.0f, 2.0f, "%.2f");
		ImGui::SetItemTooltip("0.2〜0.4 が目安。壁からこれだけ離す");
		ImGui::DragFloat("余裕##followcollisionmargin", &collisionMargin_, 0.01f, 0.0f, 2.0f, "%.2f");
		ImGui::SetItemTooltip("0.1〜0.2 が目安。当たった位置からさらに手前へ寄せる");
		ImGui::DragFloat("最小の寄り##followmincollisiondistance", &minCollisionDistance_, 0.05f, 0.1f, 10.0f, "%.2f");
		ImGui::SetItemTooltip("これ以上は寄らない。小さすぎると注視点の中へ入り込む");
		ImGui::DragFloat("地面の高さ##followgroundheight", &groundHeight_, 0.05f, -50.0f, 50.0f, "%.2f");
		ImGui::SetItemTooltip("カメラをこの高さより下へ下ろさない（床の裏が見えるのを防ぐ）");
		ImGui::TreePop();
	}

	// 仕様書 §22 のデバッグ表示
	ImGui::SeparatorText("状態 (§22)");
	ImGui::Checkbox("デバッグ線を表示##followdebugdraw", &debugDraw_);
	ImGui::SetItemTooltip("プレイヤー・ボス・注視点・カメラの関係を線で表示する");
	ImGui::Text("ヨー角: %.1f 度 / ピッチ角: %.1f 度", radiansToDegrees(yaw_), radiansToDegrees(pitch_));
	ImGui::Text("カメラ距離: %.2f", distance_);
	ImGui::Text("注視点: (%.2f, %.2f, %.2f)", target_.x, target_.y, target_.z);
	if (pTarget_)
	{
		const Vector3 playerTarget = CalcPlayerTarget();
		ImGui::Text("プレイヤー注視点: (%.2f, %.2f, %.2f)", playerTarget.x, playerTarget.y, playerTarget.z);
		const CameraFrameTarget frame = CalcFrameTarget();
		if (frame.valid)
		{
			ImGui::Text("ボス注視点: (%.2f, %.2f, %.2f) 半径 %.2f",
				frame.position.x, frame.position.y, frame.position.z, frame.radius);
			ImGui::Text("プレイヤー-ボス距離: %.2f", (frame.position - playerTarget).Length());
		}
		else
		{
			ImGui::TextUnformatted("ボス注視点: なし（プレイヤー追従のみ）");
		}
	}
	if (pCamera_)
	{
		const Vector3 cameraPosition = pCamera_->GetPosition();
		ImGui::Text("カメラ位置: (%.2f, %.2f, %.2f)", cameraPosition.x, cameraPosition.y, cameraPosition.z);
	}

	ImGui::Separator();
	if (ImGui::Button("保存##followsave"))
	{
		Save();
	}
	ImGui::SameLine();
	if (ImGui::Button("読み込み##followload"))
	{
		Load();
	}

	// 画角やクリップ距離を触りたいとき用（位置と回転は追従で毎フレーム上書きされる）
	if (pCamera_ && ImGui::TreeNode("カメラ本体の設定##followcamerabody"))
	{
		pCamera_->DrawImGui();
		ImGui::TreePop();
	}
#endif // USE_IMGUI
}

void FollowCamera::Save()
{
	if (!pCamera_)
	{
		return;
	}

	// デストラクタでファイルへ書き出される
	DataHandler data("FollowCamera", pCamera_->GetName());

	// オフセットを直接持っていた頃のキー。今は使わないので、保存のついでに掃除しておく
	for (const char* legacyKey : { "distanceFromTarget", "heightOffset", "lookAtHeightOffset",
		"rotateSpeed", "offset", "lookAtOffset", "followSmoothRate" })
	{
		data.Remove(legacyKey);
	}

	data.Save("playerTargetOffset", playerTargetOffset_);
	data.Save("normalDistance", normalDistance_);
	data.Save("normalTargetSmoothTime", normalTargetSmoothTime_);
	data.Save("normalDistanceSmoothTime", normalDistanceSmoothTime_);

	data.Save("defeatDistance", defeatDistance_);
	data.Save("defeatTargetHeight", defeatTargetHeight_);
	data.Save("defeatTargetSmoothTime", defeatTargetSmoothTime_);
	data.Save("defeatDistanceSmoothTime", defeatDistanceSmoothTime_);

	data.Save("bossPlayerWeight", bossConfig_.playerWeight);
	data.Save("bossDistanceScale", bossConfig_.distanceScale);
	data.Save("bossRadiusScale", bossConfig_.bossRadiusScale);
	data.Save("bossFramingOffset", bossConfig_.framingOffset);
	data.Save("bossMinDistance", bossConfig_.minDistance);
	data.Save("bossMaxDistance", bossConfig_.maxDistance);
	data.Save("bossTargetSmoothTime", bossConfig_.targetSmoothTime);
	data.Save("bossDistanceSmoothTime", bossConfig_.distanceSmoothTime);
	data.Save("bossTargetOffset", bossConfig_.bossTargetOffset);

	data.Save("yawSpeedDegrees", yawSpeedDegrees_);
	data.Save("pitchSpeedDegrees", pitchSpeedDegrees_);
	data.Save("pitchMinDegrees", pitchMinDegrees_);
	data.Save("pitchMaxDegrees", pitchMaxDegrees_);
	data.Save("defaultPitchDegrees", defaultPitchDegrees_);
	data.Save("rotateSmoothRate", rotateSmoothRate_);

	data.Save("autoAlignEnabled", autoAlignEnabled_);
	data.Save("autoAlignDeadDegrees", autoAlignDeadDegrees_);
	data.Save("autoAlignFullDegrees", autoAlignFullDegrees_);
	data.Save("autoAlignSpeedDegrees", autoAlignSpeedDegrees_);

	data.Save("impactDuration", impactDuration_);
	data.Save("impactPullBack", impactPullBack_);
	data.Save("impactShakeAmount", impactShakeAmount_);
	data.Save("impactShakeSpeed", impactShakeSpeed_);

	data.Save("collisionEnabled", collisionEnabled_);
	data.Save("collisionRadius", collisionRadius_);
	data.Save("collisionMargin", collisionMargin_);
	data.Save("minCollisionDistance", minCollisionDistance_);
	data.Save("groundHeight", groundHeight_);

	data.Save("fovDegrees", fovDegrees_);
}

void FollowCamera::Load()
{
	if (!pCamera_)
	{
		return;
	}

	// キーが無い場合は今の値がそのまま返るので、未保存でも既定値のまま動く
	DataHandler data("FollowCamera", pCamera_->GetName());

	playerTargetOffset_ = data.Load("playerTargetOffset", playerTargetOffset_);
	normalDistance_ = data.Load("normalDistance", normalDistance_);
	normalTargetSmoothTime_ = data.Load("normalTargetSmoothTime", normalTargetSmoothTime_);
	normalDistanceSmoothTime_ = data.Load("normalDistanceSmoothTime", normalDistanceSmoothTime_);

	defeatDistance_ = data.Load("defeatDistance", defeatDistance_);
	defeatTargetHeight_ = data.Load("defeatTargetHeight", defeatTargetHeight_);
	defeatTargetSmoothTime_ = data.Load("defeatTargetSmoothTime", defeatTargetSmoothTime_);
	defeatDistanceSmoothTime_ = data.Load("defeatDistanceSmoothTime", defeatDistanceSmoothTime_);

	bossConfig_.playerWeight = data.Load("bossPlayerWeight", bossConfig_.playerWeight);
	bossConfig_.distanceScale = data.Load("bossDistanceScale", bossConfig_.distanceScale);
	bossConfig_.bossRadiusScale = data.Load("bossRadiusScale", bossConfig_.bossRadiusScale);
	bossConfig_.framingOffset = data.Load("bossFramingOffset", bossConfig_.framingOffset);
	bossConfig_.minDistance = data.Load("bossMinDistance", bossConfig_.minDistance);
	bossConfig_.maxDistance = data.Load("bossMaxDistance", bossConfig_.maxDistance);
	bossConfig_.targetSmoothTime = data.Load("bossTargetSmoothTime", bossConfig_.targetSmoothTime);
	bossConfig_.distanceSmoothTime = data.Load("bossDistanceSmoothTime", bossConfig_.distanceSmoothTime);
	bossConfig_.bossTargetOffset = data.Load("bossTargetOffset", bossConfig_.bossTargetOffset);

	yawSpeedDegrees_ = data.Load("yawSpeedDegrees", yawSpeedDegrees_);
	pitchSpeedDegrees_ = data.Load("pitchSpeedDegrees", pitchSpeedDegrees_);
	pitchMinDegrees_ = data.Load("pitchMinDegrees", pitchMinDegrees_);
	pitchMaxDegrees_ = data.Load("pitchMaxDegrees", pitchMaxDegrees_);
	defaultPitchDegrees_ = data.Load("defaultPitchDegrees", defaultPitchDegrees_);
	rotateSmoothRate_ = data.Load("rotateSmoothRate", rotateSmoothRate_);

	autoAlignEnabled_ = data.Load("autoAlignEnabled", autoAlignEnabled_);
	autoAlignDeadDegrees_ = data.Load("autoAlignDeadDegrees", autoAlignDeadDegrees_);
	autoAlignFullDegrees_ = data.Load("autoAlignFullDegrees", autoAlignFullDegrees_);
	autoAlignSpeedDegrees_ = data.Load("autoAlignSpeedDegrees", autoAlignSpeedDegrees_);

	impactDuration_ = data.Load("impactDuration", impactDuration_);
	impactPullBack_ = data.Load("impactPullBack", impactPullBack_);
	impactShakeAmount_ = data.Load("impactShakeAmount", impactShakeAmount_);
	impactShakeSpeed_ = data.Load("impactShakeSpeed", impactShakeSpeed_);

	collisionEnabled_ = data.Load("collisionEnabled", collisionEnabled_);
	collisionRadius_ = data.Load("collisionRadius", collisionRadius_);
	collisionMargin_ = data.Load("collisionMargin", collisionMargin_);
	minCollisionDistance_ = data.Load("minCollisionDistance", minCollisionDistance_);
	groundHeight_ = data.Load("groundHeight", groundHeight_);

	fovDegrees_ = data.Load("fovDegrees", fovDegrees_);
	pCamera_->SetFovYDegrees(fovDegrees_);
}

void FollowCamera::AddDashPush(float strength)
{
	if (strength <= 0.0f)
	{
		return;
	}

	// 連続で回避したときに弱くならないよう、衝撃と同じく強いほうを採る
	dashPushStrength_ = (std::max)(dashPushStrength_, strength);
	dashPushTimer_ = dashPushDuration_;
}

float FollowCamera::CalcDashPush() const
{
	if (dashPushTimer_ <= 0.0f || dashPushStrength_ <= 0.0f)
	{
		return 0.0f;
	}

	const float duration = (std::max)(0.01f, dashPushDuration_);
	const float remain = std::clamp(dashPushTimer_ / duration, 0.0f, 1.0f);
	// 飛び出した瞬間が一番前に出ていて、そこからばねが戻るように素早く元へ戻る
	return dashPushDistance_ * dashPushStrength_ * remain * remain;
}
