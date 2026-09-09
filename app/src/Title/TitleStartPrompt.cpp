#include "TitleStartPrompt.h"
#include "src/Boss/Data/BossEasing.h"
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

} // namespace

void TitleStartPrompt::Init() {
    sprite_.Initialize(kTexturePath);
    elapsed_ = 0.0f;
}

void TitleStartPrompt::RegisterParams() {
    params_.Register("Position", &position_, {1.0f});
    params_.Register("Size", &size_, {1.0f, 8.0f, 800.0f});
    params_.Register("Scale", &scale_, {0.01f, 0.05f, 8.0f});
    params_.Register("AlphaMin", &alphaMin_, {0.01f, 0.0f, 1.0f});
    params_.Register("AlphaMax", &alphaMax_, {0.01f, 0.0f, 1.0f});
    params_.Register("Period", &period_, {0.05f, 0.2f, 10.0f});
    params_.Register("AppearDelay", &appearDelay_, {0.05f, 0.0f, 10.0f});
    params_.Register("FadeInTime", &fadeInTime_, {0.05f, 0.0f, 5.0f});
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
    ImGui::TextDisabled("保存は ゲームパラメータ > Title/StartPrompt の [保存]");
#endif // USE_IMGUI
}
