#pragma once
#include "Math/type/Vector3.h"
#include "src/Character/Player/Core/PlayerContext.h"

/// <summary>
/// プレイヤーの「ぷにぷに」を一手に引き受けるコンポーネント。
///
/// 各ステートが直接 scale_ を書くとステートを跨いだ演出（着地のぷにっ）が
/// 次のフレームで上書きされて消えてしまうので、スケールの所有権はここに集約する。
/// ステート側は「こうしたい」と要求を出すだけで、実際の合成と適用は
/// Player::Update から Update() → Apply() の順で1回だけ行う。
///
/// オフセットの符号は既存の Easing 関数と同じ規約:
///   offset > 0 … 縦に伸びて横が細い（ストレッチ）
///   offset < 0 … 平べったく潰れる（スクワッシュ）
/// </summary>
class PlayerComponentReaction {
public:
	Hagine::Vector3 SquashStretch(const Hagine::Vector3& scale, float easeT, float time, float amplitude, float period);

	Hagine::Vector3 LoopSquashStretch(const Hagine::Vector3& scale, float time, float amplitude, float period, float sharpness, float phase);

	// 調整パラメータをデバッグUIへ登録する（Player::Init から一度だけ呼ぶ）
	void RegisterParams();
	// 反応の更新
	void Update();
	// ループの設定（毎フレーム出す要求。出さなくなると自然に止まる）
	void SetLoop(float amp, float period, float sharpness, float phase);
	// 空中の細さ(目標値)（毎フレーム出す要求。出さなくなると自然に0へ戻る）
	void SetAirStretch(float target);          
	// 着地の一発(0〜1で強さ)
	void PlayLanding(float strength01);
	// 踏み切りの予備動作(縮んでから伸びる)。kUseTakeoff_ がオフなら何も起きない
	void PlayTakeoff();
	// 反応を適用したスケールを取得
	Hagine::Vector3 Apply(const Hagine::Vector3& baseScale) const;
private:
	// --- 再生状態 ---
	float loopTime_ = 0.0f;
	float loopAmp_ = 0.0f;        // 実際に効いているループの振幅（要求へ追従する）
	float loopAmpRequest_ = 0.0f; // このフレームに出されたループの要求
	float loopPeriod_ = 1.0f;
	float loopSharpness_ = 0.0f;
	float loopPhase_ = 0.0f;
	float airStretch_ = 0.0f; 
	float airTarget_ = 0.0f;
	float landTime_ = -1.0f;
	float landAmp_ = 0.0f;  // landTime_ < 0 なら非再生
	float takeoffTime_ = -1.0f; // takeoffTime_ < 0 なら非再生
	float sizePop_ = 0.0f; // 着地の「大きく」ぶん

	// --- 調整パラメータ ---
	float kLoopFollow = 10.0f;    // ループ振幅の追従の速さ（空中で呼吸が止まる速さ）
	float kAirFollow = 12.0f;     // 空中ストレッチの追従の速さ
	float kLandAmpMin = 0.15f;    // 着地の潰れ量（ぎりぎり落ちたとき）
	float kLandAmpMax = 0.45f;    // 着地の潰れ量（高いところから落ちたとき）
	float kLandDuration = 0.6f;   // 着地のぷるぷるが収まるまでの時間
	float kLandPeriod = 0.35f;    // 着地の揺れの細かさ（小さいほど速く震える）
	float kLandPop = 0.15f;       // 着地の瞬間に一様に大きくなる量
	float kPopDecay = 8.0f;       // 「大きく」ぶんが収まる速さ
	float kMaxOffset = 0.6f;      // 重ね掛けでスケールが負にならないための上限

	// --- 踏み切りの予備動作（丸ごとオン/オフできる） ---
	bool kUseTakeoff_ = true;      // false にすると PlayTakeoff() が何もしなくなる
	float kTakeoffAmp = 0.18f;     // 縮み／伸びの大きさ
	float kTakeoffDuration = 0.14f; // 縮んで伸びて戻るまでの時間
};
