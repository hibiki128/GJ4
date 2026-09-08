#include "PlayerColorComponent.h"
#include "src/Boss/Data/BossEasing.h"
#include "Frame/Frame.h"
#include "MyMath.h"
#include "Utility/Debug/Param/GameParamHub.h"

void PlayerColorComponent::RegisterParams() {
	Hagine::GameParamHub::GetInstance()->Register("Player/Color", "BlendDuration", &kBlendDuration, {0.01f, 0.0f, 2.0f});
}

void PlayerColorComponent::SetPalette(const BossColorPalette &palette) {
	palette_ = palette;

	// 色マスタが入った時点で選択色をそのまま反映する。
	// ここで補間してしまうと、ゲーム開始の一瞬だけ白いプレイヤーが見えてしまう
	displayColor_ = TargetRgba();
	blendStart_ = displayColor_;
	isBlending_ = false;
}

void PlayerColorComponent::SetSelectedColor(Color color, bool immediate) {
	const bool isChanged = (color != selectedColor_);
	selectedColor_ = color;

	if (immediate) {
		displayColor_ = TargetRgba();
		blendStart_ = displayColor_;
		blendTime_ = 0.0f;
		isBlending_ = false;
		return;
	}

	// 毎フレーム同じ色を渡されるので、変わっていないときは補間をやり直さない
	if (!isChanged) {
		return;
	}

	// 補間の途中でも、いま出している色から次の色へ繋ぎ直す
	blendStart_ = displayColor_;
	blendTime_ = 0.0f;
	isBlending_ = true;
}

void PlayerColorComponent::Update() {
	if (!isBlending_) {
		return;
	}

	blendTime_ += Hagine::Frame::DeltaTime();

	// 時間を 0 にされたとき（デバッグUIで補間を切ったとき）もここで畳む
	if (kBlendDuration <= 0.0f || blendTime_ >= kBlendDuration) {
		displayColor_ = TargetRgba();
		isBlending_ = false;
		return;
	}

	// SmoothInOut を使うこと。エンジンの EasingType::InOutSine は進捗が 0→1→0 と戻ってしまい、
	// 色が「新しい色まで行って元の色へ戻ってから切り替わる」＝点滅に見える（BossEasing.h 参照）
	const float rate = SmoothInOut(blendTime_ / kBlendDuration);
	displayColor_ = Hagine::Lerp(blendStart_, TargetRgba(), rate);
}
