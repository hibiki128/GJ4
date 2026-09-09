#pragma once
#include "BaseScene.h"
#include "src/Field/FieldSurround.h"
#include "src/GameOver/GameOverStaging.h"
#include "src/UI/Result/ResultUi.h"
#include <memory>

/// <summary>
/// ゲームオーバーシーン
/// </summary>
class GameOverScene : public Hagine::BaseScene
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
    // 置くもの（見下ろすボス・倒れたプレイヤー）と、その画角
    std::unique_ptr<GameOverStaging> staging_;

    // 周りを囲む飾りの柱。ゲームシーンと同じ場所に立っているように見せる
    std::unique_ptr<FieldSurround> fieldSurround_;

    // 「げーむおーばー...」の見出しと、右下の2択
    std::unique_ptr<ResultUi> resultUi_;

    // 案内の並び順。ResultUi は番号しか返さないので、行き先はここで決める
    enum MenuIndex {
        kMenuRetry,       // もういちど
        kMenuReturnTitle, // はじめにもどる
    };

    // 次のシーンを予約したか（決定を何度も拾って二重に予約しないため）
    bool isChanging_ = false;
};
