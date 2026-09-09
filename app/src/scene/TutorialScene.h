#pragma once
#include "BaseScene.h"
#include "src/Boss/Boss.h"
#include "src/Camera/Follow/FollowCamera.h"
#include "src/Character/Player/Player.h"
#include "src/Field/Field.h"
#include "src/Field/FieldSurround.h"
#include "src/Interface/FunctionalPlayerBridge.h"
#include "src/Tutorial/TutorialDirector.h"
#include "src/UI/Reticle/PlayerReticle.h"

/// <summary>
/// チュートリアルシーン。
///
/// 本編（GameScene）と同じ操作・同じ的で、順番に1つずつ教える。
/// 進行の中身（何を・どの順で・どこまでやれば達成か）は TutorialDirector が持っていて、
/// このシーンは「今フレーム何が起きたか」を集めて渡す係。
///
/// ボスは第1形態だけを出し、攻撃だけを止めてある。SetPaused で丸ごと止めると
/// 登場状態から抜けられず、RaycastAttach が IsAppearing() で弾いてしまって
/// いくら撃っても当たらない。攻撃だけ切れば、登場演出も怯みも撃破も普通に動くまま、
/// 反撃だけしてこない的になる。
/// </summary>
class TutorialScene : public Hagine::BaseScene {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override {};
    void DrawForOffScreen() override {};

    /// <summary>シーン設定を追加</summary>
    void AddSceneSetting() override;

    /// <summary>オブジェクト設定を追加</summary>
    void AddObjectSetting() override;

    /// <summary>パーティクル設定を追加</summary>
    void AddParticleSetting() override;

private:
    /// ===================================================
    /// private method
    /// ===================================================

    /// <summary>カメラの更新</summary>
    void CameraUpdate();

    /// <summary>照準（カメラの射線）をプレイヤーへ配る</summary>
    void UpdateAim();

    /// <summary>
    /// 今フレームに起きたことを集めて、進行役へ渡す形にする
    /// </summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    /// <returns>TutorialSignals: 進行役へ渡す内容</returns>
    TutorialSignals CollectSignals(float deltaTime);

    /// ===================================================
    /// private variables
    /// ===================================================

    std::unique_ptr<Player> player_;
    std::unique_ptr<GameInput> gameInput_;
    std::unique_ptr<Field> field_;
    std::unique_ptr<FieldSurround> fieldSurround_;
    std::unique_ptr<Boss> boss_;
    std::unique_ptr<FunctionalPlayerBridge> playerBridge_;
    std::unique_ptr<FollowCamera> followCamera_;
    std::unique_ptr<TutorialDirector> tutorial_;
    std::unique_ptr<PlayerReticle> reticle_;

    // 殻がどれだけ削れたかを前フレームと比べて「消えた瞬間」を拾う。
    // 殻が減るのは同色がそろって消えたときだけなので、これで連鎖の成立が分かる
    float previousExposure_ = 0.0f;

    // 完了テロップを見せてから本編へ送るまでの残り時間（負なら未完了）
    float finishWait_ = -1.0f;

    // ボスに攻撃させるか（チュートリアルなので既定は攻撃なし）
    bool bossAttacks_ = false;

    /// <summary>レティクルを出してよい場面か</summary>
    bool ShouldDrawReticle() const;
};
