#include "DamageVignette.h"
#include "Utility/Debug/Param/GameParamHub.h"
#include "window/WinApp.h"
#include <algorithm>

using namespace Hagine;

namespace {

/// <summary>中心が透明・ふちほど濃い、白いビネットのテクスチャ</summary>
constexpr const char* kMaskTexturePath = "UI/DamageVignette.png";

} // namespace

void DamageVignette::Init() {
	if (sprite_) {
		return;
	}

	// 画面いっぱいの1枚。大きさは Draw のたびに画面サイズから決める
	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize(kMaskTexturePath, Vector2{0.0f, 0.0f});
	sprite_->SetColor(color_);
	sprite_->SetAlpha(0.0f);
}

void DamageVignette::RegisterParams() {
	const std::string paramOwnerLabel = "Player/DamageVignette";
	GameParamHub* hub = GameParamHub::GetInstance();

	// 色は実行中に変えたらその場でスプライトへ渡す（描く直前にも入れ直すので保険）
	GameParamHub::Options colorOptions{};
	colorOptions.speed = 0.01f;
	colorOptions.min = 0.0f;
	colorOptions.max = 1.0f;
	colorOptions.onChange = [this] {
		if (sprite_) {
			sprite_->SetColor(color_);
		}
		};
	hub->Register(paramOwnerLabel, "Color", &color_, colorOptions);

	hub->Register(paramOwnerLabel, "PeakAlpha", &peakAlpha_, {0.01f, 0.0f, 1.0f});
	hub->Register(paramOwnerLabel, "AttackTime", &attackTime_, {0.01f, 0.0f, 1.0f});
	hub->Register(paramOwnerLabel, "HoldTime", &holdTime_, {0.01f, 0.0f, 1.0f});
	hub->Register(paramOwnerLabel, "FadeTime", &fadeTime_, {0.01f, 0.01f, 3.0f});
}

void DamageVignette::Play(float strength) {
	if (strength <= 0.0f) {
		return;
	}

	// 出ている最中に被弾したら、濃いほうを採ってから最初に巻き戻す。
	// 薄れかけたところで撃たれても、必ずもう一度はっきり赤くなる
	strength_ = (elapsed_ >= 0.0f) ? (std::max)(strength_, strength) : strength;
	elapsed_ = 0.0f;
}

void DamageVignette::Update(float deltaTime) {
	if (elapsed_ < 0.0f) {
		return;
	}

	elapsed_ += deltaTime;
	if (elapsed_ >= attackTime_ + holdTime_ + fadeTime_) {
		Stop();
	}
}

float DamageVignette::CalcAlpha() const {
	if (elapsed_ < 0.0f) {
		return 0.0f;
	}

	const float peak = std::clamp(peakAlpha_ * strength_, 0.0f, 1.0f);

	// 濃くなる → 保つ → 薄れる の3段階。時間を0にした段階はそのまま飛ばされる
	if (elapsed_ < attackTime_) {
		return peak * (elapsed_ / (std::max)(0.0001f, attackTime_));
	}
	if (elapsed_ < attackTime_ + holdTime_) {
		return peak;
	}

	const float fade = (elapsed_ - attackTime_ - holdTime_) / (std::max)(0.0001f, fadeTime_);
	// 終わりぎわほどゆっくり消えるので、ふっと切れた感じにならない
	const float remain = std::clamp(1.0f - fade, 0.0f, 1.0f);
	return peak * remain * remain;
}

void DamageVignette::Draw() {
	if (!sprite_ || elapsed_ < 0.0f) {
		return;
	}

	const float alpha = CalcAlpha();
	if (alpha <= 0.0f) {
		return;
	}

	sprite_->SetPosition(Vector2{0.0f, 0.0f});
	sprite_->SetSize(Vector2{static_cast<float>(WinApp::GetVirtualWidth()),
	                         static_cast<float>(WinApp::GetVirtualHeight())});
	sprite_->SetColor(color_);
	sprite_->SetAlpha(alpha);
	sprite_->Draw();
}
