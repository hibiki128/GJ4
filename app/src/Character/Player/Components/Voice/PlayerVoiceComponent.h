#pragma once

/// <summary>
/// プレイヤーの「鳴らし続ける音」の間合いを受け持つ。
///
/// 立ち止まっているときのぽよぽよ、歩き・走りの足音のように、
/// 「その状態でいるあいだ、間を置いて鳴らし続ける」音をここでまとめている。
/// 被弾・回避・射撃のような一発ものは、起きた場所から GameSounds を直接鳴らすので通らない。
///
/// 使い方は PlayerParticles の足元の粒と同じ「毎フレーム要求を出す」形。
/// ステートは自分の状態に合った Request〜 を毎フレーム呼ぶだけでよく、
/// 要求が止まれば鳴るのも止まる。ステートを跨いだときに音が途切れたり
/// 二重に鳴ったりしないよう、間隔を数えるのはこのクラス1か所にしてある。
///
/// 実際に鳴らすのは Update（Player::Update から毎フレーム1回）。
/// ポーズ中は Player::Update ごと止まるので、止めている間は鳴らない
/// </summary>
class PlayerVoiceComponent {
public:
	/// <summary>調整パラメータをデバッグUIへ登録する（Player::Init から一度だけ）</summary>
	void RegisterParams();

	/// <summary>立ち止まっていることを知らせる（待機ステートから毎フレーム）</summary>
	void RequestIdle() { request_ = Kind::Idle; }

	/// <summary>歩いていることを知らせる（移動ステートから毎フレーム）</summary>
	void RequestMove() { request_ = Kind::Move; }

	/// <summary>走っていることを知らせる（ダッシュステートから毎フレーム）</summary>
	void RequestDash() { request_ = Kind::Dash; }

	/// <summary>
	/// このフレームの要求を形にする（Player::Update から毎フレーム1回）。
	/// 要求が来ていなければ何も鳴らさず、次に要求が来たときのために間隔を仕切り直す
	/// </summary>
	/// <param name="deltaTime">経過時間（秒）</param>
	void Update(float deltaTime);

private:
	/// <summary>いま出ている要求の種類</summary>
	enum class Kind {
		None, // 要求なし（空中・被弾中・やられた後など）
		Idle,
		Move,
		Dash,
	};

	/// <summary>その種類を鳴らす間隔（秒）</summary>
	float IntervalOf(Kind kind) const;

	/// <summary>その種類の音を1回鳴らす</summary>
	void PlayOnce(Kind kind);

	// --- 再生状態 ---
	Kind request_ = Kind::None; // このフレームに出された要求（Update の最後で消す）
	Kind current_ = Kind::None; // いま数えている種類
	float timer_ = 0.0f;        // 次に鳴らすまでの残り時間（秒）
	bool useIdle2_ = false;     // 次のぽよぽよはどちらを鳴らすか（交互に切り替える）

	// --- 調整パラメータ ---
	// 立ち止まっているときの間隔。長めに取らないと、待っているだけでうるさくなる
	float kIdleInterval = 1.8f;
	// 間隔のばらつき（秒）。毎回きっかり同じ間隔だと機械的に聞こえるので少し散らす
	float kIdleIntervalRandom = 0.6f;
	float kMoveInterval = 0.42f; // 歩きの足音の間隔
	float kDashInterval = 0.28f; // 走りの足音の間隔（歩きより速い足運びに合わせる）
};
