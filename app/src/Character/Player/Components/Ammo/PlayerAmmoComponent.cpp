#include "PlayerAmmoComponent.h"
#include "Frame/Frame.h"
#include "Utility/Debug/Param/GameParamHub.h"
#include <algorithm>
#include <string>
#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

void PlayerAmmoComponent::Init() {
	ammo_.fill(params_.maxAmmo);
	regenAccumulator_.fill(0.0f);
	regenDelayTimer_.fill(0.0f);
	frameScaleRequest_.fill(1.0f);
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
	maxAmmoOptions.onChange = [this] {
		for (int &amount : ammo_) {
			amount = std::clamp(amount, 0, params_.maxAmmo);
		}
	};
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

	// 色ごとに独立して回復する。撃った色だけが待たされ、他の色は進み続ける
	for (int index = 0; index < kGameColorCount; ++index) {
		const Color color = FromColorIndex(index);

		// 撃った直後は少し待ってから回復を再開する（撃ちっぱなしでも減るようにするため）
		if (regenDelayTimer_[index] > 0.0f) {
			regenDelayTimer_[index] -= deltaTime;
		}

		if (ammo_[index] >= params_.maxAmmo) {
			// 満タンのあいだに端数が溜まると、撃った直後に1発ぶんが即座に戻ってしまう
			ammo_[index] = params_.maxAmmo;
			regenAccumulator_[index] = 0.0f;
		} else if (regenDelayTimer_[index] <= 0.0f) {
			regenAccumulator_[index] += GetEffectiveRegenPerSecond(color) * deltaTime;

			// 端数が1発ぶん貯まるたびに1発戻す
			while (regenAccumulator_[index] >= 1.0f && ammo_[index] < params_.maxAmmo) {
				regenAccumulator_[index] -= 1.0f;
				++ammo_[index];
			}
			if (ammo_[index] >= params_.maxAmmo) {
				regenAccumulator_[index] = 0.0f;
			}
		}
	}

	// 継続型の要求は1フレームぶんだけ有効。
	// 次のフレームも効かせたいギミックは、また呼びに来ることになる
	frameScaleRequest_.fill(1.0f);
}

bool PlayerAmmoComponent::CanFire(Color color) const {
	return GetAmmo(color) >= params_.costPerShot;
}

bool PlayerAmmoComponent::TryConsume(Color color) {
	if (!CanFire(color)) {
		return false;
	}

	const int index = ToColorIndex(color);
	ammo_[index] = std::max(ammo_[index] - params_.costPerShot, 0);
	regenDelayTimer_[index] = params_.regenDelayAfterShot;
	// 撃つと回復待ちに入るので、端数を残しておくと待ち明けの瞬間に1発が即座に戻ってしまう
	regenAccumulator_[index] = 0.0f;
	return true;
}

void PlayerAmmoComponent::Refund(Color color) {
	const int index = ToColorIndex(color);
	ammo_[index] = std::min(ammo_[index] + params_.costPerShot, params_.maxAmmo);
}

void PlayerAmmoComponent::RequestRegenScale(float scale) {
	// 色を指定しない要求は全色へ効く（速い床のような、色を問わないギミック用）
	for (int index = 0; index < kGameColorCount; ++index) {
		RequestRegenScale(FromColorIndex(index), scale);
	}
}

void PlayerAmmoComponent::RequestRegenScale(Color color, float scale) {
	// 一番強い要求だけを採る。掛け合わせるとギミックを重ねたときに
	// 回復速度が跳ね上がって調整が効かなくなるため。
	// 重ねがけを許したくなったら、ここと GetRegenScale の合成だけを変えればよい
	float &request = frameScaleRequest_[ToColorIndex(color)];
	request = std::max(request, scale);
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
	frameScaleRequest_.fill(1.0f);
}

int PlayerAmmoComponent::GetAmmo(Color color) const {
	return ammo_[ToColorIndex(color)];
}

float PlayerAmmoComponent::GetRatio(Color color) const {
	if (params_.maxAmmo <= 0) {
		return 0.0f;
	}
	return static_cast<float>(GetAmmo(color)) / static_cast<float>(params_.maxAmmo);
}

float PlayerAmmoComponent::GetRegenScale(Color color) const {
	float scale = frameScaleRequest_[ToColorIndex(color)];
	for (const RegenBoost& boost : boosts_) {
		scale = std::max(scale, boost.scale);
	}
	return scale;
}

float PlayerAmmoComponent::GetEffectiveRegenPerSecond(Color color) const {
	return params_.regenPerSecond * GetRegenScale(color);
}

void PlayerAmmoComponent::DrawImGui() {
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("プレイヤーの残弾", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}

	// 色ごとに別のプールなので、4色ぶんを並べて見せる。
	// 回復エリアはこのうち1色だけを早めるので、どれが伸びているか見えるようにしておく
	static constexpr ImVec4 kColorTints[kGameColorCount] = {
	    ImVec4{0.95f, 0.35f, 0.35f, 1.0f}, // RED
	    ImVec4{0.40f, 0.60f, 1.00f, 1.0f}, // BLUE
	    ImVec4{0.40f, 0.90f, 0.50f, 1.0f}, // GREEN
	    ImVec4{0.95f, 0.85f, 0.35f, 1.0f}, // YELLOW
	};

	for (int index = 0; index < kGameColorCount; ++index) {
		const Color color = FromColorIndex(index);
		ImGui::PushID(index);
		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, kColorTints[index]);

		const std::string ammoText = std::string(GetColorIdText(color)) + "  " +
		                             std::to_string(ammo_[index]) + " / " +
		                             std::to_string(params_.maxAmmo);
		ImGui::ProgressBar(GetRatio(color), ImVec2(-1.0f, 0.0f), ammoText.c_str());
		ImGui::PopStyleColor();

		const float scale = GetRegenScale(color);
		if (IsEmpty(color)) {
			ImGui::TextColored(ImVec4{1.0f, 0.4f, 0.4f, 1.0f}, "  弾切れ");
		} else if (regenDelayTimer_[index] > 0.0f) {
			ImGui::TextColored(ImVec4{1.0f, 0.8f, 0.3f, 1.0f}, "  回復待ち 残り %.2f秒",
			                   regenDelayTimer_[index]);
		} else if (IsFull(color)) {
			ImGui::TextDisabled("  満タン");
		} else if (scale > 1.0f) {
			ImGui::TextColored(ImVec4{0.4f, 1.0f, 0.6f, 1.0f}, "  %.2f 発/秒（%.2f倍）",
			                   GetEffectiveRegenPerSecond(color), scale);
		} else {
			ImGui::TextDisabled("  %.2f 発/秒（あと %.2f 発ぶん）",
			                    GetEffectiveRegenPerSecond(color), 1.0f - regenAccumulator_[index]);
		}

		ImGui::SameLine();
		if (ImGui::SmallButton("1発消費")) {
			TryConsume(color);
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("この色を早める")) {
			RequestRegenScale(color, 4.0f);
		}
		ImGui::PopID();
	}

	ImGui::SeparatorText("全色まとめて");
	ImGui::Text("時限ブースト: %d 件（色を問わず全色へ効く）", static_cast<int>(boosts_.size()));

	// ギミックの導線（倍率を要求する → 回復が速くなる）をギミック抜きで確かめるためのボタン
	if (ImGui::Button("5秒だけ倍速")) {
		AddRegenBoost(2.0f, 5.0f);
	}
	ImGui::SameLine();
	if (ImGui::Button("ブーストを消す")) {
		ClearRegenBoosts();
	}
	ImGui::SameLine();
	if (ImGui::Button("全回復")) {
		Reset();
	}
#endif // USE_IMGUI
}
