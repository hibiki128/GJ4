#pragma once
#include <string>

/// <summary>
/// ゲーム全体の設定（音量・コントローラー）を保持するシングルトン。
///
/// 値を書き換えたら Apply() でエンジン側（Audio / GamePad）へ反映し、
/// Save() で Assets/jsons/Settings/GameSettings.json へ書き出す。
/// 設定画面（PauseMenu）と ImGui のどちらから触っても同じここを見る。
/// </summary>
class GameSettings
{
public:
    /// ===================================================
    /// 設定値の範囲（設定画面のゲージ表示と共用する）
    /// ===================================================
    static constexpr float kSensitivityMin = 0.25f;  // スティック感度の下限
    static constexpr float kSensitivityMax = 3.0f;   // スティック感度の上限

    /// <summary>
    /// シングルトンインスタンスの取得
    /// </summary>
    static GameSettings *GetInstance();

    /// <summary>
    /// JSONから読み込んでエンジンへ反映する（2回目以降の呼び出しは何もしない）
    /// </summary>
    void Initialize();

    /// <summary>
    /// 保持している値をエンジン（Audio / GamePad）へ反映する
    /// </summary>
    void Apply();

    /// <summary>
    /// 現在の値をJSONへ書き出す
    /// </summary>
    void Save();

    /// <summary>
    /// 変更を「あとで書き出す」ものとして予約する。
    /// スライダーを動かしている最中に毎フレーム書き出さないための仕組みで、
    /// 実際の書き出しは Update() が少し待ってから行う
    /// </summary>
    void MarkDirty() { isDirty_ = true; }

    /// <summary>
    /// 振動の残り時間を進める。毎フレーム呼ぶ
    /// </summary>
    /// <param name="deltaTime">前フレームからの経過秒数</param>
    void Update(float deltaTime);

    /// <summary>
    /// 振動をリクエストする。設定が「振動なし」なら何も起きない
    /// </summary>
    /// <param name="strength">強さ (0.0f 〜 1.0f)。設定側の強さが更に掛かる</param>
    /// <param name="durationSec">鳴らす秒数</param>
    void RequestVibration(float strength, float durationSec);

    /// <summary>
    /// 設定項目をImGuiで描画する（ウィンドウは呼び出し側で開いておくこと）。
    /// ImGui無しのビルドでは何もしない
    /// </summary>
    void DrawImGui();

    /// ===================================================
    /// Getter
    /// ===================================================
    float GetMasterVolume() const { return masterVolume_; }
    float GetStickSensitivity() const { return stickSensitivity_; }
    bool IsVibrationEnabled() const { return isVibrationEnabled_; }
    float GetVibrationStrength() const { return vibrationStrength_; }

    /// ===================================================
    /// Setter（いずれも設定した値をそのままエンジンへ反映する）
    /// ===================================================
    void SetMasterVolume(float volume);
    void SetStickSensitivity(float sensitivity);
    void SetVibrationEnabled(bool enabled);
    void SetVibrationStrength(float strength);

private:
    GameSettings() = default;
    ~GameSettings() = default;
    GameSettings(const GameSettings &) = delete;
    GameSettings &operator=(const GameSettings &) = delete;

    /// <summary>
    /// 保存先のJSON（フォルダ "Settings" / ファイル "GameSettings"）
    /// </summary>
    static const std::string kJsonFolder;
    static const std::string kJsonFile;

    // ----- 音 -----
    float masterVolume_ = 0.8f; // マスター音量 (0.0f 〜 1.0f)

    // ----- コントローラー -----
    // デッドゾーンは設定項目に出さない（エンジン側の XInput 標準値をそのまま使う）
    float stickSensitivity_ = 1.0f;   // スティック感度（入力値に掛かる倍率）
    bool isVibrationEnabled_ = true;  // 振動を鳴らすか
    float vibrationStrength_ = 0.7f;  // 振動の強さ (0.0f 〜 1.0f)

    // ----- 実行時の状態 -----
    float vibrationTimer_ = 0.0f; // 振動の残り秒数
    bool isInitialized_ = false;  // Initialize 済みか
    bool isDirty_ = false;        // 未保存の変更があるか
    float saveTimer_ = 0.0f;      // 書き出しまでの待ち時間

    // 値をいじっている間に書き出しが走らないよう、最後の変更からこの秒数だけ待つ
    static constexpr float kSaveDelay = 0.5f;
};
