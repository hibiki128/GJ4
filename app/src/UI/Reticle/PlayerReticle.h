#pragma once
#include "camera/projection/ViewProjection.h"
#include "src/Character/Player/Core/PlayerAimReport.h"
#include "src/UI/Text/UiText.h"
#include "type/Vector2.h"
#include "type/Vector4.h"

/// <summary>
/// TPSの照準レティクル。仕様書「TPSレティクル仕様書」の実装。
///
/// 出すのは2つ。
///  ・照準レティクル（白い「+」）… 画面中央に固定。プレイヤーが狙っている向きの基準
///  ・発射レティクル（赤い「+」）… 弾が実際に当たる点。基準からずれたときだけ出る
///
/// このゲームで2つがずれる理由は、狙う向きを曲げる補正が入っているからではなく、
/// カメラと銃口（プレイヤーの位置）が別の場所にあるため。
/// 照準はカメラから射線を飛ばして当たる点を決めるが、弾が飛ぶのはプレイヤーの位置からで、
/// 同じ一点を狙っても途中の球へ先にぶつかることがある。仕様書 17.3 が名指ししているケースで、
/// 「狙ったところに当たっていない」という体験としては補正と同じものになる。
///
/// 位置の計算はここではしない。射撃が決めた PlayerAimReport をワールド→スクリーンに
/// 落とすだけで、UIの都合で弾道へ口を出すことはない（仕様書 8 / 17.1）。
///
/// 絵は白1x1テクスチャの板を組んで描く。テクスチャを持たないので、
/// 太さも大きさも実行中に GameParamHub から詰められる。
/// 被弾の赤いマスクと同じく、プレイヤーは画面を知らないのでシーンが配線する。
/// </summary>
class PlayerReticle {
public:
	/// <summary>板の置き場を用意する（シーンの初期化から1回だけ呼ぶ）</summary>
	void Init();

	/// <summary>調整パラメータをデバッグUIへ登録する（Init の後に一度だけ呼ぶ）</summary>
	void RegisterParams();

	/// <summary>
	/// 表示位置を更新する（毎フレーム）。
	/// 呼ぶのは射撃の更新が終わった直後（Player::SetOnAimReport の通知の中）。
	/// シーンの Update から呼ぶと1フレーム古い狙いで描くことになる
	/// </summary>
	/// <param name="report">射撃が決めたこのフレームの狙い</param>
	/// <param name="viewProjection">描画に使っているカメラ（ワールド→スクリーンに使う）</param>
	/// <param name="deltaTime">経過時間（秒）</param>
	void Update(const PlayerAimReport& report, const Hagine::ViewProjection& viewProjection, float deltaTime);

	/// <summary>レティクルを描く（スプライトの描画フェーズから呼ぶ）</summary>
	void Draw();

	/// <summary>発射レティクルを引っ込める（シーンのやり直しや、狙いが途切れたとき）</summary>
	void Reset();

	/// <summary>発射レティクルが出ているか</summary>
	bool IsFiringReticleVisible() const { return firingVisible_; }

private:
	/// <summary>画面（仮想解像度）の中心</summary>
	static Hagine::Vector2 ScreenCenter();

	/// <summary>
	/// 「+」を1つ描く。アウトラインを先に敷いてから明るい線を重ねるので、
	/// 明るい背景でも暗い背景でも輪郭が残る
	/// </summary>
	/// <param name="center">中心（画面座標）</param>
	/// <param name="size">線の長さ（ピクセル）</param>
	/// <param name="color">線の色</param>
	/// <param name="alpha">濃さの倍率（フェード用。色のアルファに掛かる）</param>
	void DrawCross(const Hagine::Vector2& center, float size, const Hagine::Vector4& color, float alpha);

	GameUi::UiRect rects_;      // 「+」を組む板の置き場
	bool initialized_ = false;  // Init 済みか（未初期化のまま描くと板が借りられない）

	/// ===================================================
	/// 発射レティクルの状態（仕様書 12）
	/// ===================================================

	bool firingVisible_ = false;      // 出す条件を満たしているか
	Hagine::Vector2 firingTarget_{};  // 出したい位置（着弾点の画面座標）
	Hagine::Vector2 firingCurrent_{}; // いま描いている位置
	Hagine::Vector2 firingStart_{};   // 移動を始めた位置（＝出た瞬間の画面中央）
	float firingMoveElapsed_ = 0.0f;  // 移動を始めてからの経過時間（秒）
	float firingAlpha_ = 0.0f;        // いまの濃さ（0〜1。出入りのフェード）

	/// ===================================================
	/// 調整パラメータ（初期値は仕様書 13 の表から）
	/// ===================================================

	bool enabled_ = true;          // レティクルを出すか（撮影・確認用に切れるようにしておく）
	float aimSize_ = 14.0f;        // 照準レティクルの線の長さ（px）
	float firingSize_ = 10.0f;     // 発射レティクルの線の長さ（px。照準よりやや小さめ）
	float lineWidth_ = 2.0f;       // 線の太さ（px）
	float outlineWidth_ = 1.0f;    // アウトラインのはみ出し幅（px。0でアウトラインなし）
	float outlineAlpha_ = 0.5f;    // アウトラインの濃さ

	// 発射レティクルを出す最低のズレ幅（px）。これを下回るズレでは出さない。
	// 小さなズレで出し入れすると、レティクルが細かく震えて画面が落ち着かない（仕様書 6 / 17.2）
	float displayThreshold_ = 8.0f;

	// 画面中央から着弾点へ動くのにかける時間（秒）。
	// 長すぎると表示位置と実際の射撃方向が一致しなくなるので短くする（仕様書 7）
	float moveTime_ = 0.10f;

	// 出入りのフェードにかける時間（秒）
	float fadeTime_ = 0.10f;

	// 色は役割で分ける（仕様書 14.1）。色だけに頼らないよう、大きさと表示状態も変えてある
	Hagine::Vector4 aimColor_ = {0.85f, 0.95f, 1.0f, 0.90f};   // 照準：白〜薄い水色
	Hagine::Vector4 firingColor_ = {1.0f, 0.30f, 0.20f, 1.0f}; // 発射：赤
};
