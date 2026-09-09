#pragma once
#include "BaseScene.h"
#include "src/Clear/ClearStaging.h"
#include "src/Field/FieldSurround.h"
#include "src/UI/Result/ResultUi.h"
#include <memory>

/// <summary>
/// クリアシーン
/// </summary>
class ClearScene : public Hagine::BaseScene
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
    // 置くもの（喜んでいるプレイヤー・転がったボス）と、その画角
    std::unique_ptr<ClearStaging> staging_;

    // 周りを囲む飾りの柱。ゲームシーンと同じ場所に立っているように見せる
    std::unique_ptr<FieldSurround> fieldSurround_;

    // 「くりあ！！」の見出しと、右下の案内
    std::unique_ptr<ResultUi> resultUi_;

    // 次のシーンを予約したか（決定を何度も拾って二重に予約しないため）
    bool isChanging_ = false;
};
