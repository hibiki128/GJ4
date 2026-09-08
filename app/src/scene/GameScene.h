#pragma once
#include "BaseScene.h"
#include "src/Boss/Boss.h"
#include "src/Boss/Debug/BossTestDriver.h"
#include "src/Boss/Spider/BossSpider.h"
#include "src/Boss/Effect/BossDefeatDirector.h"
#include "src/Character/Player/Player.h"
#include "src/Interface/FunctionalPlayerBridge.h"
#include "src/Camera/Follow/FollowCamera.h"

/// <summary>
/// ゲームシーン
/// </summary>
class GameScene : public Hagine::BaseScene
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
    /// 第1形態（球体）を倒し切ったら、そのコアを第2形態（蜘蛛）へ引き渡す
    /// </summary>
    void UpdateFormChange();

    /// <summary>
    /// 第2形態の撃破演出（黒帯とカメラ寄せ）を進める
    /// </summary>
    void UpdateDefeatDirection();

    /// <summary>
    /// カメラの更新
    /// </summary>
    void CameraUpdate();

    /// <summary>
    /// シーン切り替え
    /// </summary>
    void ChangeScene();

private:
    std::unique_ptr<Player> player_;
    std::unique_ptr<GameInput> gameInput_;

    // ----- ボス（プレイヤー側の処理には触らず、インターフェース経由で連携する）-----
    std::unique_ptr<Boss> boss_;
    // プレイヤーの具象クラスへボスを依存させないためのアダプタ
    std::unique_ptr<FunctionalPlayerBridge> playerBridge_;
    // 連鎖マッチ検証用のデバッグ射撃（プレイヤーの射撃実装が入るまでの代役）
    std::unique_ptr<BossTestDriver> bossTestDriver_;
    // 第2形態（蜘蛛）。球体形態を倒したあとに出現させる
    std::unique_ptr<BossSpider> bossSpider_;
std::unique_ptr<BossDefeatDirector> defeatDirector_; // 撃破演出（黒帯・カメラ寄せ）
    bool isBossPaused_ = false;                          // 敵の更新を止めているか（調整用）
	std::unique_ptr<FollowCamera> followCamera_;
};
