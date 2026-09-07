#include "GameSettings.h"
#include <Audio.h>
#include <Input.h>
#include <data/DataHandler.h>
#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

using namespace Hagine;

const std::string GameSettings::kJsonFolder = "Settings";
const std::string GameSettings::kJsonFile = "GameSettings";

namespace {
/// <summary>
/// 振動モーターの強さ (0.0f 〜 1.0f) を XInput の 0 〜 65535 へ変換する
/// </summary>
WORD ToMotorSpeed(float strength)
{
    const float clamped = std::clamp(strength, 0.0f, 1.0f);
    return static_cast<WORD>(clamped * 65535.0f);
}
} // namespace

GameSettings *GameSettings::GetInstance()
{
    static GameSettings instance;
    return &instance;
}

void GameSettings::Initialize()
{
    if (isInitialized_)
    {
        return;
    }
    isInitialized_ = true;

    DataHandler data(kJsonFolder, kJsonFile);
    masterVolume_ = data.Load("masterVolume", masterVolume_);
    stickSensitivity_ = data.Load("stickSensitivity", stickSensitivity_);
    isVibrationEnabled_ = data.Load("vibrationEnabled", isVibrationEnabled_);
    vibrationStrength_ = data.Load("vibrationStrength", vibrationStrength_);

    Apply();
}

void GameSettings::Apply()
{
    Audio::GetInstance()->SetMasterVolume(masterVolume_);

    if (GamePad *gamePad = Input::GetInstance()->GetGamePad())
    {
        gamePad->SetStickSensitivity(stickSensitivity_);
        gamePad->SetVibrationEnabled(isVibrationEnabled_);
    }
}

void GameSettings::Save()
{
    DataHandler data(kJsonFolder, kJsonFile);
    data.Save("masterVolume", masterVolume_);
    data.Save("stickSensitivity", stickSensitivity_);
    // 以前は保存していたデッドゾーンを、残っていたら消しておく
    data.Remove("leftDeadZone");
    data.Remove("rightDeadZone");
    data.Save("vibrationEnabled", isVibrationEnabled_);
    data.Save("vibrationStrength", vibrationStrength_);
    // デストラクタでも書き出されるが、設定画面を閉じた時点で確実に残したいので明示する
    data.Flush();

    isDirty_ = false;
    saveTimer_ = 0.0f;
}

void GameSettings::Update(float deltaTime)
{
    // 振動の鳴らしっぱなしを防ぐ
    if (vibrationTimer_ > 0.0f)
    {
        vibrationTimer_ -= deltaTime;
        if (vibrationTimer_ <= 0.0f)
        {
            vibrationTimer_ = 0.0f;
            if (GamePad *gamePad = Input::GetInstance()->GetGamePad())
            {
                gamePad->StopVibration();
            }
        }
    }

    // 変更が落ち着いてからまとめて書き出す
    if (isDirty_)
    {
        saveTimer_ -= deltaTime;
        if (saveTimer_ <= 0.0f)
        {
            Save();
        }
    }
}

void GameSettings::RequestVibration(float strength, float durationSec)
{
    if (!isVibrationEnabled_ || durationSec <= 0.0f)
    {
        return;
    }

    GamePad *gamePad = Input::GetInstance()->GetGamePad();
    if (!gamePad)
    {
        return;
    }

    // 設定側の強さを掛けてから左右のモーターへ流す
    const WORD motor = ToMotorSpeed(strength * vibrationStrength_);
    gamePad->SetVibration(motor, motor);
    vibrationTimer_ = std::max(vibrationTimer_, durationSec);
}

void GameSettings::SetMasterVolume(float volume)
{
    masterVolume_ = std::clamp(volume, 0.0f, 1.0f);
    Audio::GetInstance()->SetMasterVolume(masterVolume_);
    isDirty_ = true;
    saveTimer_ = kSaveDelay;
}

void GameSettings::SetStickSensitivity(float sensitivity)
{
    stickSensitivity_ = std::clamp(sensitivity, kSensitivityMin, kSensitivityMax);
    if (GamePad *gamePad = Input::GetInstance()->GetGamePad())
    {
        gamePad->SetStickSensitivity(stickSensitivity_);
    }
    isDirty_ = true;
    saveTimer_ = kSaveDelay;
}

void GameSettings::SetVibrationEnabled(bool enabled)
{
    isVibrationEnabled_ = enabled;
    if (GamePad *gamePad = Input::GetInstance()->GetGamePad())
    {
        gamePad->SetVibrationEnabled(isVibrationEnabled_);
    }
    if (!isVibrationEnabled_)
    {
        vibrationTimer_ = 0.0f;
    }
    isDirty_ = true;
    saveTimer_ = kSaveDelay;
}

void GameSettings::SetVibrationStrength(float strength)
{
    vibrationStrength_ = std::clamp(strength, 0.0f, 1.0f);
    isDirty_ = true;
    saveTimer_ = kSaveDelay;
}

void GameSettings::DrawImGui()
{
#ifdef USE_IMGUI
    if (ImGui::CollapsingHeader("サウンド", ImGuiTreeNodeFlags_DefaultOpen))
    {
        float volume = masterVolume_;
        if (ImGui::SliderFloat("全体の音量", &volume, 0.0f, 1.0f, "%.2f"))
        {
            SetMasterVolume(volume);
        }
    }

    if (ImGui::CollapsingHeader("コントローラー", ImGuiTreeNodeFlags_DefaultOpen))
    {
        GamePad *gamePad = Input::GetInstance()->GetGamePad();
        ImGui::TextDisabled("接続: %s", (gamePad && gamePad->IsConnected()) ? "あり" : "なし");

        float sensitivity = stickSensitivity_;
        if (ImGui::SliderFloat("スティック感度", &sensitivity, kSensitivityMin, kSensitivityMax, "%.2f"))
        {
            SetStickSensitivity(sensitivity);
        }

        bool vibration = isVibrationEnabled_;
        if (ImGui::Checkbox("振動", &vibration))
        {
            SetVibrationEnabled(vibration);
        }

        float vibrationStrength = vibrationStrength_;
        if (ImGui::SliderFloat("振動の強さ", &vibrationStrength, 0.0f, 1.0f, "%.2f"))
        {
            SetVibrationStrength(vibrationStrength);
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("試す"))
        {
            RequestVibration(1.0f, 0.35f);
        }

        // 設定した値が実際の入力へどう効いているかをその場で確認できるようにする
        if (gamePad)
        {
            ImGui::TextDisabled("L(%.2f, %.2f)  R(%.2f, %.2f)",
                                gamePad->GetLeftStickX(), gamePad->GetLeftStickY(),
                                gamePad->GetRightStickX(), gamePad->GetRightStickY());
        }
    }
#endif // USE_IMGUI
}
