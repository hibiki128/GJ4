#pragma once
#include "Math/type/Vector3.h"
#include "src/Character/Player/States/Base/PlayerStateBase.h"

/// <summary>
/// やられステート。HPが0になったときに入り、そのまま抜けない終端のステート。
///
/// 流れは「震える → はじけ飛ぶ → 消えたまま待つ」の3段。
///   震え  : 体を細かく揺らしながら、揺れ幅と潰れを時間とともに大きくしていく
///   はじけ: 体を消して、粒を撒き散らす
///   余韻  : 何も出さずに待つ。この待ちが終わると IsFinished() が立つ
///
/// 揺れは描画オフセット（Player::SetRenderShake）で出す。座標そのものは動かさないので、
/// 重力・フィールドの押し戻し・当たり判定と喧嘩しない。
/// 潰れは反応コンポーネントへ要求を出すだけにしてある（スケールの書き手は Player::Update の1か所）。
///
/// シーンへの導線は Player::IsDefeatFinished()。
/// ゲームオーバーへ送る間合いは、この演出が終わってからシーン側が決める
/// </summary>
class PlayerStateDefeated : public PlayerStateBase {
public:
	PlayerStateDefeated() = default;
	~PlayerStateDefeated() = default;

	void RegisterParams() override;
	void Enter(Player& player, PlayerContext& context) override;
	void Update(Player& player, PlayerContext& context) override;
	void Exit(Player& player, PlayerContext& context) override;

	/// <summary>演出をやり切ったか（シーンはこれを見てゲームオーバーへ送る）</summary>
	bool IsFinished() const { return isFinished_; }

private:
	/// <summary>震えの段を1フレーム進める</summary>
	void UpdateShake(Player& player, PlayerContext& context);

	/// <summary>はじける瞬間の処理（体を消して粒を撒く。1回だけ）</summary>
	void Burst(Player& player, PlayerContext& context);

	// --- 再生状態 ---
	float elapsed_ = 0.0f;    // このステートに入ってからの経過時間（秒）
	bool hasBurst_ = false;   // すでに弾けたか
	bool isFinished_ = false; // 余韻まで終わったか

	// --- 調整パラメータ ---
	float kShakeDuration = 0.9f;  // 震えている時間（秒）。ここが過ぎると弾ける
	float kShakeAmplitude = 0.22f; // 震えの最大の振れ幅。終わり際がいちばん大きい
	float kShakeSpeed = 46.0f;    // 震えの速さ（大きいほど細かく震える）
	float kShakeSquash = 0.35f;   // 震えに合わせた潰れの最大の強さ
	float kShakePeriod = 0.09f;   // 潰れの周期（秒）。小さいほど速く伸び縮みする
	float kAfterglow = 0.5f;      // 弾けたあと、次のシーンへ送るまでの余韻（秒）
};
