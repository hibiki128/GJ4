#include "TitleStartPrompt.h"
#include "src/Boss/Data/BossEasing.h"
#include "data/DataHandler.h"
#include "debug/imgui/ImGuiNotification.h"
#include <cmath>
#include <numbers>
#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

using namespace Hagine;

namespace {

/// <summary>Aボタンの絵（チュートリアルで使っているものと同じ）</summary>
constexpr const char *kTexturePath = "Tutorial/btn_a.png";

constexpr float kPi = std::numbers::pi_v<float>;

/// <summary>
/// 調整値の保存先（Assets/jsons/Title/StartPrompt.json）。
///
/// ゲームパラメータのハブではなく自前のファイルに持っている。
/// 触る場所（オブジェクト設定のパネル）と保存する場所が同じほうが、
/// 「調整したのに保存されない」が起きない
/// </summary>
constexpr const char *kDataFolder = "Title";
constexpr const char *kDataFile = "StartPrompt";

} // namespace

void TitleStartPrompt::Init() {
    sprite_.Initialize(kTexturePath);
    elapsed_ = 0.0f;
    Load();
}

void TitleStartPrompt::Load() {
    // 読めなければコードの既定値がそのまま残る（Load の第2引数が今の値）
    DataHandler data(kDataFolder, kDataFile);
    position_ = data.Load<Vector2>("position", position_);
    size_ = data.Load<Vector2>("size", size_);
    scale_ = data.Load<float>("scale", scale_);
    alphaMin_ = data.Load<float>("alphaMin", alphaMin_);
    alphaMax_ = data.Load<float>("alphaMax", alphaMax_);
    period_ = data.Load<float>("period", period_);
    appearDelay_ = data.Load<float>("appearDelay", appearDelay_);
    fadeInTime_ = data.Load<float>("fadeInTime", fadeInTime_);
}

void TitleStartPrompt::Save() const {
    DataHandler data(kDataFolder, kDataFile);
    data.Save<Vector2>("position", position_);
    data.Save<Vector2>("size", size_);
    data.Save<float>("scale", scale_);
    data.Save<float>("alphaMin", alphaMin_);
    data.Save<float>("alphaMax", alphaMax_);
    data.Save<float>("period", period_);
    data.Save<float>("appearDelay", appearDelay_);
    data.Save<float>("fadeInTime", fadeInTime_);
    data.Flush();
}

void TitleStartPrompt::Update(float deltaTime) {
    elapsed_ += deltaTime;
}

void TitleStartPrompt::Draw() {
    if (!sprite_.IsReady() || elapsed_ < appearDelay_) {
        return;
    }

    const float time = elapsed_ - appearDelay_;

    // 出はじめの1回だけ、すっと現れる
    const float appear = (fadeInTime_ > 0.0f) ? SmoothInOut(time / fadeInTime_) : 1.0f;

    // 明滅。cos なので折り返しで速度が0になり、濃さの変わり方が途切れない
    const float safePeriod = (period_ > 0.0f) ? period_ : 1.0f;
    const float wave = 0.5f - 0.5f * std::cos(2.0f * kPi * time / safePeriod);
    const float alpha = (alphaMin_ + (alphaMax_ - alphaMin_) * wave) * appear;

    sprite_.Draw(position_, Vector2{size_.x * scale_, size_.y * scale_},
                 Vector4{1.0f, 1.0f, 1.0f, alpha});
}

void TitleStartPrompt::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SeparatorText("スタートの案内（Aボタン）");

    if (ImGui::Button("保存")) {
        Save();
        ImGuiNotification::Post("スタートの案内の設定を保存しました", {0.2f, 0.8f, 0.2f, 1.0f});
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Assets/jsons/Title/StartPrompt.json");

    ImGui::DragFloat2("位置", &position_.x, 1.0f);
    ImGui::DragFloat2("もとの大きさ(px)", &size_.x, 1.0f, 8.0f, 800.0f, "%.0f");
    ImGui::DragFloat("倍率(縦横同時)", &scale_, 0.01f, 0.05f, 8.0f, "%.2f 倍");

    // 縦横を別々に触ったあと、元絵の形へ戻すための口
    if (ImGui::Button("元絵の比率にそろえる")) {
        const Vector2 &base = sprite_.GetBaseSize();
        if (base.x > 0.0f) {
            size_.y = size_.x * (base.y / base.x);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("出はじめからやり直す")) {
        Restart();
    }

    ImGui::DragFloat("薄いときの濃さ", &alphaMin_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("濃いときの濃さ", &alphaMax_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("1往復の時間", &period_, 0.05f, 0.2f, 10.0f, "%.2f 秒");
    ImGui::DragFloat("出てくるまでの待ち", &appearDelay_, 0.05f, 0.0f, 10.0f, "%.2f 秒");
    ImGui::DragFloat("出てくる時間", &fadeInTime_, 0.05f, 0.0f, 5.0f, "%.2f 秒");

    const Vector2 &base = sprite_.GetBaseSize();
    ImGui::TextDisabled("元絵 %.0fx%.0f  →  いま %.0fx%.0f px", base.x, base.y,
                        size_.x * scale_, size_.y * scale_);
#endif // USE_IMGUI
}
