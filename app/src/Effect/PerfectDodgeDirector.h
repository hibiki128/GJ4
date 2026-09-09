#pragma once
#include "2d/Sprite.h"
#include "type/Vector3.h"
#include <memory>

/// <summary>
/// ジャスト回避が決まったときの画面ぜんたいの演出。
///
/// いま受け持っているのは白フラッシュ1つ。プレイヤー1体の話ではないので、
/// 被弾の赤いマスクと同じくシーンが持つ
/// （体の伸びと粒はプレイヤー側の演出コンポーネントが受け持つ）。
///
/// 仕様書にあるスローモーションはここへ足す想定だったが、
/// ゲーム全体の時間を遅くするにはエンジン側に時間倍率が要るので見送っている。
/// カメラ寄せや専用SEを足すならこのクラスへ入れると、窓口が1つにまとまる。
/// </summary>
class PerfectDodgeDirector {
public:
	/// <summary>スプライトを用意する（シーンの初期化から1回だけ呼ぶ）</summary>
	void Init();

	/// <summary>調整パラメータをデバッグUIへ登録する（Init の後に一度だけ呼ぶ）</summary>
	void RegisterParams();

	/// <summary>演出を出す。出ている最中に呼び直せば、そこから出し直しになる</summary>
	void Play();

	/// <summary>演出を進める（毎フレーム）</summary>
	/// <param name="deltaTime">経過時間（秒）</param>
	void Update(float deltaTime);

	/// <summary>白フラッシュを描く（スプライトの描画フェーズから呼ぶ）</summary>
	void Draw();

	/// <summary>出ている最中か</summary>
	bool IsActive() const { return elapsed_ >= 0.0f; }

	/// <summary>すぐに消す（シーンのやり直しなど）</summary>
	void Stop() { elapsed_ = -1.0f; }

private:
	/// <summary>いまの白さ（0〜1）を求める</summary>
	float CalcFlashAlpha() const;

	std::unique_ptr<Hagine::Sprite> flash_;

	// --- 再生状態 ---
	float elapsed_ = -1.0f; // 出してからの経過時間（秒）。負なら止まっている

	// --- 調整パラメータ ---
	Hagine::Vector3 flashColor_ = {1.0f, 1.0f, 1.0f}; // フラッシュの色
	float flashAlpha_ = 0.3f; // 出た瞬間の白さ（画面を覆いすぎない）
	float flashTime_ = 0.06f; // 白が抜けきるまでの時間（秒）
};
