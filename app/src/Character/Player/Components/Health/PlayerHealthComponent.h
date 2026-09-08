#pragma once
#include "src/Interface/IDamageable.h"
#include "type/Vector3.h"

/// <summary>
/// プレイヤーの体力を一手に引き受けるコンポーネント。
///
/// ダメージが届くのは相手（ボス）の更新の途中なので、ここでは体力を減らして
/// 「被弾した」という事実を積んでおくだけにしてある。演出とステートの切り替えは
/// プレイヤー自身の Update が ConsumeHit() で拾ってから行う。
/// PlayerJumpComponent::ConsumeLanded() と同じ考え方で、ボスとプレイヤーの
/// どちらが先に更新されても結果が変わらないようにするため。
/// </summary>
class PlayerHealthComponent {
public:
	/// <summary>体力の調整値（デバッグUIから触る）</summary>
	struct Params {
		int maxHp = 5;               // 最大HP
		int damagePerHit = 1;        // 1回の被弾で減る量
		float invincibleTime = 1.0f; // 被弾してから次の被弾を受け付けるまでの時間（秒）
	};

	// HPを満タンにする（Player::Init から呼ぶ。RegisterParams の後にすること）
	void Init();

	// 調整パラメータをデバッグUIへ登録する（Player::Init から一度だけ呼ぶ）
	void RegisterParams();

	// 無敵時間を進める（毎フレーム）
	void Update();

	/// <summary>
	/// ダメージを受ける。
	/// 減る量は damagePerHit 固定で、DamageInfo::amount（攻撃ごとの強さ）は見ない。
	/// 攻撃の種類で減り方を変えたくなったら、その変換はここ1か所で行う
	/// </summary>
	/// <param name="info">ダメージ情報（着弾位置は演出用に控える）</param>
	/// <returns>bool: 実際にHPが減れば true（無敵中・死亡後は false）</returns>
	bool ApplyDamage(const DamageInfo& info);

	// 被弾したことを消費する（1回の被弾につき1回だけ true を返す）
	bool ConsumeHit();

	// HPを満タンに戻す（リトライやデバッグ用）
	void Reset() { Init(); }

	int GetHp() const { return hp_; }
	int GetMaxHp() const { return params_.maxHp; }
	// 残量の割合（0〜1）。HPゲージの表示に使う
	float GetRatio() const;
	bool IsDead() const { return hp_ <= 0; }
	bool IsInvincible() const { return invincibleTimer_ > 0.0f; }
	// 無敵時間の残り（点滅などの演出に使う）
	float GetInvincibleTimer() const { return invincibleTimer_; }
	// 直近に受けたダメージ（被弾の通知に乗せて、演出側が着弾位置などを見るのに使う）
	const DamageInfo& GetLastDamage() const { return lastDamage_; }
	// 直近に被弾した位置（のけぞりやヒットエフェクトの向きに使う）
	const Hagine::Vector3& GetLastHitPoint() const { return lastDamage_.hitPoint; }

	// 体力の状態を表示する（シーンの「オブジェクト設定」窓から呼ぶ）
	void DrawImGui();

private:
	// --- 調整パラメータ ---
	Params params_{};

	// --- 状態 ---
	int hp_ = 0;
	float invincibleTimer_ = 0.0f;     // 無敵の残り時間（秒）
	bool hitPending_ = false;          // まだ拾われていない被弾があるか
	DamageInfo lastDamage_{};          // 直近に受けたダメージ（着弾位置は演出が使う）
};
