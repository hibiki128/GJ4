#pragma once
#include "Math/type/Vector3.h"
#include "src/Character/Player/States/Base/PlayerStateBase.h"

/// <summary>
/// 被弾ステート。
///
/// 短いあいだ操作を受け付けず、潰れながら被弾方向の反対へ少し押される。
/// 点滅・ヒットストップといった演出はここへ足していく。
/// 被弾した位置は context.healthComponent_->GetLastHitPoint() から引けるので、
/// 吹き飛ぶ向きはそこから作っている。
///
/// このステートへ入るのは Player::Update の1か所だけ（被弾を拾った瞬間）。
/// ボスの更新中に直接切り替えないのは、更新の順番で挙動が変わらないようにするため。
/// カメラの衝撃と画面の赤いマスクはプレイヤーの仕事ではないので、
/// Player::SetOnDamaged の通知を受けてシーンが出す。
/// </summary>
class PlayerStateDamaged : public PlayerStateBase {
public:
	PlayerStateDamaged() = default;
	~PlayerStateDamaged() = default;
	void RegisterParams() override;
	void Enter(Player& player, PlayerContext& context) override;
	void Update(Player& player, PlayerContext& context) override;
	void Exit(Player& player, PlayerContext& context) override;

private:
	/// <summary>押される向きを決める（被弾位置の反対側。分からなければ見ている向きの後ろ）</summary>
	Hagine::Vector3 CalcKnockbackDirection(const PlayerContext& context) const;

	/// <summary>反動で後ろへ下がる（押される総量は時間で決まる）</summary>
	void UpdateKnockback(PlayerContext& context);

	// --- 再生状態 ---
	float elapsed_ = 0.0f;                   // このステートに入ってからの経過時間（秒）
	Hagine::Vector3 knockbackDirection_{};   // 押される向き（水平）
	float knockbackMoved_ = 0.0f;            // すでに押されたぶんの距離

	// --- 調整パラメータ ---
	float kStunDuration = 0.35f;      // 操作を受け付けない時間（秒）
	float kSquashStrength = 0.8f;     // 被弾したときの潰れの強さ（0〜1）
	float kKnockbackDistance = 0.8f;  // 後ろへ押される距離
	float kKnockbackTime = 0.1f;      // 押されきるまでの時間（秒）
};
