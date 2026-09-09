#include "PauseMenu.h"
#include "src/Settings/GameSettings.h"
#include <Frame.h>
#include <Input.h>
#include <WinApp.h>
#include <utility/scene/SceneManager.h>
#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI
#include <cstdio>

using namespace Hagine;

namespace {

/// ===================================================
/// 見た目の調整値（仮のUIなのでここを触れば一通り変えられる）
/// ===================================================

// 開閉アニメーション
constexpr float kOpenDuration = 0.34f;   // 開くのにかける秒数
constexpr float kCloseDuration = 0.18f;  // 閉じるのにかける秒数
constexpr float kOpenStartScale = 0.15f; // 開き始めのスケール

// 入力のリピート
constexpr float kRepeatDelay = 0.32f;    // 押しっぱなしでリピートが始まるまで
constexpr float kRepeatInterval = 0.07f; // リピートの間隔
constexpr float kStickThreshold = 0.5f;  // スティックを倒したとみなす値

// パネル（ローカル座標はパネル中心が原点）
constexpr float kRootPanelWidth = 820.0f;
constexpr float kRootPanelHeight = 520.0f;
constexpr float kSettingsPanelWidth = 940.0f;
constexpr float kPanelBorder = 6.0f; // 枠として見せるための外側の張り出し

// 設定ページの縦位置。項目を増減してもレイアウトが崩れないよう、
// パネルの縦の長さは行数から決める（下の各値はパネル上端からの距離）
constexpr float kSettingsRowSpacing = 72.0f;      // 行の間隔
constexpr float kSettingsPanelBase = 280.0f;      // 1行だけのときのパネルの縦の長さ
constexpr float kSettingsTitleOffset = 75.0f;     // タイトルの中心まで
constexpr float kSettingsDividerOffset = 130.0f;  // 区切り線まで
constexpr float kSettingsFirstRowOffset = 200.0f; // 1行目まで
constexpr float kSettingsHintGap = 35.0f;         // 最終行から操作説明まで

// 色
constexpr Vector4 kOverlayColor = {0.02f, 0.02f, 0.05f, 0.72f};
constexpr Vector4 kPanelColor = {0.08f, 0.09f, 0.14f, 0.94f};
constexpr Vector4 kBorderColor = {0.28f, 0.52f, 0.86f, 0.95f};
constexpr Vector4 kDividerColor = {0.28f, 0.52f, 0.86f, 0.55f};
constexpr Vector4 kTitleColor = {1.0f, 1.0f, 1.0f, 1.0f};
constexpr Vector4 kItemColor = {0.70f, 0.74f, 0.82f, 1.0f};
constexpr Vector4 kItemSelectedColor = {1.0f, 0.85f, 0.36f, 1.0f};
constexpr Vector4 kMarkerColor = {1.0f, 0.72f, 0.20f, 1.0f};
constexpr Vector4 kGaugeBackColor = {0.18f, 0.20f, 0.28f, 1.0f};
constexpr Vector4 kGaugeFillColor = {0.36f, 0.62f, 0.94f, 1.0f};
constexpr Vector4 kGaugeFillSelectedColor = {1.0f, 0.78f, 0.32f, 1.0f};
constexpr Vector4 kHintColor = {0.52f, 0.56f, 0.64f, 1.0f};

// 文字の大きさ
constexpr float kTitleHeight = 76.0f;
constexpr float kRootItemHeight = 48.0f;
constexpr float kSettingLabelHeight = 36.0f;
constexpr float kValueHeight = 34.0f;
constexpr float kHintHeight = 24.0f;

/// <summary>
/// 色のアルファに任意の倍率を掛けた色を返す（フェード用）
/// </summary>
Vector4 WithAlpha(const Vector4 &color, float alphaScale)
{
    return {color.x, color.y, color.z, color.w * alphaScale};
}

/// <summary>
/// 値を 0〜1 の割合へ正規化する
/// </summary>
float Normalize(float value, float min, float max)
{
    if (max - min <= 0.0f)
    {
        return 0.0f;
    }
    return std::clamp((value - min) / (max - min), 0.0f, 1.0f);
}

/// <summary>
/// 0〜1 の値を "85%" のような文字列にする
/// </summary>
std::string ToPercentText(float ratio)
{
    char buffer[16] = {};
    std::snprintf(buffer, sizeof(buffer), "%d%%", static_cast<int>(std::round(ratio * 100.0f)));
    return buffer;
}

/// <summary>
/// 倍率を "1.50" のような文字列にする
/// </summary>
std::string ToScaleText(float value)
{
    char buffer[16] = {};
    std::snprintf(buffer, sizeof(buffer), "%.2f", value);
    return buffer;
}

} // namespace

/// ===================================================
/// RepeatInput
/// ===================================================

bool PauseMenu::RepeatInput::Update(bool held, float deltaTime)
{
    if (!held)
    {
        isHeld = false;
        timer = 0.0f;
        return false;
    }

    if (!isHeld)
    {
        // 押した瞬間は必ず1回反応させる
        isHeld = true;
        timer = kRepeatDelay;
        return true;
    }

    timer -= deltaTime;
    if (timer <= 0.0f)
    {
        timer = kRepeatInterval;
        return true;
    }
    return false;
}

/// ===================================================
/// PauseMenu
/// ===================================================

PauseMenu *PauseMenu::GetInstance()
{
    // params_ の解除（デストラクタ）がハブの破棄より後に走らないよう、ハブを先に作っておく
    GameParamHub::GetInstance();
    static PauseMenu instance;
    return &instance;
}

void PauseMenu::Finalize()
{
    // 板・数値・文字ラベルが抱えているスプライトをすべて手放す。
    // ここを通さないと、シングルトンなので終了時まで生き残る
    rects_.Finalize();
    number_.Finalize();

    titleRoot_.Finalize();
    titleSettings_.Finalize();
    for (GameUi::UiText &item : rootItems_)
    {
        item.Finalize();
    }
    for (GameUi::UiText &label : settingLabels_)
    {
        label.Finalize();
    }
    valueOn_.Finalize();
    valueOff_.Finalize();
    hintRoot_.Finalize();
    hintSettings_.Finalize();

    // 作り直せる状態へ戻しておく
    isInitialized_ = false;
}

void PauseMenu::Initialize()
{
    if (isInitialized_)
    {
        return;
    }
    isInitialized_ = true;

    // 設定の読み込みもここで済ませる（ポーズ画面を開かなくても設定が効くように）
    GameSettings::GetInstance()->Initialize();

    // 単色の板。暗幕・パネル・ゲージはすべてこれを色とサイズを変えて使い回す
    rects_.Initialize(kRectCapacity);

    // TextRenderer はフォントから直接グリフを起こすので日本語もそのまま出せる。
    // ただしこのフォントに無い字は消えるので、文字を変えたら実機で確認すること
    titleRoot_.Create("Pause_TitleRoot", "ぽーず");
    titleSettings_.Create("Pause_TitleSettings", "設定");

    rootItems_[kRootResume].Create("Pause_ItemResume", "つづける");
    rootItems_[kRootSettings].Create("Pause_ItemSettings", "設定");
    rootItems_[kRootReturnTitle].Create("Pause_ItemReturnTitle", "はじめにもどる");

    settingLabels_[kSettingMasterVolume].Create("Pause_LabelVolume", "全体の音量", 3.0f);
    settingLabels_[kSettingSensitivity].Create("Pause_LabelSensitivity", "感度", 3.0f);
    settingLabels_[kSettingVibration].Create("Pause_LabelVibration", "振動", 3.0f);
    settingLabels_[kSettingBack].Create("Pause_LabelBack", "もどる", 3.0f);

    valueOn_.Create("Pause_ValueOn", "あり", 3.0f);
    valueOff_.Create("Pause_ValueOff", "なし", 3.0f);

    hintRoot_.Create("Pause_HintRoot", "A:けってい   B:とじる", 2.0f);
    hintSettings_.Create("Pause_HintSettings", "左右:へんこう   B:もどる", 2.0f);

    number_.Create("Pause_Numbers", kNumberCapacity);

    // 画面に対する大きさは実機で見ながら決めたいので、ハブから触れるようにしておく。
    // 保存済みの値があればこの登録時にそのまま読み戻される
    GameParamHub::Options scaleOptions{};
    scaleOptions.speed = 0.01f;
    scaleOptions.min = 0.2f;
    scaleOptions.max = 3.0f;
    params_.Register("全体の大きさ", &uiScale_, scaleOptions);
    params_.Register("文字の大きさ", &textScale_, scaleOptions);
}

void PauseMenu::Open()
{
    if (state_ == State::Opening || state_ == State::Open)
    {
        return;
    }

    state_ = State::Opening;
    page_ = Page::Root;
    rootIndex_ = 0;
    settingIndex_ = 0;
    elapsed_ = 0.0f;
    // 小さい状態から少し行き過ぎて戻る出方にする
    scaleEase_.Reset(kOpenStartScale, 1.0f, kOpenDuration, EasingType::OutBack);
    scale_ = kOpenStartScale;
}

void PauseMenu::Close()
{
    if (state_ == State::Closed || state_ == State::Closing)
    {
        return;
    }

    state_ = State::Closing;
    scaleEase_.Reset(scale_, 0.0f, kCloseDuration, EasingType::InBack);

    // 閉じるときに、いじった設定を確実に書き出しておく
    GameSettings::GetInstance()->Save();
}

void PauseMenu::CloseImmediately()
{
    state_ = State::Closed;
    page_ = Page::Root;
    scale_ = 0.0f;
    fade_ = 0.0f;
    elapsed_ = 0.0f;
}

void PauseMenu::Update()
{
    Initialize();

    const float deltaTime = Frame::DeltaTime();
    const MenuInput input = PollInput(deltaTime);

    if (input.toggle)
    {
        if (state_ == State::Closed || state_ == State::Closing)
        {
            Open();
        }
        else
        {
            Close();
        }
    }

    UpdateAnimation(deltaTime);

    if (state_ != State::Open)
    {
        return; // アニメーション中は項目の操作を受け付けない
    }

    elapsed_ += deltaTime;

    if (page_ == Page::Root)
    {
        UpdateRootPage(input);
    }
    else
    {
        UpdateSettingsPage(input);
    }
}

PauseMenu::MenuInput PauseMenu::PollInput(float deltaTime)
{
    MenuInput input;

    Input *pInput = Input::GetInstance();
    GamePad *gamePad = pInput->GetGamePad();
    const bool padConnected = gamePad && gamePad->IsConnected();

    // ----- 開閉 -----
    input.toggle = pInput->TriggerKey(DIK_ESCAPE);
    if (padConnected)
    {
        input.toggle = input.toggle || gamePad->IsTrigger(XINPUT_GAMEPAD_START);
    }

    // ----- 決定・戻る -----
    input.decide = pInput->TriggerKey(DIK_RETURN) || pInput->TriggerKey(DIK_SPACE);
    input.cancel = pInput->TriggerKey(DIK_BACK);
    if (padConnected)
    {
        input.decide = input.decide || gamePad->IsTrigger(XINPUT_GAMEPAD_A);
        input.cancel = input.cancel || gamePad->IsTrigger(XINPUT_GAMEPAD_B);
    }

    // ----- カーソル・値の増減（押しっぱなしでリピート）-----
    bool up = pInput->PushKey(DIK_W) || pInput->PushKey(DIK_UP);
    bool down = pInput->PushKey(DIK_S) || pInput->PushKey(DIK_DOWN);
    bool left = pInput->PushKey(DIK_A) || pInput->PushKey(DIK_LEFT);
    bool right = pInput->PushKey(DIK_D) || pInput->PushKey(DIK_RIGHT);

    if (padConnected)
    {
        const float stickX = gamePad->GetLeftStickX();
        const float stickY = gamePad->GetLeftStickY();
        up = up || gamePad->IsPress(XINPUT_GAMEPAD_DPAD_UP) || stickY > kStickThreshold;
        down = down || gamePad->IsPress(XINPUT_GAMEPAD_DPAD_DOWN) || stickY < -kStickThreshold;
        left = left || gamePad->IsPress(XINPUT_GAMEPAD_DPAD_LEFT) || stickX < -kStickThreshold;
        right = right || gamePad->IsPress(XINPUT_GAMEPAD_DPAD_RIGHT) || stickX > kStickThreshold;
    }

    input.up = repeatUp_.Update(up, deltaTime);
    input.down = repeatDown_.Update(down, deltaTime);
    input.left = repeatLeft_.Update(left, deltaTime);
    input.right = repeatRight_.Update(right, deltaTime);

    return input;
}

void PauseMenu::UpdateAnimation(float deltaTime)
{
    switch (state_)
    {
    case State::Opening:
        scale_ = scaleEase_.Update(deltaTime);
        // 暗幕はスケールの行き過ぎに引きずられないよう、経過時間から素直に出す
        fade_ = std::clamp(scaleEase_.time / kOpenDuration, 0.0f, 1.0f);
        if (scaleEase_.IsFinished())
        {
            state_ = State::Open;
            scale_ = 1.0f;
            fade_ = 1.0f;
        }
        break;

    case State::Closing:
        scale_ = scaleEase_.Update(deltaTime);
        fade_ = 1.0f - std::clamp(scaleEase_.time / kCloseDuration, 0.0f, 1.0f);
        if (scaleEase_.IsFinished())
        {
            CloseImmediately();
        }
        break;

    case State::Open:
        scale_ = 1.0f;
        fade_ = 1.0f;
        break;

    case State::Closed:
    default:
        break;
    }

    // 行き過ぎの戻り際にスケールが負へ落ちると裏返って見えるので止める
    scale_ = std::max(scale_, 0.0f);
}

void PauseMenu::UpdateRootPage(const MenuInput &input)
{
    if (input.up)
    {
        rootIndex_ = (rootIndex_ + kRootItemCount - 1) % kRootItemCount;
        OnCursorMoved();
    }
    if (input.down)
    {
        rootIndex_ = (rootIndex_ + 1) % kRootItemCount;
        OnCursorMoved();
    }

    if (input.cancel)
    {
        Close();
        return;
    }

    if (!input.decide)
    {
        return;
    }

    OnDecided();
    switch (rootIndex_)
    {
    case kRootResume:
        Close();
        break;

    case kRootSettings:
        page_ = Page::Settings;
        settingIndex_ = 0;
        break;

    case kRootReturnTitle:
        Close();
        SceneManager::GetInstance()->NextSceneReservation("TITLE");
        break;

    default:
        break;
    }
}

void PauseMenu::UpdateSettingsPage(const MenuInput &input)
{
    if (input.up)
    {
        settingIndex_ = (settingIndex_ + kSettingItemCount - 1) % kSettingItemCount;
        OnCursorMoved();
    }
    if (input.down)
    {
        settingIndex_ = (settingIndex_ + 1) % kSettingItemCount;
        OnCursorMoved();
    }

    if (input.left)
    {
        AdjustSetting(settingIndex_, -1);
    }
    if (input.right)
    {
        AdjustSetting(settingIndex_, +1);
    }

    if (input.cancel)
    {
        page_ = Page::Root;
        return;
    }

    if (!input.decide)
    {
        return;
    }

    OnDecided();
    if (settingIndex_ == kSettingBack)
    {
        page_ = Page::Root;
    }
    else if (settingIndex_ == kSettingVibration)
    {
        // 決定でも切り替えられるようにしておく
        AdjustSetting(kSettingVibration, +1);
    }
}

void PauseMenu::AdjustSetting(int item, int direction)
{
    GameSettings *settings = GameSettings::GetInstance();
    const float sign = static_cast<float>(direction);

    switch (item)
    {
    case kSettingMasterVolume:
        settings->SetMasterVolume(settings->GetMasterVolume() + sign * 0.02f);
        break;

    case kSettingSensitivity:
        settings->SetStickSensitivity(settings->GetStickSensitivity() + sign * 0.05f);
        break;

    case kSettingVibration:
        settings->SetVibrationEnabled(!settings->IsVibrationEnabled());
        // 切り替えた結果がその場で分かるように、オンにしたときは一度鳴らす
        settings->RequestVibration(0.6f, 0.2f);
        break;

    default:
        break;
    }
}

void PauseMenu::OnCursorMoved()
{
    GameSettings::GetInstance()->RequestVibration(0.25f, 0.05f);
}

void PauseMenu::OnDecided()
{
    GameSettings::GetInstance()->RequestVibration(0.5f, 0.1f);
}

float PauseMenu::SelectionPulse() const
{
    return 1.0f + 0.045f * std::sin(elapsed_ * 6.5f);
}

Vector2 PauseMenu::ToScreen(float localX, float localY) const
{
    const float centerX = static_cast<float>(WinApp::GetVirtualWidth()) * 0.5f;
    const float centerY = static_cast<float>(WinApp::GetVirtualHeight()) * 0.5f;
    const float drawScale = DrawScale();
    return {centerX + localX * drawScale, centerY + localY * drawScale};
}

void PauseMenu::DrawPanelRect(float localX, float localY, float width, float height, const Vector4 &color)
{
    const float drawScale = DrawScale();
    rects_.Draw(ToScreen(localX, localY), {width * drawScale, height * drawScale}, WithAlpha(color, fade_));
}

void PauseMenu::Draw()
{
    if (state_ == State::Closed || !isInitialized_)
    {
        return;
    }

    // 板と数字は同じスプライトを並べて使い回すので、フレーム頭で使用位置を戻す
    rects_.BeginFrame();
    number_.BeginFrame();

    const float screenWidth = static_cast<float>(WinApp::GetVirtualWidth());
    const float screenHeight = static_cast<float>(WinApp::GetVirtualHeight());

    // 背景の暗幕。スケールの影響を受けない全画面
    rects_.Draw({screenWidth * 0.5f, screenHeight * 0.5f},
                {screenWidth, screenHeight},
                WithAlpha(kOverlayColor, fade_));

    if (page_ == Page::Root)
    {
        DrawRootPage();
    }
    else
    {
        DrawSettingsPage();
    }
}

void PauseMenu::DrawRootPage()
{
    // 枠 → パネルの順に重ねて縁取りに見せる
    DrawPanelRect(0.0f, 0.0f, kRootPanelWidth + kPanelBorder * 2.0f, kRootPanelHeight + kPanelBorder * 2.0f, kBorderColor);
    DrawPanelRect(0.0f, 0.0f, kRootPanelWidth, kRootPanelHeight, kPanelColor);

    titleRoot_.DrawCentered(ToScreen(0.0f, -180.0f), kTitleHeight * TextScale(), WithAlpha(kTitleColor, fade_));
    DrawPanelRect(0.0f, -118.0f, 700.0f, 3.0f, kDividerColor);

    constexpr float kFirstItemY = -40.0f;
    constexpr float kItemSpacing = 84.0f;
    constexpr float kItemLeft = -300.0f;

    for (int i = 0; i < kRootItemCount; ++i)
    {
        const bool isSelected = (i == rootIndex_);
        const float localY = kFirstItemY + kItemSpacing * static_cast<float>(i);
        const float height = kRootItemHeight * (isSelected ? SelectionPulse() : 1.0f);

        if (isSelected)
        {
            // 選択中の目印
            DrawPanelRect(kItemLeft - 40.0f, localY, 16.0f, 40.0f, kMarkerColor);
        }

        const Vector2 itemPos = ToScreen(kItemLeft, localY);
        rootItems_[i].DrawLeft(itemPos.x, itemPos.y, height * TextScale(),
                               WithAlpha(isSelected ? kItemSelectedColor : kItemColor, fade_));
    }

    hintRoot_.DrawCentered(ToScreen(0.0f, 215.0f), kHintHeight * TextScale(), WithAlpha(kHintColor, fade_));
}

void PauseMenu::DrawSettingsPage()
{
    const float panelHeight = kSettingsPanelBase + kSettingsRowSpacing * static_cast<float>(kSettingItemCount - 1);
    const float panelTop = -panelHeight * 0.5f;

    DrawPanelRect(0.0f, 0.0f, kSettingsPanelWidth + kPanelBorder * 2.0f, panelHeight + kPanelBorder * 2.0f, kBorderColor);
    DrawPanelRect(0.0f, 0.0f, kSettingsPanelWidth, panelHeight, kPanelColor);

    titleSettings_.DrawCentered(ToScreen(0.0f, panelTop + kSettingsTitleOffset), kTitleHeight * TextScale(), WithAlpha(kTitleColor, fade_));
    DrawPanelRect(0.0f, panelTop + kSettingsDividerOffset, 820.0f, 3.0f, kDividerColor);

    const float firstRowY = panelTop + kSettingsFirstRowOffset;
    for (int i = 0; i < kSettingItemCount; ++i)
    {
        const float localY = firstRowY + kSettingsRowSpacing * static_cast<float>(i);
        DrawSettingRow(i, localY, i == settingIndex_);
    }

    const float lastRowY = firstRowY + kSettingsRowSpacing * static_cast<float>(kSettingItemCount - 1);
    hintSettings_.DrawCentered(ToScreen(0.0f, lastRowY + kSettingsHintGap), kHintHeight * TextScale(), WithAlpha(kHintColor, fade_));
}

void PauseMenu::DrawSettingRow(int item, float localY, bool isSelected)
{
    constexpr float kLabelLeft = -380.0f;
    constexpr float kGaugeLeft = 30.0f;
    constexpr float kGaugeWidth = 280.0f;
    constexpr float kGaugeHeight = 16.0f;
    constexpr float kValueRight = 420.0f;

    const float textScale = TextScale();
    const Vector4 textColor = WithAlpha(isSelected ? kItemSelectedColor : kItemColor, fade_);

    if (isSelected)
    {
        DrawPanelRect(kLabelLeft - 30.0f, localY, 12.0f, 34.0f, kMarkerColor);
    }

    // 「戻る」だけは値を持たないので中央寄せで置く
    if (item == kSettingBack)
    {
        const float height = kSettingLabelHeight * (isSelected ? SelectionPulse() : 1.0f);
        settingLabels_[item].DrawCentered(ToScreen(0.0f, localY), height * textScale, textColor);
        return;
    }

    const Vector2 labelPos = ToScreen(kLabelLeft, localY);
    settingLabels_[item].DrawLeft(labelPos.x, labelPos.y, kSettingLabelHeight * textScale, textColor);

    GameSettings *settings = GameSettings::GetInstance();

    // 振動は ON / OFF の表示だけ
    if (item == kSettingVibration)
    {
        GameUi::UiText &valueText = settings->IsVibrationEnabled() ? valueOn_ : valueOff_;
        const Vector2 valuePos = ToScreen(kValueRight, localY);
        const float width = valueText.WidthAt(kValueHeight * textScale);
        valueText.DrawCentered({valuePos.x - width * 0.5f, valuePos.y}, kValueHeight * textScale, textColor);
        return;
    }

    // ゲージと数値
    float ratio = 0.0f;
    std::string valueString;
    switch (item)
    {
    case kSettingMasterVolume:
        ratio = settings->GetMasterVolume();
        valueString = ToPercentText(ratio);
        break;

    case kSettingSensitivity:
        ratio = Normalize(settings->GetStickSensitivity(), GameSettings::kSensitivityMin, GameSettings::kSensitivityMax);
        valueString = ToScaleText(settings->GetStickSensitivity());
        break;

    default:
        break;
    }

    // ゲージは左端を固定したいので、中心をずらしながら幅を変える
    DrawPanelRect(kGaugeLeft + kGaugeWidth * 0.5f, localY, kGaugeWidth, kGaugeHeight, kGaugeBackColor);
    if (ratio > 0.0f)
    {
        const float fillWidth = kGaugeWidth * ratio;
        DrawPanelRect(kGaugeLeft + fillWidth * 0.5f, localY, fillWidth, kGaugeHeight,
                      isSelected ? kGaugeFillSelectedColor : kGaugeFillColor);
    }

    const Vector2 valuePos = ToScreen(kValueRight, localY);
    number_.DrawRight(valueString, valuePos.x, valuePos.y, kValueHeight * textScale, textColor);
}

void PauseMenu::DrawImGui()
{
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader("ポーズメニュー", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    const char *stateNames[] = {"閉じている", "開いている途中", "開いている", "閉じている途中"};
    ImGui::TextDisabled("状態: %s / スケール %.2f", stateNames[static_cast<int>(state_)], scale_);

    if (ImGui::Button("開く"))
    {
        Open();
    }
    ImGui::SameLine();
    if (ImGui::Button("閉じる"))
    {
        Close();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(パッドの START / ESC でも開閉)");

    ImGui::Separator();
    GameSettings::GetInstance()->DrawImGui();
#endif // USE_IMGUI
}
