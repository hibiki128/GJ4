#include "ResultUi.h"
#include <Input.h>
#include <algorithm>
#include <cmath>
#include <numbers>
#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

using namespace Hagine;

namespace {

/// <summary>Aボタンの図（チュートリアルと同じものを使い回す）</summary>
constexpr const char *kButtonATexture = "Tutorial/btn_a.png";

/// <summary>反応が収まるまでの時間（秒）</summary>
constexpr float kCursorAnimTime = 0.22f;
constexpr float kDecideAnimTime = 0.30f;

/// <summary>決めてからシーンを送るまでの間（秒）。押した手応えを見せるぶん</summary>
constexpr float kDecideWait = 0.24f;

/// <summary>入力を受け付けるようになるまでの間（秒）</summary>
/// ポーズを閉じたAを、そのまま決定として拾わないようにするための待ち
constexpr float kInputWakeTime = 0.18f;

/// <summary>スティックを「倒した」とみなす量</summary>
constexpr float kStickThreshold = 0.5f;

const Vector4 kWhite = {1.0f, 1.0f, 1.0f, 1.0f};
const Vector4 kPlateColor = {0.06f, 0.07f, 0.12f, 0.80f};

/// <summary>ゲームオーバーの見出しの色。彩度を落として沈ませる</summary>
const Vector4 kWobbleColor = {0.78f, 0.79f, 0.88f, 1.0f};

} // namespace

void ResultUi::Init(const std::string &idPrefix, const std::vector<std::string> &titleChars,
                    const std::vector<std::string> &itemLabels, ResultTitleMotion motion) {
    if (isInitialized_) {
        return;
    }
    isInitialized_ = true;
    motion_ = motion;

    // 見出しの色をゲーム中の球と揃えたいので、同じマスタを読む
    palette_.LoadMaster();

    rects_.Initialize(kRectCapacity);

    // 見出しは1文字＝1枚。まとめて1枚にすると文字ごとに動かせない。
    // フチは太めに。3Dの上に直接載るので、細いと背景に負ける
    titleCharCount_ = (std::min)(static_cast<int>(titleChars.size()), kMaxTitleChars);
    for (int index = 0; index < titleCharCount_; ++index) {
        titleChars_[index].Create(idPrefix + "_TitleChar" + std::to_string(index),
                                  titleChars[static_cast<size_t>(index)], 9.0f);
    }

    itemCount_ = (std::min)(static_cast<int>(itemLabels.size()), kMaxItems);
    for (int index = 0; index < itemCount_; ++index) {
        items_[index].Create(idPrefix + "_Item" + std::to_string(index),
                             itemLabels[static_cast<size_t>(index)], 6.0f);
    }

    buttonA_.Initialize(kButtonATexture);
}

void ResultUi::Finalize() {
    rects_.Finalize();
    for (GameUi::UiText &character : titleChars_) {
        character.Finalize();
    }
    for (GameUi::UiText &item : items_) {
        item.Finalize();
    }
    buttonA_.Finalize();
    isInitialized_ = false;
}

void ResultUi::RegisterParams(const std::string &ownerName) {
    params_.SetOwner(ownerName);

    params_.Register("TitleCenter", &titleCenter_, {1.0f});
    params_.Register("TitleSize", &titleSize_, {1.0f, 20.0f, 400.0f});
    params_.Register("TitleSpacing", &titleSpacing_, {0.01f, 0.4f, 2.0f});
    params_.Register("TitleMaxWidth", &titleMaxWidth_, {1.0f, 200.0f, 1760.0f});

    params_.Register("MenuAnchor", &menuAnchor_, {1.0f});
    params_.Register("ItemSize", &itemSize_, {0.5f, 8.0f, 120.0f});
    params_.Register("ItemSpacing", &itemSpacing_, {0.5f, 20.0f, 200.0f});
    params_.Register("ItemGlyphSize", &itemGlyphSize_, {0.5f, 8.0f, 200.0f});
    params_.Register("ItemGlyphGap", &itemGlyphGap_, {0.5f, 0.0f, 200.0f});
}

void ResultUi::SetInputEnabled(bool enabled) {
    if (enabled && !isInputEnabled_) {
        // 切っていた間に押されたボタンを、戻した瞬間に拾わないよう少し待つ
        inputWakeTimer_ = kInputWakeTime;
    }
    isInputEnabled_ = enabled;
}

void ResultUi::Update(float deltaTime) {
    if (!isInitialized_) {
        return;
    }

    time_ += deltaTime;

    auto decay = [deltaTime](float &value, float duration) {
        if (value > 0.0f) {
            value = (std::max)(0.0f, value - deltaTime / duration);
        }
    };
    decay(cursorAnim_, kCursorAnimTime);
    decay(decideAnim_, kDecideAnimTime);

    if (decidedIndex_ >= 0) {
        // 決まった後は演出を見せるだけ。数え終わったらシーンが引き取る
        decideTimer_ = (std::max)(0.0f, decideTimer_ - deltaTime);
        return;
    }

    PollInput(deltaTime);
}

void ResultUi::PollInput(float deltaTime) {
    if (inputWakeTimer_ > 0.0f) {
        inputWakeTimer_ = (std::max)(0.0f, inputWakeTimer_ - deltaTime);
        return;
    }
    if (!isInputEnabled_ || itemCount_ <= 0) {
        return;
    }

    Input *pInput = Input::GetInstance();
    GamePad *gamePad = pInput->GetGamePad();
    const bool padConnected = gamePad && gamePad->IsConnected();

    bool up = pInput->TriggerKey(DIK_W) || pInput->TriggerKey(DIK_UP);
    bool down = pInput->TriggerKey(DIK_S) || pInput->TriggerKey(DIK_DOWN);
    bool decide = pInput->TriggerKey(DIK_RETURN) || pInput->TriggerKey(DIK_SPACE);

    if (padConnected) {
        up = up || gamePad->IsTrigger(XINPUT_GAMEPAD_DPAD_UP);
        down = down || gamePad->IsTrigger(XINPUT_GAMEPAD_DPAD_DOWN);

        // スティックには「倒した瞬間」が無いので、前フレームと比べて自分で作る
        const float stickY = gamePad->GetLeftStickY();
        const bool stickUp = (stickY > kStickThreshold);
        const bool stickDown = (stickY < -kStickThreshold);
        up = up || (stickUp && !previousStickUp_);
        down = down || (stickDown && !previousStickDown_);
        previousStickUp_ = stickUp;
        previousStickDown_ = stickDown;

        decide = decide || gamePad->IsTrigger(XINPUT_GAMEPAD_A);
    }

    if (itemCount_ > 1) {
        if (up) {
            cursor_ = (cursor_ + itemCount_ - 1) % itemCount_;
            cursorAnim_ = 1.0f;
        }
        if (down) {
            cursor_ = (cursor_ + 1) % itemCount_;
            cursorAnim_ = 1.0f;
        }
    }

    if (decide) {
        decidedIndex_ = cursor_;
        decideAnim_ = 1.0f;
        decideTimer_ = kDecideWait;
    }
}

Vector4 ResultUi::TitleColorOf(int index, float alpha) const {
    if (motion_ == ResultTitleMotion::Wobble) {
        return {kWobbleColor.x, kWobbleColor.y, kWobbleColor.z, alpha};
    }

    // クリアの見出しは1文字ずつゲームの色で塗る。まわりの柱と同じ4色を使って、
    // 最後の絵にもこのゲームの色が残るようにする
    Vector4 rgba = palette_.GetRgba(FromColorIndex(index % kGameColorCount));
    rgba.w = alpha;
    return rgba;
}

void ResultUi::Draw() {
    if (!isInitialized_) {
        return;
    }
    rects_.BeginFrame();

    DrawTitle();
    DrawMenu();
}

void ResultUi::DrawTitle() {
    if (titleCharCount_ <= 0) {
        return;
    }

    // 中央に置きたいので、まず並べたときの幅を測る。
    // 文字ごとに幅が違う（「.」と「く」では別）ので、それぞれの幅を足していく
    float widths[kMaxTitleChars]{};
    float totalWidth = 0.0f;
    for (int index = 0; index < titleCharCount_; ++index) {
        widths[index] = titleChars_[index].WidthAt(titleSize_) * titleSpacing_;
        totalWidth += widths[index];
    }

    // 画面からはみ出すなら、収まるところまで全体を縮める
    float fit = 1.0f;
    if (titleMaxWidth_ > 0.0f && totalWidth > titleMaxWidth_) {
        fit = titleMaxWidth_ / totalWidth;
        for (int index = 0; index < titleCharCount_; ++index) {
            widths[index] *= fit;
        }
        totalWidth *= fit;
    }
    const float titleHeight = titleSize_ * fit;

    float left = titleCenter_.x - totalWidth * 0.5f;
    for (int index = 0; index < titleCharCount_; ++index) {
        const float centerX = left + widths[index] * 0.5f;
        left += widths[index];

        float offsetY = 0.0f;
        float scale = 1.0f;
        float rotation = 0.0f;
        float alpha = 1.0f;

        if (motion_ == ResultTitleMotion::Bounce) {
            // 左から順に落ちてきて、そのあとは波のように跳ね続ける
            const float appear = std::clamp((time_ - static_cast<float>(index) * 0.07f) / 0.34f,
                                            0.0f, 1.0f);
            const float drop = (1.0f - appear) * (1.0f - appear) * -240.0f;

            const float phase = time_ * 3.4f - static_cast<float>(index) * 0.55f;
            const float wave = std::sin(phase);
            offsetY = drop - std::abs(wave) * titleHeight * 0.10f;
            scale = 1.0f + (std::max)(0.0f, wave) * 0.07f;
            rotation = std::sin(phase * 0.5f) * 0.05f;
            alpha = appear;
        } else {
            // ゆっくり現れて、そのまま少し沈む。
            // 傾きは文字ごとにばらつかせる。そろっているとただの飾りに見えて、
            // 「崩れている」感じが出ない
            const float appear = std::clamp((time_ - static_cast<float>(index) * 0.06f) / 0.55f,
                                            0.0f, 1.0f);
            const float lean = std::sin(static_cast<float>(index) * 2.3f) * 0.15f;
            const float sway = std::sin(time_ * 0.9f + static_cast<float>(index) * 1.3f) * 0.05f;

            // 右へ行くほど下がる。並びが傾いていると、それだけで不安定に見える
            const float sag = static_cast<float>(index) * 2.6f;
            offsetY = -(1.0f - appear) * 26.0f + appear * 12.0f + sag +
                      std::sin(time_ * 0.8f + static_cast<float>(index) * 0.9f) * 3.0f;
            rotation = (lean + sway) * appear;
            alpha = appear;
        }

        titleChars_[index].DrawCentered({centerX, titleCenter_.y + offsetY}, titleHeight * scale,
                                        TitleColorOf(index, alpha), rotation);
    }
}

void ResultUi::DrawMenu() {
    if (itemCount_ <= 0) {
        return;
    }

    // 見出しが出そろってから案内を出す。同時に出すと、どちらも読み落とされる
    const float appear = std::clamp((time_ - 0.7f) / 0.4f, 0.0f, 1.0f);
    if (appear <= 0.01f) {
        return;
    }

    // Aボタンの図は右端に固定する。行が変わっても押す場所は変わらないので、
    // 図まで動かすと目移りするだけになる
    const Vector2 glyphBase = buttonA_.GetBaseSize();
    const float glyphWidth =
        (glyphBase.y > 0.0f) ? itemGlyphSize_ * (glyphBase.x / glyphBase.y) : itemGlyphSize_;
    const float textRight = menuAnchor_.x - glyphWidth - itemGlyphGap_;

    // 一番下の行が menuAnchor_ の高さ。項目は上へ積む
    auto rowCenterY = [this](int index) {
        return menuAnchor_.y - itemSpacing_ * static_cast<float>(itemCount_ - 1 - index);
    };

    // 出るときは右から滑り込ませる
    const float slide = (1.0f - appear) * 60.0f;

    // 3Dの上でも読めるように、下に暗い板を敷く
    {
        float widest = 0.0f;
        for (int index = 0; index < itemCount_; ++index) {
            widest = (std::max)(widest, items_[index].WidthAt(itemSize_));
        }
        const float plateWidth = widest + glyphWidth + itemGlyphGap_ + 56.0f;
        const float plateHeight =
            itemSpacing_ * static_cast<float>(itemCount_ - 1) + itemSize_ + 46.0f;
        const float plateCenterY = (rowCenterY(0) + rowCenterY(itemCount_ - 1)) * 0.5f;
        rects_.Draw({menuAnchor_.x + 24.0f - plateWidth * 0.5f + slide, plateCenterY},
                    {plateWidth, plateHeight},
                    {kPlateColor.x, kPlateColor.y, kPlateColor.z, kPlateColor.w * appear});
    }

    for (int index = 0; index < itemCount_; ++index) {
        const bool isSelected = (index == cursor_);

        // 選んでいる行だけ大きく・はっきり。選び直した瞬間と決めた瞬間に弾む
        float size = itemSize_ * (isSelected ? 1.0f : 0.86f);
        if (isSelected) {
            size *= 1.0f + cursorAnim_ * 0.10f + decideAnim_ * 0.22f;
        }
        const float alpha = (isSelected ? 1.0f : 0.5f) * appear;

        // 決めた行は白く光らせる
        Vector4 color = kWhite;
        if (!isSelected) {
            color = {0.80f, 0.82f, 0.90f, 1.0f};
        }
        color.w = alpha;

        const float centerY = rowCenterY(index);
        items_[index].DrawLeft(textRight - items_[index].WidthAt(size) + slide, centerY, size,
                               color);

        // Aボタンの図は選んでいる行にだけ添える。どこで押せばよいかが一目で分かる
        if (isSelected) {
            const float pop = 1.0f + std::sin(time_ * 4.0f) * 0.04f + decideAnim_ * 0.25f;
            const float height = itemGlyphSize_ * pop;
            const float width = (glyphBase.y > 0.0f) ? height * (glyphBase.x / glyphBase.y) : height;
            buttonA_.Draw({menuAnchor_.x - glyphWidth * 0.5f + slide, centerY}, {width, height},
                          {1.0f, 1.0f, 1.0f, appear});
        }
    }
}

void ResultUi::DrawImGui() {
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader("結果画面のUI")) {
        return;
    }
    ImGui::Text("えらんでいる項目: %d / %d", cursor_ + 1, itemCount_);
    ImGui::Text("決定: %s", (decidedIndex_ >= 0) ? "した" : "まだ");
    ImGui::TextDisabled("配置と大きさは ゲームパラメータ から");

    for (int index = 0; index < itemCount_; ++index) {
        ImGui::PushID(index);
        if (ImGui::SmallButton("これを決定")) {
            cursor_ = index;
            decidedIndex_ = index;
            decideAnim_ = 1.0f;
            decideTimer_ = kDecideWait;
        }
        ImGui::SameLine();
        ImGui::Text("%d 番目", index + 1);
        ImGui::PopID();
    }
#endif // USE_IMGUI
}
