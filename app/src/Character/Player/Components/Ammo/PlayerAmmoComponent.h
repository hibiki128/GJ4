#pragma once
#include "src/Character/ColorStruct.h"
#include <vector>

/// <summary>
/// プレイヤーの残弾を一手に引き受けるコンポーネント。
///
/// 弾は撃つと減り、時間で自動回復する。回復速度を上げるギミックを後から足せるよう、
/// 倍率は「設定する」のではなく「要求する」形にしてある（RequestRegenScale）。
/// 設定式だとギミックから離れたときに誰が 1.0 へ戻すのかという後始末が要るうえ、
/// ギミックが2つ重なると片方を抜けた瞬間にもう片方の効果まで消えてしまう。
/// 要求式なら効かせたい間だけ毎フレーム呼べばよく、重複も自然に解ける。
/// PlayerHealthComponent と同じく、更新の順番で結果が変わらないようにするための作り。
///
/// 弾数はいまのところ色をまたいだ1本のプールで、公開する口だけ Color を受け取る形にしてある。
/// 色別に分けたくなってもこのクラスの中と表示側だけの変更で済み、撃つ側は触らずにすむ。
/// </summary>
class PlayerAmmoComponent {
public:
	/// <summary>残弾の調整値（デバッグUIから触る）</summary>
	struct Params {
		int maxAmmo = 30;                 // 最大弾数
		int costPerShot = 1;              // 1発で減る量
		float regenPerSecond = 2.0f;      // 何も効いていないときの回復速度（発/秒）
		float regenDelayAfterShot = 0.3f; // 撃った後、回復が再開するまでの間（秒）
	};

	// 弾を満タンにする（Player::Init から呼ぶ。RegisterParams の後にすること）
	void Init();

	// 調整パラメータをデバッグUIへ登録する（Player::Init から一度だけ呼ぶ）
	void RegisterParams();

	// 回復を進める（毎フレーム）
	void Update();

	/// <summary>撃てるだけの残弾があるか</summary>
	bool CanFire(Color color) const;

	/// <summary>
	/// 1発ぶん消費する。撃てるときだけ減らすので、戻り値をそのまま発射の可否に使える
	/// </summary>
	/// <param name="color">撃つ色（いまは単一プールなので結果は変わらない）</param>
	/// <returns>bool: 消費できれば true（弾切れなら false）</returns>
	bool TryConsume(Color color);

	/// <summary>消費を取り消す（発射に失敗したときの払い戻し）</summary>
	void Refund(Color color);

	// 弾を満タンに戻す（リトライやデバッグ用）
	void Reset() { Init(); }

	/// ===================================================
	/// 回復速度を変えるギミックの導線
	/// ===================================================

	/// <summary>
	/// この1フレームだけ回復倍率を要求する。乗っている間だけ速い床のような
	/// 継続型のギミックが毎フレーム呼ぶ。離れれば呼ばれなくなるので自動で元に戻る
	/// </summary>
	/// <param name="scale">回復速度の倍率（2.0f で倍速）</param>
	void RequestRegenScale(float scale);

	/// <summary>
	/// 一定時間だけ効く回復倍率を足す。拾ったらしばらく速いアイテムのような
	/// 時限型のギミックが1回だけ呼ぶ
	/// </summary>
	/// <param name="scale">回復速度の倍率（2.0f で倍速）</param>
	/// <param name="duration">効いている時間（秒）</param>
	void AddRegenBoost(float scale, float duration);

	/// <summary>効いている倍率をすべて捨てる（リトライ時など）</summary>
	void ClearRegenBoosts();

	int GetAmmo(Color color) const;
	int GetMaxAmmo() const { return params_.maxAmmo; }
	// 残量の割合（0〜1）。弾数ゲージの表示に使う
	float GetRatio(Color color) const;
	bool IsEmpty(Color color) const { return GetAmmo(color) <= 0; }
	// いま実際に効いている回復速度（発/秒）。表示と確認に使う
	float GetEffectiveRegenPerSecond() const;
	// いま効いている回復倍率（1.0 なら素の速さ）
	float GetRegenScale() const;
	// 撃った直後の回復待ちの残り（0 なら回復中）
	float GetRegenDelayTimer() const { return regenDelayTimer_; }

	// 残弾の状態を表示する（シーンの「オブジェクト設定」窓から呼ぶ）
	void DrawImGui();

private:
	/// <summary>時限型の回復倍率</summary>
	struct RegenBoost {
		float scale = 1.0f;  // 回復速度の倍率
		float remain = 0.0f; // 残り時間（秒）
	};

	// --- 調整パラメータ ---
	Params params_{};

	// --- 状態 ---
	int ammo_ = 0;
	// 回復は毎秒の実数、弾数は整数なので、1発に満たない端数をここに貯める
	float regenAccumulator_ = 0.0f;
	float regenDelayTimer_ = 0.0f;  // 撃った後の回復待ちの残り（秒）
	float frameScaleRequest_ = 1.0f; // 継続型の要求（Update の最後で 1.0 に戻す）
	std::vector<RegenBoost> boosts_; // 時限型の倍率
};
