#pragma once
#include "camera/projection/ViewProjection.h"
#include "src/Character/Player/Core/PlayerAimReport.h"
#include "src/UI/Text/UiText.h"
#include "type/Vector2.h"
#include "type/Vector4.h"

/// <summary>
/// TPSの照準レティクル。仕様書「TPSレティクル仕様書」の実装。
///
/// 出すのは2つで、どちらも常に画面に出ている。
///  ・照準レティクル（Hud/Reticle/reticle.png ＝ 色つきの十字）
///      … 画面中央に固定。プレイヤーが狙っている向きの基準
///  ・発射レティクル（Hud/Reticle/reticle_assist.png ＝ 水色の円）
///      … 弾が実際に当たる点。ズレていないときは画面中央で照準レティクルを囲み、
///        ズレたらその着弾点へ移る
///
/// 仕様書 5.1 / 9.2 は「補正が無いときは発射レティクルを消す」としているが、
/// ここでは出しっぱなしにして行き先だけを動かしている。消えたり出たりするより、
/// 同じ印が寄り添っているか離れているかで見せたほうが、ズレの有無が一目で分かるため。
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
/// 絵は2枚のテクスチャをそのまま出す。形がはっきり違うので、色を見なくても
/// どちらがどちらか分かる（仕様書 14「色だけに依存しない」）。
/// 大きさと濃さは実行中に GameParamHub から詰められる。
/// 被弾の赤いマスクと同じく、プレイヤーは画面を知らないのでシーンが配線する。
/// </summary>
class PlayerReticle {
public:
	/// <summary>スプライトを用意する（シーンの初期化から1回だけ呼ぶ）</summary>
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

	/// <summary>発射レティクルを画面中央へ戻す（シーンのやり直しや、狙いが途切れたとき）</summary>
	void Reset();

	/// <summary>発射レティクルが中央を離れて着弾点を指しているか</summary>
	bool IsShowingFirePoint() const { return firingDiverged_; }

private:
	/// <summary>画面（仮想解像度）の中心</summary>
	static Hagine::Vector2 ScreenCenter();

	/// <summary>
	/// レティクルを1枚描く。テクスチャ本来の縦横比を保ったまま、
	/// height で指定した高さに合わせて拡縮する
	/// </summary>
	/// <param name="sprite">描くスプライト</param>
	/// <param name="center">中心（画面座標）</param>
	/// <param name="height">描画したい高さ（ピクセル）</param>
	/// <param name="color">重ねる色（白なら絵の色そのまま）</param>
	static void DrawReticle(GameUi::UiSprite& sprite, const Hagine::Vector2& center, float height,
	                        const Hagine::Vector4& color);

	// スプライトは1枚につき1フレーム1回しか描けないが、どちらも1回ずつしか出さないので
	// プールは要らない（UiText.h の UiSprite の注意書きを参照）
	GameUi::UiSprite aimSprite_;    // 照準レティクル（色つきの十字）
	GameUi::UiSprite firingSprite_; // 発射レティクル（水色の円）

	/// ===================================================
	/// 発射レティクルの状態（仕様書 12）
	/// ===================================================

	// 常に出ているので「見えているか」は持たない。持つのは行き先がどちらかだけ
	bool firingDiverged_ = false;     // 着弾点を指しているか（false なら画面中央にいる）
	Hagine::Vector2 firingTarget_{};  // 行き先（画面中央 or 着弾点の画面座標）
	Hagine::Vector2 firingCurrent_{}; // いま描いている位置
	Hagine::Vector2 firingStart_{};   // 移動を始めた位置（行き先が入れ替わった瞬間の位置）
	float firingMoveElapsed_ = 0.0f;  // 移動を始めてからの経過時間（秒）

	/// ===================================================
	/// 調整パラメータ（初期値は仕様書 13 の表から）
	/// ===================================================

	// 仕様書 13 の表は細い線画の「+」を前提にした 10〜16px だが、この絵は描き込みがあるので
	// そのサイズだと潰れて読めない。また仕様書 4.3 は「発射レティクルは照準よりやや小さめ」
	// としているが、こちらは常時表示にしたぶん、ズレていないときは円が十字を囲む形にしたいので
	// 発射レティクルのほうを大きくしてある。離れれば大小の差がそのまま2つの区別になる
	bool enabled_ = true;      // レティクルを出すか（撮影・確認用に切れるようにしておく）
	float aimSize_ = 48.0f;    // 照準レティクルの高さ（px）
	float firingSize_ = 54.0f; // 発射レティクルの高さ（px。中央では照準を囲む大きさ）

	// 着弾点へ移り始める最低のズレ幅（px）。これを下回るズレでは画面中央に留まる。
	// わずかなズレで行き先が入れ替わると、レティクルが細かく震えて画面が落ち着かない
	// （仕様書 6 / 17.2）
	float displayThreshold_ = 8.0f;

	// 行き先が入れ替わったときに動き切るまでの時間（秒）。
	// 長すぎると表示位置と実際の射撃方向が一致しなくなるので短くする（仕様書 7）
	float moveTime_ = 0.10f;

	// 絵に重ねる色。テクスチャがすでに役割ごとに違う色と形で描かれているので、
	// 既定は白＝描かれたとおりに出す。ここを触れば全体の色味と濃さを寄せられる
	Hagine::Vector4 aimColor_ = {1.0f, 1.0f, 1.0f, 0.90f};    // 照準（色つきの十字）
	Hagine::Vector4 firingColor_ = {1.0f, 1.0f, 1.0f, 1.0f};  // 発射（水色の円）
};
