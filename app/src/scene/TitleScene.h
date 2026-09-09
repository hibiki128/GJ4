#pragma once
#include "BaseScene.h"
#include "src/Boss/Boss.h"
#include "src/Field/Field.h"
#include "src/Field/FieldSurround.h"
#include "src/Title/TitleLogo.h"
#include "src/Title/TitlePlayerActor.h"
#include "src/Title/TitleStartPrompt.h"
#include "src/Title/TitleTutorialDialog.h"
#include "debug/param/GameParamHub.h"
#include <memory>
#include <string>

/// <summary>
/// タイトルシーン
/// </summary>
class TitleScene : public Hagine::BaseScene
{
public:
    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>
    /// 初期化
    /// </summary>
    void Initialize() override;

    /// <summary>
    /// 終了処理
    /// </summary>
    void Finalize() override;

    /// <summary>
    /// 更新処理
    /// </summary>
    void Update() override;

    /// <summary>
    /// 描画処理
    /// </summary>
    void Draw() override {};

    /// <summary>
    /// オフスクリーン描画処理
    /// </summary>
    void DrawForOffScreen() override {};

    /// <summary>
    /// シーン設定を追加
    /// </summary>
    void AddSceneSetting() override;

    /// <summary>
    /// オブジェクト設定を追加
    /// </summary>
    void AddObjectSetting() override;

    /// <summary>
    /// パーティクル設定を追加
    /// </summary>
    void AddParticleSetting() override;

    /// <summary>
    /// カメラの更新
    /// </summary>
    void CameraUpdate();

    /// <summary>
    /// シーン切り替え
    /// </summary>
    void ChangeScene();

private:
    /// ===================================================
    /// private method
    /// ===================================================

    /// <summary>置き場所と向きをいまのパラメータへ合わせ直す</summary>
    void ApplyLayout();

    /// <summary>Aを押してから行き先が決まるまでを進める</summary>
    void UpdateStart();

    /// <summary>「はじめる」を押したか（パッドのA・Enter・スペース）</summary>
    static bool IsDecidePressed();

    /// ===================================================
    /// private variables
    /// ===================================================

    // 戦う場所の外周。ゲーム中と同じものを線だけ出す
    std::unique_ptr<Field> field_;

    // その外側を囲む飾りの柱と、背景を塞ぐ球。
    // ゲーム中と同じ広さ・同じ見た目にして「あの舞台」に見せる
    std::unique_ptr<FieldSurround> fieldSurround_;

    // ゲーム中と同じ球体形態のボス。状態遷移と攻撃は止めて、殻の脈打ちだけを見せる
    std::unique_ptr<Boss> boss_;

    // ゲーム中と同じ揺れ方をするプレイヤー役
    std::unique_ptr<TitlePlayerActor> player_;

    // ロゴ「からぽっぷ」。1文字ずつ落ちてきて、ときどき順にはじける
    std::unique_ptr<TitleLogo> logo_;

    // 中央下のAボタンの案内。透明度をゆっくり上げ下げする
    std::unique_ptr<TitleStartPrompt> startPrompt_;

    // Aを押したら出す「チュートリアルをプレイしますか？」
    std::unique_ptr<TitleTutorialDialog> tutorialDialog_;

    // 行き先（決まるまで空）と、予約済みかどうか（二重予約を防ぐ）
    std::string nextSceneName_{};
    bool isNextSceneReserved_ = false;

    // --- 構図（デバッグUIから触って保存できる）---
    Hagine::Vector3 bossPosition_ = {6.0f, 0.0f, 6.0f};      // ボスの立ち位置（高さは自動）
    Hagine::Vector3 playerPosition_ = {-7.0f, 0.0f, -9.0f};  // プレイヤーの立ち位置
    Hagine::Vector3 cameraPosition_ = {-14.0f, 7.0f, -26.0f}; // カメラの位置
    Hagine::Vector3 cameraRotation_ = {8.0f, 25.0f, 0.0f};    // カメラの向き（度）
    float cameraFovDegrees_ = 45.0f;                          // 画角（度）
    float bossSpinSpeed_ = 12.0f;                             // ボスの自転（度/秒）

    Hagine::GameParamOwner params_{"Title/Layout"};
};
