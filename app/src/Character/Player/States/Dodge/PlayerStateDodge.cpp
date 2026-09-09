#include "PlayerStateDodge.h"
#include "Frame/Frame.h"
#include "Utility/Debug/Param/GameParamHub.h"
#include "src/Character/Player/Components/Health/PlayerHealthComponent.h"
#include "src/Character/Player/Components/Move/PlayerMoveComponent.h"
#include "src/Character/Player/Components/Reaction/PlayerComponentReaction.h"
#include "src/Character/Player/Effect/PlayerParticles.h"
#include "src/Character/Player/Core/PlayerContext.h"
#include "src/Character/Player/Player.h"
#include <MyMath.h>
#include <algorithm>

void PlayerStateDodge::RegisterParams() {
	const std::string paramOwnerLabel = "Player/Dodge";
	Hagine::GameParamHub* hub = Hagine::GameParamHub::GetInstance();

	hub->Register(paramOwnerLabel, "BurstTime", &kBurstTime, {0.01f, 0.0f, 1.0f});
	hub->Register(paramOwnerLabel, "Duration", &kDuration, {0.01f, 0.05f, 2.0f});
	hub->Register(paramOwnerLabel, "MaxSpeed", &kMaxSpeed, {0.5f, 0.0f, 100.0f});
	hub->Register(paramOwnerLabel, "EndSpeed", &kEndSpeed, {0.5f, 0.0f, 100.0f});
	hub->Register(paramOwnerLabel, "InvincibleTime", &kInvincibleTime, {0.01f, 0.0f, 2.0f});
	hub->Register(paramOwnerLabel, "Cooldown", &kCooldown, {0.01f, 0.0f, 3.0f});
	hub->Register(paramOwnerLabel, "JustWindow", &kJustWindow, {0.01f, 0.0f, 1.0f});
}

void PlayerStateDodge::Enter(Player& player, PlayerContext& context) {
	elapsed_ = 0.0f;
	dodgeDirection_ = CalcDodgeDirection(context);

	// 無敵の持ち主は体力コンポーネント。ここで時間を預けておけば、
	// 減らすのも被弾を弾くのも向こうがやってくれる
	if (context.healthComponent_) {
		context.healthComponent_->AddInvincible(kInvincibleTime);
	}

	// 次に回避できるようになるまでの待ち時間。回避ステートの外でも減らし続ける必要があるので、
	// タイマーは context が持ち、Player::Update が進める
	context.dodgeCooldown_ = kCooldown;

	// 出だしのこの時間に攻撃を無敵で弾けばジャスト回避。判定そのものは Player::Update が行う
	// （被弾はボスの更新の途中で届くので、拾うのは自分の更新の頭にそろえてある）
	context.dodgeJustTimer_ = kJustWindow;

	// 体の演出（溜め→進行方向へ伸び→揺り戻し）。スケールを直接書かず要求を出すだけなのは他のステートと同じ
	context.reactionComponent_->PlayDashBurst(dodgeDirection_);

	// カメラの押し出しなど画面まわりの演出はシーンが受け持つ。
	// プレイヤーはカメラを知らないので、被弾と同じく通知だけ投げる
	player.NotifyDodge(dodgeDirection_);

	// 後ろへ散るゼリー飛沫。粒の見た目はパーティクル側に任せて、ここは位置と向きと色だけ渡す
	if (context.transform_) {
		PlayerParticles::GetInstance()->BurstDodgeJelly(
			context.transform_->translation_, dodgeDirection_, player.GetDisplayColor());
	}
}

void PlayerStateDodge::Update(Player& player, PlayerContext& context) {
	elapsed_ += Hagine::Frame::DeltaTime();

	// 向きは Enter で決めたものを最後まで使う。距離を毎フレーム作って渡すので、
	// フレームレートが変わっても同じ速さで同じだけ進む
	context.moveComponent_->MoveWorld(context, dodgeDirection_, CalcSpeed() * Hagine::Frame::DeltaTime());

	// 回避中は移動もジャンプも受け付けない（避けきるまで操作を返さない）
	if (elapsed_ < kDuration) {
		return;
	}

	// 終わりの速度をダッシュに揃えてあるので、そのまま繋がって走り出す。
	// ダッシュのボタンを離していれば、Dash 側がすぐ Move へ落としてくれる
	player.ChangeState("Dash");
}

void PlayerStateDodge::Exit(Player& player, PlayerContext& context) {
}

Hagine::Vector3 PlayerStateDodge::CalcDodgeDirection(const PlayerContext& context) const {
	// 入力は「画面の上下左右」なので、Move と同じようにカメラの向きへ回してワールドの向きにする。
	// 斜め入力は長さが√2になるので、そのぶん速くならないよう先に正規化しておく
	const Hagine::Vector3 inputDirection = {context.input_.dir.x, 0.0f, context.input_.dir.y};
	if (inputDirection.LengthSq() > 0.0001f) {
		return Hagine::TransformNormal(
			inputDirection.Normalize(), Hagine::MakeRotateYMatrix(context.cameraYaw_));
	}

	// 倒していないフレームに入ったときは見ている向きへ飛ぶ（水平成分だけ使う）
	Hagine::Vector3 forward = {context.aimDirection_.x, 0.0f, context.aimDirection_.z};
	if (forward.LengthSq() <= 0.0001f) {
		// 真上や真下を向いていて水平の向きが出ないときの保険
		return Hagine::Vector3{0.0f, 0.0f, 1.0f};
	}
	return forward.Normalize();
}

float PlayerStateDodge::CalcSpeed() const {
	// 最初のわずかな時間は最高速のまま。ここが「飛び出した」手応えになる
	if (elapsed_ <= kBurstTime) {
		return kMaxSpeed;
	}

	const float t = std::clamp((elapsed_ - kBurstTime) / (std::max)(0.0001f, kDuration - kBurstTime), 0.0f, 1.0f);
	// 落ち始めが一番急で、終わりに近いほど緩やか。落ちきった先がダッシュの速度になる
	const float eased = 1.0f - (1.0f - t) * (1.0f - t);
	return kMaxSpeed + (kEndSpeed - kMaxSpeed) * eased;
}
