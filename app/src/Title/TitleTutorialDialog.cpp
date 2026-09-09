#include "TitleTutorialDialog.h"
#include "src/Boss/Data/BossEasing.h"
#include "src/Settings/GameSettings.h"
#include "data/DataHandler.h"
#include "debug/imgui/ImGuiNotification.h"
#include <Input.h>
#include <WinApp.h>
#include <algorithm>
#include <cmath>
#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

using namespace Hagine;

namespace {

/// ===================================================
/// 見た目（チュートリアルのテロップに合わせてある）
/// ===================================================

constexpr Vector4 kOverlayColor = {0.02f, 0.02f, 0.05f, 0.68f}; // 画面全体の暗幕
constexpr Vector4 kPanelColor = {0.07f, 0.09f, 0.15f, 0.92f};   // 板
constexpr Vector4 kAccentColor = {0.35f, 0.78f, 1.00f, 1.0f};   // 左の色帯・選んでいる枠
constexpr Vector4 kTitleColor = {1.0f, 1.0f, 1.0f, 1.0f};       // 問いかけ
constexpr Vector4 kOptionIdleBox = {1.0f, 1.0f, 1.0f, 0.10f};   // 選んでいない枠
constexpr Vector4 kOptionIdleText = {0.70f, 0.74f, 0.82f, 1.0f}; // 選んでいない文字
constexpr Vector4 kOptionPickedText = {0.05f, 0.07f, 0.11f, 1.0f}; // 選んでいる文字（枠が明るいので暗く）
constexpr Vector4 kHintColor = {0.62f, 0.66f, 0.74f, 1.0f};     // 操作説明
constexpr Vector4 kWhite = {1.0f, 1.0f, 1.0f, 1.0f};

/// <summary>板の左に立てる色帯の幅</summary>
constexpr float kAccentBarWidth = 8.0f;

/// <summary>板の中の縦位置（板の高さに対する割合）</summary>
constexpr float kTitleRatio = 0.26f;
constexpr float kOptionRatio = 0.56f;
constexpr float kHintRatio = 0.85f;

/// <summary>操作説明の図と文字のすき間 / 説明どうしのすき間</summary>
constexpr float kHintIconGap = 12.0f;
constexpr float kHintGroupGap = 56.0f;

/// <summary>開き始めの大きさ（ここから1.0へ）</summary>
constexpr float kOpenStartScale = 0.86f;

/// <summary>
/// 文字を「見た目の中心」に置くための持ち上げ量（文字の高さに対する割合）。
///
/// 焼いた文字の画像は、フォントの上の余白（ascent 230）と下の余白（descent 26）を
/// そのまま含んでいて上下が対称でない。中心をそのまま合わせると字が約10%下がって見えるので、
/// そのぶん持ち上げる
/// </summary>
constexpr float kTextCenterLift = 0.10f;

/// <summary>スティックを「倒した」とみなす量（ポーズ画面と同じ）</summary>
constexpr float kStickThreshold = 0.5f;

/// <summary>操作説明に使う図</summary>
constexpr const char *kStickTexture = "Tutorial/stick_l.png";
constexpr const char *kDpadTexture = "Tutorial/dpad.png";
constexpr const char *kDecideTexture = "Tutorial/btn_a.png";

/// <summary>
/// 調整値の保存先（Assets/jsons/Title/TutorialDialog.json）。
///
/// ゲームパラメータのハブではなく自前のファイルに持っている。
/// 触る場所（オブジェクト設定のパネル）と保存する場所が同じほうが、
/// 「調整したのに保存されない」が起きない
/// </summary>
constexpr const char *kDataFolder = "Title";
constexpr const char *kDataFile = "TutorialDialog";

/// <summary>色のアルファに倍率を掛ける（開き際のフェード用）</summary>
Vector4 WithAlpha(const Vector4 &color, float alphaScale) {
    return {color.x, color.y, color.z, color.w * alphaScale};
}

/// <summary>画像の縦横比を保ったまま、指定の高さに収めたときの大きさ</summary>
Vector2 FitHeight(const GameUi::UiSprite &sprite, float height) {
    const Vector2 &base = sprite.GetBaseSize();
    const float width = (base.y > 0.0f) ? height * (base.x / base.y) : height;
    return {width, height};
}

} // namespace

void TitleTutorialDialog::Init() {
    // 暗幕・板・色帯・選択肢の枠・選んでいる枠の縁で最大7枚
    rects_.Initialize(12);

    title_.Create("TitleDialog_Title", "チュートリアルをプレイしますか？");
    options_[0].Create("TitleDialog_Yes", "する");
    options_[1].Create("TitleDialog_No", "しない");
    hintSelect_.Create("TitleDialog_HintSelect", "でえらぶ", 3.0f);
    hintDecide_.Create("TitleDialog_HintDecide", "できめる", 3.0f);

    glyphStick_.Initialize(kStickTexture);
    glyphDpad_.Initialize(kDpadTexture);
    glyphDecide_.Initialize(kDecideTexture);

    Load();
}

void TitleTutorialDialog::Load() {
    // 読めなければコードの既定値がそのまま残る（Load の第2引数が今の値）
    DataHandler data(kDataFolder, kDataFile);
    panelCenter_ = data.Load<Vector2>("panelCenter", panelCenter_);
    panelSize_ = data.Load<Vector2>("panelSize", panelSize_);
    optionBoxSize_ = data.Load<Vector2>("optionBoxSize", optionBoxSize_);
    optionGap_ = data.Load<float>("optionGap", optionGap_);
    titleSize_ = data.Load<float>("titleSize", titleSize_);
    optionSize_ = data.Load<float>("optionSize", optionSize_);
    hintSize_ = data.Load<float>("hintSize", hintSize_);
    glyphSize_ = data.Load<float>("glyphSize", glyphSize_);
    openTime_ = data.Load<float>("openTime", openTime_);
    decideTime_ = data.Load<float>("decideTime", decideTime_);
}

void TitleTutorialDialog::Save() const {
    DataHandler data(kDataFolder, kDataFile);
    data.Save<Vector2>("panelCenter", panelCenter_);
    data.Save<Vector2>("panelSize", panelSize_);
    data.Save<Vector2>("optionBoxSize", optionBoxSize_);
    data.Save<float>("optionGap", optionGap_);
    data.Save<float>("titleSize", titleSize_);
    data.Save<float>("optionSize", optionSize_);
    data.Save<float>("hintSize", hintSize_);
    data.Save<float>("glyphSize", glyphSize_);
    data.Save<float>("openTime", openTime_);
    data.Save<float>("decideTime", decideTime_);
    data.Flush();
}

void TitleTutorialDialog::Open() {
    state_ = State::Opening;
    stateTimer_ = 0.0f;
    elapsed_ = 0.0f;
    selectedIndex_ = 0; // 既定は「する」。初めての人が素通りしないように
    isResultReported_ = false;

    // 開いた瞬間に倒れっぱなしのスティックで動かないよう、いまの倒し向きを見たことにする
    axisDirection_ = 0;
}

void TitleTutorialDialog::Close() {
    state_ = State::Closed;
    stateTimer_ = 0.0f;
    isResultReported_ = false;
}

TitleTutorialDialog::Result TitleTutorialDialog::Update(float deltaTime) {
    if (state_ == State::Closed) {
        return Result::None;
    }

    elapsed_ += deltaTime;
    stateTimer_ += deltaTime;

    // 開いている最中は操作を受け付けない。
    // これが無いと、問いかけを開いたその A がそのまま決定に使われてしまう
    if (state_ == State::Opening) {
        if (stateTimer_ >= openTime_) {
            state_ = State::Choosing;
            stateTimer_ = 0.0f;
        }
        // 開いている間もスティックの倒し向きは追っておく（倒しっぱなしを「押した」にしない）
        PollInput();
        return Result::None;
    }

    if (state_ == State::Choosing) {
        const DialogInput input = PollInput();

        if (input.cancel) {
            Close();
            return Result::None;
        }

        const int previous = selectedIndex_;
        if (input.left) {
            selectedIndex_ = (selectedIndex_ + kOptionCount - 1) % kOptionCount;
        }
        if (input.right) {
            selectedIndex_ = (selectedIndex_ + 1) % kOptionCount;
        }
        if (selectedIndex_ != previous) {
            // ポーズ画面のカーソル移動と同じ手ざわりにそろえる
            GameSettings::GetInstance()->RequestVibration(0.25f, 0.05f);
        }

        if (input.decide) {
            GameSettings::GetInstance()->RequestVibration(0.5f, 0.1f);
            state_ = State::Decided;
            stateTimer_ = 0.0f;
        }
        return Result::None;
    }

    // 決まった。選んだほうが光るのを見せてから返す
    if (!isResultReported_ && stateTimer_ >= decideTime_) {
        isResultReported_ = true;
        return (selectedIndex_ == 0) ? Result::Yes : Result::No;
    }
    return Result::None;
}

TitleTutorialDialog::DialogInput TitleTutorialDialog::PollInput() {
    DialogInput result{};

    Input *pInput = Input::GetInstance();
    GamePad *gamePad = pInput->GetGamePad();
    const bool padConnected = gamePad && gamePad->IsConnected();

    bool left = pInput->PushKey(DIK_A) || pInput->PushKey(DIK_LEFT);
    bool right = pInput->PushKey(DIK_D) || pInput->PushKey(DIK_RIGHT);
    result.decide = pInput->TriggerKey(DIK_RETURN) || pInput->TriggerKey(DIK_SPACE);
    result.cancel = pInput->TriggerKey(DIK_ESCAPE) || pInput->TriggerKey(DIK_BACK);

    if (padConnected) {
        const float stickX = gamePad->GetLeftStickX();
        left = left || gamePad->IsPress(XINPUT_GAMEPAD_DPAD_LEFT) || stickX < -kStickThreshold;
        right = right || gamePad->IsPress(XINPUT_GAMEPAD_DPAD_RIGHT) || stickX > kStickThreshold;
        result.decide = result.decide || gamePad->IsTrigger(XINPUT_GAMEPAD_A);
        result.cancel = result.cancel || gamePad->IsTrigger(XINPUT_GAMEPAD_B);
    }

    // 2択なので、倒しっぱなしでの繰り返しは要らない。倒し切った瞬間だけ拾う。
    // 繰り返すと、少し長く倒しただけで選択が行ったり来たりして狙いが定まらない
    const int direction = left ? -1 : (right ? 1 : 0);
    if (direction != axisDirection_) {
        result.left = (direction < 0);
        result.right = (direction > 0);
        axisDirection_ = direction;
    }
    return result;
}

float TitleTutorialDialog::SelectionPulse() const {
    // ポーズ画面の選択中と同じ脈打ち
    return 1.0f + 0.045f * std::sin(elapsed_ * 6.5f);
}

void TitleTutorialDialog::Draw() {
    if (state_ == State::Closed) {
        return;
    }

    rects_.BeginFrame();

    const float screenWidth = static_cast<float>(WinApp::GetVirtualWidth());
    const float screenHeight = static_cast<float>(WinApp::GetVirtualHeight());

    // 開き際は板が少し小さいところから開く。中身も同じ倍率で動かすので形が崩れない
    const float appear =
        (state_ == State::Opening && openTime_ > 0.0f) ? SmoothInOut(stateTimer_ / openTime_) : 1.0f;
    const float scale = kOpenStartScale + (1.0f - kOpenStartScale) * appear;

    // 板の中心からの相対位置で組み立てる（倍率をそのまま掛けられる）
    const auto place = [&](float offsetX, float offsetY) {
        return Vector2{panelCenter_.x + offsetX * scale, panelCenter_.y + offsetY * scale};
    };

    // 暗幕。後ろのロゴやボスと文字が喧嘩しないよう一段落とす
    rects_.Draw({screenWidth * 0.5f, screenHeight * 0.5f}, {screenWidth, screenHeight},
                WithAlpha(kOverlayColor, appear));

    // 板と、左端の色帯（チュートリアルのテロップと同じ作り）
    const Vector2 panelSize = {panelSize_.x * scale, panelSize_.y * scale};
    rects_.Draw(panelCenter_, panelSize, WithAlpha(kPanelColor, appear));
    rects_.Draw(place(-panelSize_.x * 0.5f + kAccentBarWidth * 0.5f, 0.0f),
                {kAccentBarWidth * scale, panelSize.y}, WithAlpha(kAccentColor, appear));

    const float panelTop = -panelSize_.y * 0.5f;

    // 問いかけ
    const float titleHeight = titleSize_ * scale;
    Vector2 titleCenter = place(0.0f, panelTop + panelSize_.y * kTitleRatio);
    titleCenter.y -= titleHeight * kTextCenterLift;
    title_.DrawCentered(titleCenter, titleHeight, WithAlpha(kTitleColor, appear));

    // 選択肢。選んでいるほうは枠を反転させ、少し大きくして脈打たせる
    const float optionY = panelTop + panelSize_.y * kOptionRatio;
    const float step = optionBoxSize_.x + optionGap_;
    for (int i = 0; i < kOptionCount; ++i) {
        const bool isPicked = (i == selectedIndex_);
        const float offsetX = (static_cast<float>(i) - 0.5f) * step;
        const Vector2 center = place(offsetX, optionY);

        float boxScale = scale;
        if (isPicked) {
            boxScale *= SelectionPulse();
            // 決まったあとは、選んだほうがひと回り大きくなって光る
            if (state_ == State::Decided) {
                const float flash =
                    (decideTime_ > 0.0f) ? std::clamp(stateTimer_ / decideTime_, 0.0f, 1.0f) : 1.0f;
                boxScale *= 1.0f + 0.12f * (1.0f - flash);
            }
        }

        const Vector2 boxSize = {optionBoxSize_.x * boxScale, optionBoxSize_.y * boxScale};

        if (isPicked) {
            // 縁を1枚だけ後ろに敷いて、選んでいるほうを浮かせる
            rects_.Draw(center, {boxSize.x + 10.0f * scale, boxSize.y + 10.0f * scale},
                        WithAlpha(kAccentColor, 0.35f * appear));
        }
        rects_.Draw(center, boxSize,
                    WithAlpha(isPicked ? kAccentColor : kOptionIdleBox, appear));

        // 文字は枠の中心。選んでいるほうは枠が明るいので黒めにして読みやすくする
        const float optionHeight = optionSize_ * boxScale;
        options_[i].DrawCentered({center.x, center.y - optionHeight * kTextCenterLift}, optionHeight,
                                 WithAlpha(isPicked ? kOptionPickedText : kOptionIdleText, appear));
    }

    // 操作説明: [スティック][十字] でえらぶ   [A] できめる
    const float glyph = glyphSize_ * scale;
    const Vector2 stickSize = FitHeight(glyphStick_, glyph);
    const Vector2 dpadSize = FitHeight(glyphDpad_, glyph);
    const Vector2 decideSize = FitHeight(glyphDecide_, glyph);
    const float hintTextSize = hintSize_ * scale;
    const float selectTextWidth = hintSelect_.WidthAt(hintTextSize);
    const float decideTextWidth = hintDecide_.WidthAt(hintTextSize);

    const float selectGroupWidth = stickSize.x + dpadSize.x + kHintIconGap * scale + selectTextWidth;
    const float decideGroupWidth = decideSize.x + kHintIconGap * scale + decideTextWidth;
    const float hintTotal = selectGroupWidth + kHintGroupGap * scale + decideGroupWidth;

    const Vector2 hintCenter = place(0.0f, panelTop + panelSize_.y * kHintRatio);
    float cursorX = hintCenter.x - hintTotal * 0.5f;

    const auto drawGlyph = [&](GameUi::UiSprite &sprite, const Vector2 &size) {
        sprite.Draw({cursorX + size.x * 0.5f, hintCenter.y}, size, WithAlpha(kWhite, appear));
        cursorX += size.x;
    };

    const float hintTextY = hintCenter.y - hintTextSize * kTextCenterLift;

    drawGlyph(glyphStick_, stickSize);
    drawGlyph(glyphDpad_, dpadSize);
    cursorX += kHintIconGap * scale;
    hintSelect_.DrawLeft(cursorX, hintTextY, hintTextSize, WithAlpha(kHintColor, appear));
    cursorX += selectTextWidth + kHintGroupGap * scale;

    drawGlyph(glyphDecide_, decideSize);
    cursorX += kHintIconGap * scale;
    hintDecide_.DrawLeft(cursorX, hintTextY, hintTextSize, WithAlpha(kHintColor, appear));
}

void TitleTutorialDialog::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SeparatorText("チュートリアルの問いかけ");

    if (ImGui::Button("保存")) {
        Save();
        ImGuiNotification::Post("問いかけの設定を保存しました", {0.2f, 0.8f, 0.2f, 1.0f});
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Assets/jsons/Title/TutorialDialog.json");

    static const char *kStateNames[] = {"閉じている", "開いている最中", "選んでもらい中", "決まった"};
    ImGui::Text("いまの状態: %s", kStateNames[static_cast<int>(state_)]);
    ImGui::Text("選んでいる: %s", (selectedIndex_ == 0) ? "する" : "しない");

    if (ImGui::Button("出してみる")) {
        Open();
    }
    ImGui::SameLine();
    if (ImGui::Button("閉じる")) {
        Close();
    }

    ImGui::DragFloat2("板の中心", &panelCenter_.x, 1.0f);
    ImGui::DragFloat2("板の大きさ", &panelSize_.x, 1.0f, 100.0f, 1760.0f, "%.0f");
    ImGui::DragFloat2("選択肢の枠", &optionBoxSize_.x, 1.0f, 40.0f, 800.0f, "%.0f");
    ImGui::DragFloat("選択肢の間", &optionGap_, 1.0f, 0.0f, 400.0f, "%.0f");
    ImGui::DragFloat("問いかけの文字", &titleSize_, 0.5f, 10.0f, 150.0f, "%.0f");
    ImGui::DragFloat("選択肢の文字", &optionSize_, 0.5f, 10.0f, 150.0f, "%.0f");
    ImGui::DragFloat("説明の文字", &hintSize_, 0.5f, 8.0f, 100.0f, "%.0f");
    ImGui::DragFloat("説明の図", &glyphSize_, 0.5f, 8.0f, 150.0f, "%.0f");
    ImGui::DragFloat("開く時間", &openTime_, 0.01f, 0.05f, 2.0f, "%.2f 秒");
    ImGui::DragFloat("決めてから進むまで", &decideTime_, 0.01f, 0.0f, 3.0f, "%.2f 秒");
#endif // USE_IMGUI
}
