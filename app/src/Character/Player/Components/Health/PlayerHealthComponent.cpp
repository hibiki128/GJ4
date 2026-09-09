#include "PlayerHealthComponent.h"
#include "Frame/Frame.h"
#include "Utility/Debug/Param/GameParamHub.h"
#include <algorithm>
#include <string>
#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

void PlayerHealthComponent::Init() {
	hp_ = params_.maxHp;
	invincibleTimer_ = 0.0f;
	hitPending_ = false;
	lastDamage_ = DamageInfo{};
	blockedPending_ = false;
	lastBlocked_ = DamageInfo{};
}

void PlayerHealthComponent::RegisterParams() {
	const std::string paramOwnerLabel = "Player/Health";
	Hagine::GameParamHub* hub = Hagine::GameParamHub::GetInstance();

	// 実行中に最大HPを下げても、いまのHPが最大を超えたままにならないようにする
	Hagine::GameParamHub::Options maxHpOptions{};
	maxHpOptions.speed = 1.0f;
	maxHpOptions.min = 1.0f;
	maxHpOptions.max = 20.0f;
	maxHpOptions.onChange = [this] { hp_ = std::clamp(hp_, 0, params_.maxHp); };
	hub->Register(paramOwnerLabel, "MaxHp", &params_.maxHp, maxHpOptions);

	hub->Register(paramOwnerLabel, "DamagePerHit", &params_.damagePerHit, {1.0f, 1.0f, 10.0f});
	hub->Register(paramOwnerLabel, "InvincibleTime", &params_.invincibleTime, {0.05f, 0.0f, 5.0f});
}

void PlayerHealthComponent::Update() {
	if (invincibleTimer_ > 0.0f) {
		invincibleTimer_ -= Hagine::Frame::DeltaTime();
	}
}

bool PlayerHealthComponent::ApplyDamage(const DamageInfo& info) {
	if (IsDead()) {
		return false;
	}

	// 無敵中の被弾は無かったことにする（複数の攻撃が同じフレームに届いても1回ぶんだけ減る）。
	// ただし「弾いた」という事実だけは残す。回避の出だしで弾けていればジャスト回避になる
	if (IsInvincible()) {
		lastBlocked_ = info;
		blockedPending_ = true;
		return false;
	}

	hp_ = std::max(hp_ - params_.damagePerHit, 0);
	invincibleTimer_ = params_.invincibleTime;
	lastDamage_ = info;
	hitPending_ = true;
	return true;
}

bool PlayerHealthComponent::Heal(int amount) {
	if (amount <= 0 || IsDead()) {
		return false;
	}

	const int healed = std::min(hp_ + amount, params_.maxHp);
	if (healed == hp_) {
		return false; // すでに満タン。呼び出し側はこれを見て「効かなかった」と分かる
	}

	hp_ = healed;
	return true;
}

bool PlayerHealthComponent::ConsumeHit() {
	const bool wasHit = hitPending_;
	hitPending_ = false;
	return wasHit;
}

bool PlayerHealthComponent::ConsumeBlocked() {
	const bool wasBlocked = blockedPending_;
	blockedPending_ = false;
	return wasBlocked;
}

float PlayerHealthComponent::GetRatio() const {
	if (params_.maxHp <= 0) {
		return 0.0f;
	}
	return static_cast<float>(hp_) / static_cast<float>(params_.maxHp);
}

void PlayerHealthComponent::DrawImGui() {
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("プレイヤーの体力", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}

	const std::string hpText = std::to_string(hp_) + " / " + std::to_string(params_.maxHp);
	ImGui::ProgressBar(GetRatio(), ImVec2(-1.0f, 0.0f), hpText.c_str());

	if (IsDead()) {
		ImGui::TextColored(ImVec4{1.0f, 0.4f, 0.4f, 1.0f}, "倒れている");
	} else if (IsInvincible()) {
		ImGui::TextColored(ImVec4{1.0f, 0.8f, 0.3f, 1.0f}, "無敵 残り %.2f秒", invincibleTimer_);
	} else {
		ImGui::TextDisabled("被弾を受け付ける状態");
	}

	// 被弾の導線（体力が減る → 被弾ステートへ入る）をボス抜きで確かめるためのボタン
	if (ImGui::Button("1回ぶん被弾させる")) {
		DamageInfo info{};
		info.amount = static_cast<float>(params_.damagePerHit);
		info.hitPoint = lastDamage_.hitPoint;
		ApplyDamage(info);
	}
	ImGui::SameLine();
	// 回復の導線（HPが1増える）をアイテム抜きで確かめるためのボタン
	if (ImGui::Button("1回復")) {
		Heal(1);
	}
	ImGui::SameLine();
	if (ImGui::Button("全回復")) {
		Reset();
	}
#endif // USE_IMGUI
}
