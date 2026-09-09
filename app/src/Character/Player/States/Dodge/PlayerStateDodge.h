#pragma once
#include "Math/type/Vector3.h"
#include "src/Character/Player/States/Base/PlayerStateBase.h"

/// <summary>
/// 回避ステート。
///
/// 入ってすぐ無敵になり、入った瞬間に入力していた向きへ飛び出す。
/// 最初のわずかな時間は最高速のまま進み、そこからダッシュの速度まで落ちていって、
/// 落ちきったところでダッシュへ移る。終わりの速度をダッシュに揃えてあるので、
/// ステートが切り替わっても速度は繋がったまま走り出す。
///
/// 飛び出す向きは Enter で一度だけ決めてワールドの向きで持つ。
/// 入力の向きのまま持ち回すと PlayerMoveComponent::Move が毎フレーム
/// カメラの向きへ回してしまい、回避の途中でカメラを動かすと軌道が曲がるため。
///
/// 無敵そのものは持たず、PlayerHealthComponent へ時間を預けるだけにしてある。
/// ダメージが通るかを決める場所を ApplyDamage の1か所に保つため。
/// 無敵が効いている間は被弾しても体力が減らないので、Player::Update が
/// 被弾ステートへ切り替えることもなく、回避は最後まで再生される。
/// </summary>
class PlayerStateDodge : public PlayerStateBase {
public:
	PlayerStateDodge() = default;
	~PlayerStateDodge() = default;
	void RegisterParams() override;
	void Enter(Player& player, PlayerContext& context) override;
	void Update(Player& player, PlayerContext& context) override;
	void Exit(Player& player, PlayerContext& context) override;

private:
	/// <summary>飛び出す向きを決める（入力があればその向き、無ければ見ている向き）</summary>
	Hagine::Vector3 CalcDodgeDirection(const PlayerContext& context) const;

	/// <summary>いまの速度（最初は最高速のまま、そこからダッシュの速度まで落ちていく）</summary>
	float CalcSpeed() const;

	// --- 再生状態 ---
	float elapsed_ = 0.0f;               // このステートに入ってからの経過時間（秒）
	Hagine::Vector3 dodgeDirection_{};   // 飛び出す向き（水平・ワールド）

	// --- 調整パラメータ ---
	float kBurstTime = 0.06f;      // 最高速のまま飛び出す時間（秒）。60FPSなら約4フレーム
	float kDuration = 0.2f;        // ダッシュへ移るまでの時間（秒）
	float kMaxSpeed = 25.0f;       // 飛び出しの速度（1秒あたりに進む距離）
	float kEndSpeed = 15.0f;       // 回避の終わりの速度。ダッシュと揃えると繋ぎ目が消える
	float kInvincibleTime = 0.3f;  // 無敵でいる時間（秒）
	float kCooldown = 0.6f;        // 次の回避が出せるようになるまでの時間（秒）
	float kJustWindow = 0.15f;     // ジャスト回避として認める、回避の出だしの時間（秒）
};
