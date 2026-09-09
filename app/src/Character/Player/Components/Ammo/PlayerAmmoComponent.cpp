#include "PlayerAmmoComponent.h"
#include "Frame/Frame.h"
#include "Utility/Debug/Param/GameParamHub.h"
#include <algorithm>
#include <string>
#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

void PlayerAmmoComponent::Init() {
	ammo_ = params_.maxAmmo;
	regenAccumulator_ = 0.0f;
	regenDelayTimer_ = 0.0f;
	frameScaleRequest_ = 1.0f;
	boosts_.clear();
}

void PlayerAmmoComponent::RegisterParams() {
	const std::string paramOwnerLabel = "Player/Ammo";
	Hagine::GameParamHub* hub = Hagine::GameParamHub::GetInstance();

	// 実行中に最大弾数を下げても、いまの残弾が最大を超えたままにならないようにする
	Hagine::GameParamHub::Options maxAmmoOptions{};
	maxAmmoOptions.speed = 1.0f;
	maxAmmoOptions.min = 1.0f;
	maxAmmoOptions.max = 200.0f;
	maxAmmoOptions.onChange = [this] { ammo_ = std::clamp(ammo_, 0, params_.maxAmmo); };
	hub->Register(paramOwnerLabel, "MaxAmmo", &params_.maxAmmo, maxAmmoOptions);

	hub->Register(paramOwnerLabel, "CostPerShot", &params_.costPerShot, {1.0f, 1.0f, 10.0f});
	hub->Register(paramOwnerLabel, "RegenPerSecond", &params_.regenPerSecond, {0.1f, 0.0f, 60.0f});
	hub->Register(paramOwnerLabel, "RegenDelayAfterShot", &params_.regenDelayAfterShot, {0.05f, 0.0f, 5.0f});
}

void PlayerAmmoComponent::Update() {
	const float deltaTime = Hagine::Frame::DeltaTime();

	// 時限型の倍率を進める。切れたものはここで捨てる
	for (RegenBoost& boost : boosts_) {
		boost.remain -= deltaTime;
	}
	boosts_.erase(std::remove_if(boosts_.begin(), boosts_.end(),
	                             [](const RegenBoost& boost) { return boost.remain <= 0.0f; }),
	              boosts_.end());

	// 撃った直後は少し待ってから回復を再開する（撃ちっぱなしでも減るようにするため）
	if (regenDelayTimer_ > 0.0f) {
		regenDelayTimer_ -= deltaTime;
	}

	if (ammo_ >= params_.maxAmmo) {
		// 満タンのあいだに端数が溜まると、撃った直後に1発ぶんが即座に戻ってしまう
		ammo_ = params_.maxAmmo;
		regenAccumulator_ = 0.0f;
	} else if (regenDelayTimer_ <= 0.0f) {
		regenAccumulator_ += GetEffectiveRegenPerSecond() * deltaTime;

		// 端数が1発ぶん貯まるたびに1発戻す
		while (regenAccumulator_ >= 1.0f && ammo_ < params_.maxAmmo) {
			regenAccumulator_ -= 1.0f;
			++ammo_;
		}
		if (ammo_ >= params_.maxAmmo) {
			regenAccumulator_ = 0.0f;
		}
	}

	// 継続型の要求は1フレームぶんだけ有効。
	// 次のフレームも効かせたいギミックは、また呼びに来ることになる
	frameScaleRequest_ = 1.0f;
}

bool PlayerAmmoComponent::CanFire(Color color) const {
	return GetAmmo(color) >= params_.costPerShot;
}

bool PlayerAmmoComponent::TryConsume(Color color) {
	if (!CanFire(color)) {
		return false;
	}

	ammo_ = std::max(ammo_ - params_.costPerShot, 0);
	regenDelayTimer_ = params_.regenDelayAfterShot;
	// 撃つと回復待ちに入るので、端数を残しておくと待ち明けの瞬間に1発が即座に戻ってしまう
	regenAccumulator_ = 0.0f;
	return true;
}

void PlayerAmmoComponent::Refund(Color color) {
	// いまは単一プールなので、どの色を撃ったかは結果に効かない
	(void)color;
	ammo_ = std::min(ammo_ + params_.costPerShot, params_.maxAmmo);
}

void PlayerAmmoComponent::RequestRegenScale(float scale) {
	// 一番強い要求だけを採る。掛け合わせるとギミックを重ねたときに
	// 回復速度が跳ね上がって調整が効かなくなるため。
	// 重ねがけを許したくなったら、ここと GetRegenScale の合成だけを変えればよい
	frameScaleRequest_ = std::max(frameScaleRequest_, scale);
}

void PlayerAmmoComponent::AddRegenBoost(float scale, float duration) {
	if (duration <= 0.0f) {
		return;
	}

	RegenBoost boost{};
	boost.scale = scale;
	boost.remain = duration;
	boosts_.push_back(boost);
}

void PlayerAmmoComponent::ClearRegenBoosts() {
	boosts_.clear();
	frameScaleRequest_ = 1.0f;
}

int PlayerAmmoComponent::GetAmmo(Color color) const {
	// いまは単一プールなので、どの色でも同じ残弾を返す
	(void)color;
	return ammo_;
}

float PlayerAmmoComponent::GetRatio(Color color) const {
	if (params_.maxAmmo <= 0) {
		return 0.0f;
	}
	return static_cast<float>(GetAmmo(color)) / static_cast<float>(params_.maxAmmo);
}

float PlayerAmmoComponent::GetRegenScale() const {
	float scale = frameScaleRequest_;
	for (const RegenBoost& boost : boosts_) {
		scale = std::max(scale, boost.scale);
	}
	return scale;
}

float PlayerAmmoComponent::GetEffectiveRegenPerSecond() const {
	return params_.regenPerSecond * GetRegenScale();
}

void PlayerAmmoComponent::DrawImGui() {
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("プレイヤーの残弾", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}

	const std::string ammoText = std::to_string(ammo_) + " / " + std::to_string(params_.maxAmmo);
	ImGui::ProgressBar(GetRatio(Color::RED), ImVec2(-1.0f, 0.0f), ammoText.c_str());

	if (IsEmpty(Color::RED)) {
		ImGui::TextColored(ImVec4{1.0f, 0.4f, 0.4f, 1.0f}, "弾切れ");
	} else if (regenDelayTimer_ > 0.0f) {
		ImGui::TextColored(ImVec4{1.0f, 0.8f, 0.3f, 1.0f}, "回復待ち 残り %.2f秒", regenDelayTimer_);
	} else if (ammo_ >= params_.maxAmmo) {
		ImGui::TextDisabled("満タン");
	} else {
		ImGui::TextDisabled("回復中（あと %.2f 発ぶん）", 1.0f - regenAccumulator_);
	}

	ImGui::SeparatorText("回復速度");
	const float scale = GetRegenScale();
	if (scale > 1.0f) {
		ImGui::TextColored(ImVec4{0.4f, 1.0f, 0.6f, 1.0f}, "%.2f 発/秒（%.2f倍）",
		                   GetEffectiveRegenPerSecond(), scale);
	} else {
		ImGui::Text("%.2f 発/秒", GetEffectiveRegenPerSecond());
	}
	ImGui::Text("時限ブースト: %d 件", static_cast<int>(boosts_.size()));

	// ギミックの導線（倍率を要求する → 回復が速くなる）をギミック抜きで確かめるためのボタン
	if (ImGui::Button("5秒だけ倍速")) {
		AddRegenBoost(2.0f, 5.0f);
	}
	ImGui::SameLine();
	if (ImGui::Button("ブーストを消す")) {
		ClearRegenBoosts();
	}

	ImGui::SeparatorText("残弾");
	if (ImGui::Button("1発ぶん消費")) {
		TryConsume(Color::RED);
	}
	ImGui::SameLine();
	if (ImGui::Button("全回復")) {
		Reset();
	}
#endif // USE_IMGUI
}
