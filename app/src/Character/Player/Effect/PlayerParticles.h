#pragma once
#include "type/Vector3.h"
#include "type/Vector4.h"
#include <array>
#include <vector>

namespace Hagine {
	class ParticleCSEmitter;
}

/// <summary>
/// プレイヤーまわりの粒をまとめて受け持つ。
///
/// 作りは BossParticles と同じで、効果ひとつが Assets/jsons/ParticleCS/&lt;名前&gt;.json 1枚に対応する。
/// 見た目の調整はエンジンのエミッター編集UIをそのまま出しているので、
/// 「パーティクル設定」ウィンドウから触って保存すれば json に書き戻る。
///
/// ただし飛ぶ向きと色だけは出すたびにコードから入れ直すので、UIで触っても残らない。
/// 向きは入力で決まり、色は体の色に合わせたいという、どちらも json に書けない値のため。
///
/// ※ テンプレートを手で足すときは Assets/jsons/ParticleCSGroup/&lt;group_0_name&gt;.json も要る。
///    名前が登録簿に無いと粒子グループが素通りされ、エラーも出ないまま1粒も出なくなる
///    （エディタから作った場合は両方まとめて保存されるので気づかない）。
///
/// 実体の所有は ParticleCSSpawner 側。シーンを切り替えると捨てられるので、
/// シーンの初期化のたびに Init() を呼ぶこと（生きているぶんは出し直さない）。
/// </summary>
class PlayerParticles {
public:
	/// <summary>出せる効果</summary>
	enum class Id {
		DodgeJelly, // 回避で後ろへ散るゼリー飛沫
		Walk,       // 移動中（ダッシュ含む）に足元から出る粒
		Landing,    // 着地で上へ跳ね上がる粒
		PerfectRipple, // ジャスト回避で外へ広がる波紋
		PerfectBurst,  // ジャスト回避で外へ飛ぶゼリー粒
		Count
	};

	/// ===================================================
	/// public method
	/// ===================================================

	static PlayerParticles* GetInstance();

	/// <summary>テンプレートを読み込んでシーンへエミッターを出す（シーンの初期化から毎回呼ぶ）</summary>
	void Init();

	/// <summary>
	/// このフレームの要求を形にする（Player::Update から毎フレーム1回）。
	/// 歩きの粒は「出し続けたい」という要求が来たフレームだけ発生させる。
	/// 反応コンポーネントの SetLoop と同じ考え方で、要求が止まれば自然に消える
	/// </summary>
	void Update();

	/// <summary>
	/// 移動中であることを知らせる（移動・ダッシュのステートから毎フレーム）
	/// </summary>
	/// <param name="playerPosition">プレイヤーの位置（ワールド）。足元へ下ろすのはこちらで行う</param>
	/// <param name="bodyColor">体の色。粒を同系色にするのに使う</param>
	void RequestWalk(const Hagine::Vector3& playerPosition, const Hagine::Vector4& bodyColor);

	/// <summary>
	/// 回避のゼリー飛沫を1回出す。粒は飛び出した向きの後ろへ散る
	/// </summary>
	/// <param name="position">プレイヤーの位置（ワールド）</param>
	/// <param name="dodgeDirection">飛び出した向き（水平・正規化済み）</param>
	/// <param name="bodyColor">体の色。粒を同系色にするのに使う</param>
	void BurstDodgeJelly(const Hagine::Vector3& position, const Hagine::Vector3& dodgeDirection, const Hagine::Vector4& bodyColor);

	/// <summary>
	/// 着地の粒を1回出す。足元から上向きに飛び散る
	/// </summary>
	/// <param name="position">プレイヤーの位置（ワールド）。足元へ下ろすのはこちらで行う</param>
	/// <param name="strength01">強さ（0〜1）。高いところから落ちたときほど大きく跳ねる</param>
	/// <param name="bodyColor">体の色。粒を同系色にするのに使う</param>
	void BurstLanding(const Hagine::Vector3& position, float strength01, const Hagine::Vector4& bodyColor);

	/// <summary>
	/// ジャスト回避の粒を1回出す。波紋と飛沫をまとめて出す
	/// </summary>
	/// <param name="position">プレイヤーの位置（ワールド）</param>
	/// <param name="bodyColor">体の色。波紋も粒も同系色にする</param>
	void BurstPerfectDodge(const Hagine::Vector3& position, const Hagine::Vector4& bodyColor);

	/// <summary>調整UI（エンジンのエミッター編集をそのまま出す）</summary>
	void DrawImGui();

private:
	/// ===================================================
	/// private method
	/// ===================================================

	PlayerParticles() = default;
	~PlayerParticles() = default;
	PlayerParticles(const PlayerParticles&) = delete;
	PlayerParticles& operator=(const PlayerParticles&) = delete;

	/// <summary>効果ひとつぶんの持ち物</summary>
	struct Effect {
		// 続けて出したときに前のぶんを消してしまわないよう、必要な数だけ用意して順番に使う。
		// 実体は ParticleCSSpawner が持っているので、ここは参照するだけ
		std::vector<Hagine::ParticleCSEmitter*> emitters;
		size_t next = 0;               // 次に使うエミッター
		const char* templateName = ""; // json のファイル名
		const char* label = "";        // 調整UIでの表示名
		int burstCount = 0;            // これまでに出した回数（呼べているかの確認用）
	};

	/// <summary>効果を引く</summary>
	Effect& Get(Id id) { return effects_[static_cast<size_t>(id)]; }

	/// <summary>次に使うエミッターを1体引く（使えるものが無ければ nullptr）</summary>
	Hagine::ParticleCSEmitter* NextEmitter(Id id);

	/// <summary>json の値だけで1回出す（向きも色も上書きしない。原因の切り分け用）</summary>
	void BurstTemplateOnly(Id id, const Hagine::Vector3& position);

	/// <summary>最初の1回だけ、出した位置をログへ残す（出ているかを後から確かめる用）</summary>
	void LogFirstBurst(Id id, const Hagine::ParticleCSEmitter* emitter) const;

	/// ===================================================
	/// private variables
	/// ===================================================

	std::array<Effect, static_cast<size_t>(Id::Count)> effects_{};

	// 歩きの粒は「このフレームに要求が来たか」で出し続けるかを決める
	bool walkRequested_ = false;
	Hagine::Vector3 walkPosition_ = {0.0f, 0.0f, 0.0f};
	Hagine::Vector4 walkColor_ = {1.0f, 1.0f, 1.0f, 1.0f};
	bool walkEmitting_ = false; // いま出しっぱなしにしているか（切り替えのときだけ触る）

	// --- 調整UIの試し撃ち ---
	Hagine::Vector3 testPosition_ = {0.0f, 1.0f, 0.0f};    // 出す位置
	float testYaw_ = 0.0f;                                 // 飛び出す向き(ラジアン)
	Hagine::Vector4 testColor_ = {1.0f, 1.0f, 1.0f, 1.0f}; // 体の色として渡す値
};
