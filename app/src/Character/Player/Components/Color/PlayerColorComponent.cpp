#include "PlayerColorComponent.h"
#include "src/Boss/Data/BossEasing.h"
#include "Frame/Frame.h"
#include "MyMath.h"
#include "Utility/Debug/Param/GameParamHub.h"

namespace {
/// <summary>色相・彩度・明度（h は 0〜1 で一周ぶん）</summary>
struct Hsv {
	float h = 0.0f;
	float s = 0.0f;
	float v = 0.0f;
};

/// <summary>これ以下の彩度は「色相を持たない色」として扱う（白・黒・灰）</summary>
constexpr float kAchromaticThreshold = 0.0001f;

/// <summary>0〜1 の一周へ畳む（負の色相もここで正の側へ戻す）</summary>
float WrapHue(float hue) {
	return hue - std::floorf(hue);
}

/// <summary>RGBA を HSV へ変換する（アルファは捨てる）</summary>
Hsv ToHsv(const Hagine::Vector4 &rgba) {
	const float maxValue = (std::max)({rgba.x, rgba.y, rgba.z});
	const float minValue = (std::min)({rgba.x, rgba.y, rgba.z});
	const float range = maxValue - minValue;

	Hsv hsv{};
	hsv.v = maxValue;
	hsv.s = (maxValue > 0.0f) ? (range / maxValue) : 0.0f;

	// 無彩色は色相が定まらない。0（赤）を入れておき、混ぜる側で相手の色相を借りる
	if (range <= 0.0f) {
		return hsv;
	}

	if (maxValue == rgba.x) {
		hsv.h = (rgba.y - rgba.z) / range;
	} else if (maxValue == rgba.y) {
		hsv.h = 2.0f + (rgba.z - rgba.x) / range;
	} else {
		hsv.h = 4.0f + (rgba.x - rgba.y) / range;
	}
	hsv.h = WrapHue(hsv.h / 6.0f);
	return hsv;
}

/// <summary>HSV を RGBA へ戻す</summary>
Hagine::Vector4 ToRgba(const Hsv &hsv, float alpha) {
	if (hsv.s <= kAchromaticThreshold) {
		return {hsv.v, hsv.v, hsv.v, alpha};
	}

	// 色相環を6分割して、いまどの区間にいるかで並べ替える
	const float hue = WrapHue(hsv.h) * 6.0f;
	const int sector = static_cast<int>(hue) % 6; // hue が 6.0 に丸まっても 0 へ戻す
	const float rate = hue - static_cast<float>(sector);

	const float bottom = hsv.v * (1.0f - hsv.s);
	const float falling = hsv.v * (1.0f - hsv.s * rate);
	const float rising = hsv.v * (1.0f - hsv.s * (1.0f - rate));

	switch (sector) {
	case 0:
		return {hsv.v, rising, bottom, alpha};
	case 1:
		return {falling, hsv.v, bottom, alpha};
	case 2:
		return {bottom, hsv.v, rising, alpha};
	case 3:
		return {bottom, falling, hsv.v, alpha};
	case 4:
		return {rising, bottom, hsv.v, alpha};
	default:
		return {hsv.v, bottom, falling, alpha};
	}
}

/// <summary>
/// 色相環を回しながら2色を混ぜる。
/// RGB を直線で混ぜると中間がくすんだ灰色を通ってしまうので、
/// 彩度と明度は保ったまま色相だけを動かして「色が移っていく」ように見せる
/// </summary>
/// <param name="start">開始色</param>
/// <param name="end">終了色</param>
/// <param name="rate">0〜1 の進捗</param>
/// <returns>Vector4: 混ざった色</returns>
Hagine::Vector4 LerpThroughHue(const Hagine::Vector4 &start, const Hagine::Vector4 &end, float rate) {
	Hsv from = ToHsv(start);
	Hsv to = ToHsv(end);

	// 片方が無彩色なら相手の色相を借りる。
	// そうしないと 0（赤）から回り始めて、頼んでいない色が一瞬混ざる
	if (from.s <= kAchromaticThreshold) {
		from.h = to.h;
	}
	if (to.s <= kAchromaticThreshold) {
		to.h = from.h;
	}

	// 色相は輪なので近いほうへ回す（赤→青なら紫側、青→赤なら紫側へ戻る）
	float delta = to.h - from.h;
	if (delta > 0.5f) {
		delta -= 1.0f;
	} else if (delta < -0.5f) {
		delta += 1.0f;
	}

	Hsv blended{};
	blended.h = from.h + delta * rate;
	blended.s = Hagine::Lerp(from.s, to.s, rate);
	blended.v = Hagine::Lerp(from.v, to.v, rate);

	return ToRgba(blended, Hagine::Lerp(start.w, end.w, rate));
}
} // namespace

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

	// イージングは 0〜1 の進捗にだけ掛け、色そのものは色相環を回して混ぜる。
	// SmoothInOut を使うこと。エンジンの EasingType::InOutSine は進捗が 0→1→0 と戻ってしまい、
	// 色が「新しい色まで行って元の色へ戻ってから切り替わる」＝点滅に見える（BossEasing.h 参照）
	const float rate = SmoothInOut(blendTime_ / kBlendDuration);
	displayColor_ = LerpThroughHue(blendStart_, TargetRgba(), rate);
}
