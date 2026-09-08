#pragma once
#include "2d/Sprite.h"
#include "type/Vector3.h"
#include <memory>

/// <summary>
/// 被弾したときに画面のふちが赤く染まるマスク。
///
/// 中心が透明・ふちほど濃いビネットのテクスチャ（UI/DamageVignette.png）を
/// 画面いっぱいに1枚だけ貼り、色と濃さをここで動かす。
/// テクスチャの色は白なので、赤みはスプライトの色で決まる（実行中に変えられる）。
///
/// ポストエフェクトではなくスプライトなのは、エンジンのビネット
/// （ShaderMode::Vignette）がセピア調に固定されていて赤くできないため。
/// UIの一番手前ではなくゲーム画面の上へ重ねるので、描くのは黒帯やポーズ画面より先。
/// </summary>
class DamageVignette {
public:
	/// <summary>スプライトを用意する（シーンの初期化から1回だけ呼ぶ）</summary>
	void Init();

	/// <summary>調整パラメータをデバッグUIへ登録する（Init の後に一度だけ呼ぶ）</summary>
	void RegisterParams();

	/// <summary>
	/// 赤いマスクを出す。再生中に呼び直せば、そこから出し直しになる
	/// </summary>
	/// <param name="strength">濃さの倍率（1.0 で調整値どおり）</param>
	void Play(float strength = 1.0f);

	/// <summary>濃さを進める（毎フレーム）</summary>
	/// <param name="deltaTime">経過時間（秒）</param>
	void Update(float deltaTime);

	/// <summary>マスクを描く（スプライトの描画フェーズから呼ぶ）</summary>
	void Draw();

	/// <summary>出ている最中か</summary>
	bool IsActive() const { return elapsed_ >= 0.0f; }

	/// <summary>すぐに消す（シーンのやり直しなどで使う）</summary>
	void Stop() { elapsed_ = -1.0f; }

private:
	/// <summary>いまの濃さ（0〜1）を求める</summary>
	float CalcAlpha() const;

	std::unique_ptr<Hagine::Sprite> sprite_;

	// --- 再生状態 ---
	float elapsed_ = -1.0f;  // 出してからの経過時間（秒）。負なら止まっている
	float strength_ = 1.0f;  // 今回の濃さの倍率

	// --- 調整パラメータ ---
	Hagine::Vector3 color_ = {0.85f, 0.05f, 0.05f}; // マスクの色
	float peakAlpha_ = 0.75f;   // 一番濃いときの不透明度
	float attackTime_ = 0.05f;  // 濃くなりきるまでの時間（秒）
	float holdTime_ = 0.06f;    // 濃いまま保つ時間（秒）
	float fadeTime_ = 0.45f;    // 薄れて消えるまでの時間（秒）
};
