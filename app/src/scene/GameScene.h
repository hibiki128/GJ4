#pragma once
#include "BaseScene.h"
#include "src/Boss/Boss.h"
#include "src/Boss/Spider/BossSpider.h"
#include "src/Boss/Effect/BossDefeatDirector.h"
#include "src/Character/Player/Player.h"
#include "src/Field/Field.h"
#include "src/Field/FieldSurround.h"
#include "src/Interface/FunctionalPlayerBridge.h"
#include "src/Camera/Follow/FollowCamera.h"
#include "src/UI/Damage/DamageVignette.h"
#include "src/Effect/PerfectDodgeDirector.h"

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
    /// 照準（カメラの射線）をプレイヤーへ配る。
    /// プレイヤーはカメラを知らないので、カメラを動かした後にシーンから渡す
    /// </summary>
    void UpdateAim();

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

    // プレイヤーと敵を閉じ込める円柱の外周（見た目は線だけ）
    std::unique_ptr<Field> field_;

    // その外側をぐるりと囲む飾りの柱（背景のクリアカラーを塞ぐ役も兼ねる）
    std::unique_ptr<FieldSurround> fieldSurround_;

    // ----- ボス（プレイヤー側の処理には触らず、インターフェース経由で連携する）-----
    std::unique_ptr<Boss> boss_;
    // プレイヤーの具象クラスへボスを依存させないためのアダプタ
    std::unique_ptr<FunctionalPlayerBridge> playerBridge_;
    // 第2形態（蜘蛛）。球体形態を倒したあとに出現させる
    std::unique_ptr<BossSpider> bossSpider_;
std::unique_ptr<BossDefeatDirector> defeatDirector_; // 撃破演出（黒帯・カメラ寄せ）
    bool isBossPaused_ = false;                          // 敵の更新を止めているか（調整用）
	std::unique_ptr<FollowCamera> followCamera_;
	// 被弾したときに画面のふちを赤く染めるマスク
	std::unique_ptr<DamageVignette> damageVignette_;

	// ジャスト回避の画面演出（白フラッシュ）
	std::unique_ptr<PerfectDodgeDirector> perfectDodge_;
};
