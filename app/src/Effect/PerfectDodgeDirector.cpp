#include "PerfectDodgeDirector.h"
#include "Utility/Debug/Param/GameParamHub.h"
#include "window/WinApp.h"
#include <algorithm>

using namespace Hagine;

namespace {

/// <summary>画面いっぱいに引き伸ばす真っ白な1枚</summary>
constexpr const char* kFlashTexturePath = "debug/white1x1.png";

} // namespace

void PerfectDodgeDirector::Init() {
	if (flash_) {
		return;
	}

	// 画面いっぱいの1枚。大きさは Draw のたびに画面サイズから決める
	flash_ = std::make_unique<Sprite>();
	flash_->Initialize(kFlashTexturePath, Vector2{0.0f, 0.0f});
	flash_->SetColor(flashColor_);
	flash_->SetAlpha(0.0f);
}

void PerfectDodgeDirector::RegisterParams() {
	const std::string paramOwnerLabel = "Player/PerfectDodge";
	GameParamHub* hub = GameParamHub::GetInstance();

	// 色は実行中に変えたらその場でスプライトへ渡す（描く直前にも入れ直すので保険）
	GameParamHub::Options colorOptions{};
	colorOptions.speed = 0.01f;
	colorOptions.min = 0.0f;
	colorOptions.max = 1.0f;
	colorOptions.onChange = [this] {
		if (flash_) {
			flash_->SetColor(flashColor_);
		}
		};
	hub->Register(paramOwnerLabel, "FlashColor", &flashColor_, colorOptions);

	hub->Register(paramOwnerLabel, "FlashAlpha", &flashAlpha_, {0.01f, 0.0f, 1.0f});
	hub->Register(paramOwnerLabel, "FlashTime", &flashTime_, {0.01f, 0.0f, 0.5f});
}

void PerfectDodgeDirector::Play() {
	// 続けて決めたときは最初から出し直す（受け流すたびに必ず白く光る）
	elapsed_ = 0.0f;
}

void PerfectDodgeDirector::Update(float deltaTime) {
	if (elapsed_ < 0.0f) {
		return;
	}

	elapsed_ += deltaTime;
	if (elapsed_ >= flashTime_) {
		Stop();
	}
}

float PerfectDodgeDirector::CalcFlashAlpha() const {
	if (elapsed_ < 0.0f || elapsed_ >= flashTime_) {
		return 0.0f;
	}

	// 出た瞬間が一番白く、そこからすぐ抜ける（1〜2フレームで消える想定）
	const float t = elapsed_ / (std::max)(0.0001f, flashTime_);
	return std::clamp(flashAlpha_ * (1.0f - t), 0.0f, 1.0f);
}

void PerfectDodgeDirector::Draw() {
	if (!flash_) {
		return;
	}

	const float alpha = CalcFlashAlpha();
	if (alpha <= 0.0f) {
		return;
	}

	flash_->SetPosition(Vector2{0.0f, 0.0f});
	flash_->SetSize(Vector2{static_cast<float>(WinApp::GetVirtualWidth()),
	                        static_cast<float>(WinApp::GetVirtualHeight())});
	flash_->SetColor(flashColor_);
	flash_->SetAlpha(alpha);
	flash_->Draw();
}
