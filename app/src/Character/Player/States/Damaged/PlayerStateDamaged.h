#pragma once
#include "src/Character/Player/States/Base/PlayerStateBase.h"

/// <summary>
/// 被弾ステート。
///
/// いまは「短いあいだ操作を受け付けず、潰れてから Idle へ戻る」だけの土台で、
/// のけぞり・点滅・ノックバック・ヒットストップといった演出はここへ足していく。
/// 被弾した位置は context.healthComponent_->GetLastHitPoint() から引けるので、
/// 吹き飛ぶ向きはそこから作れる。
///
/// このステートへ入るのは Player::Update の1か所だけ（被弾を拾った瞬間）。
/// ボスの更新中に直接切り替えないのは、更新の順番で挙動が変わらないようにするため。
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
	// --- 再生状態 ---
	float elapsed_ = 0.0f; // このステートに入ってからの経過時間（秒）

	// --- 調整パラメータ ---
	float kStunDuration = 0.35f;  // 操作を受け付けない時間（秒）
	float kSquashStrength = 0.8f; // 被弾したときの潰れの強さ（0〜1）
};
