#pragma once
#include "src/Boss/Data/BossColorPalette.h"
#include "src/Character/ColorStruct.h"
#include "type/Vector4.h"

/// <summary>
/// プレイヤーの見た目の色を一手に引き受けるコンポーネント。
///
/// 選択色が変わった瞬間にモデルの色を差し替えるとチカチカして目が痛いので、
/// 「いま出している色」をここで持ち、選択色のRGBAへイージングで寄せていく。
/// 混ぜ方は RGB の直線ではなく色相環を回す方式。
/// 直線で混ぜると赤→青の中間がくすんだ灰紫を通ってしまうが、色相を回せば
/// 赤→桃→紫→藍→青 と彩度を保ったまま「色が移っていく」ように見える。
/// 補間の途中でさらに色を変えられても、そのとき出している色から次の色へ繋ぎ直すので、
/// 色替えを連打しても白へ戻ったり逆再生になったりはしない。
///
/// 色のRGBAはボスと同じマスタ（BossColorPalette）から引く。
/// プレイヤーとボスで同じ赤に見えないと、色を合わせて撃つゲームが成立しないため。
/// </summary>
class PlayerColorComponent {
public:
	// 調整パラメータをデバッグUIへ登録する（Player::Init から一度だけ呼ぶ）
	void RegisterParams();
	// 色マスタを受け取る。この時点の選択色はそのまま（補間せず）反映される
	void SetPalette(const BossColorPalette& palette);
	// 選択色を設定する。今と違う色なら補間が始まる
	// immediate を true にすると補間せずその場で切り替える（初期化時用）
	void SetSelectedColor(Color color, bool immediate = false);
	// 補間を進める
	void Update();
	// 選択中の色
	Color GetSelectedColor() const { return selectedColor_; }
	// いまモデルへ出すべきRGBA
	const Hagine::Vector4& GetDisplayColor() const { return displayColor_; }
	// 色の切り替え中か（演出を重ねたいときに使う）
	bool IsBlending() const { return isBlending_; }
private:
	// 選択色のRGBA（補間の行き先）
	Hagine::Vector4 TargetRgba() const { return palette_.GetRgba(selectedColor_); }

	// --- 色の元データ ---
	BossColorPalette palette_{};       // 色マスタ（ボスと共通のものを受け取る）
	Color selectedColor_ = Color::RED; // 選択中の色

	// --- 再生状態 ---
	Hagine::Vector4 displayColor_ = {1.0f, 1.0f, 1.0f, 1.0f}; // いま出している色
	Hagine::Vector4 blendStart_ = {1.0f, 1.0f, 1.0f, 1.0f};   // 補間の開始色
	float blendTime_ = 0.0f;                                  // 補間の経過時間（秒）
	bool isBlending_ = false;                                 // 補間中か

	// --- 調整パラメータ ---
	float kBlendDuration = 0.25f; // 色が入れ替わりきるまでの時間
};
