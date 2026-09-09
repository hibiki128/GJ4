#pragma once
#include "src/GameOver/GameOverBossActor.h"
#include "src/GameOver/GameOverContext.h"
#include "src/GameOver/GameOverPlayerActor.h"
#include "debug/param/GameParamHub.h"
#include "type/Vector3.h"
#include <memory>

namespace Hagine {
class BaseObjectManager;
class Camera;
} // namespace Hagine

/// <summary>
/// ゲームオーバー画面の絵作りをまとめて受け持つクラス。
///
/// クリア画面の ClearStaging と対になる。置くものが入れ替わっていて、
/// 手前左に勝ったボス・奥右に倒れたプレイヤーを置く。
///
/// 構図は形態ごとに別に持つ。第2形態（蜘蛛）は胴の高さだけで 5〜6 あり、
/// 第1形態と同じ画角では画面から溢れるので、蜘蛛のときはカメラを引いて
/// 見上げる角度にしてある。調整値も形態ごとに別の名前で保存される。
/// </summary>
class GameOverStaging {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>
    /// 負けた相手の形態を見て登場人物を作り、オブジェクトマネージャーへ預ける
    /// </summary>
    /// <param name="form">負けた瞬間に出ていた形態</param>
    /// <param name="objectManager">登録先</param>
    void Init(BossFormId form, Hagine::BaseObjectManager *objectManager);

    /// <summary>調整パラメータをデバッグUIへ登録する（Init の後に一度だけ）</summary>
    void RegisterParams();

    /// <summary>
    /// 出す形態を入れ替える（構図もその形態のものへ切り替わる）。
    /// 本番はゲームシーンで負けた形態が入るが、確認用にUIからも呼べる
    /// </summary>
    /// <param name="form">出す形態</param>
    void SetForm(BossFormId form);

    /// <summary>ボスの動きを進める（オブジェクトの更新より前に呼ぶこと）</summary>
    void Update();

    /// <summary>カメラを構図の位置へ置く（毎フレーム）</summary>
    /// <param name="camera">動かすカメラ</param>
    void UpdateCamera(Hagine::Camera *camera);

    /// <summary>殻の形をGPUで作り直す（コンピュートフェーズから呼ぶ）</summary>
    void DispatchCompute();

    /// <summary>構図の確認用UI（シーンの「オブジェクト設定」窓から呼ぶ）</summary>
    void DrawImGui();

    /// ===================================================
    /// getter
    /// ===================================================

    GameOverPlayerActor *GetPlayer() const { return player_.get(); }
    GameOverBossActor *GetBoss() const { return boss_.get(); }
    BossFormId GetForm() const { return form_; }

private:
    /// <summary>1つの形態ぶんの構図</summary>
    struct Layout {
        // クリア画面と手前・奥を入れ替えた配置。「カメラが -Z 側から +Z を向く」前提
        Hagine::Vector3 bossPosition{};
        Hagine::Vector3 playerPosition{};
        Hagine::Vector3 cameraPosition{};
        // カメラの向き（オイラー角・度）。x が正で見下ろし、負で見上げ
        Hagine::Vector3 cameraRotation{};
        float fovDegrees = 55.0f;
    };

    /// <summary>形態を配列の添字にする</summary>
    static size_t ToIndex(BossFormId form) { return (form == BossFormId::Spider) ? 1u : 0u; }

    /// <summary>いま出している形態の構図</summary>
    Layout &CurrentLayout() { return layouts_[ToIndex(form_)]; }
    const Layout &CurrentLayout() const { return layouts_[ToIndex(form_)]; }

    /// <summary>いまの構図を登場人物へ配る</summary>
    void ApplyLayout();

    BossFormId form_ = BossFormId::Sphere;
    std::unique_ptr<GameOverPlayerActor> player_;
    std::unique_ptr<GameOverBossActor> boss_;

    // 構図は形態ごと（0=球体 / 1=蜘蛛）。既定値は Init で入れる
    Layout layouts_[2]{};

    // 登録した調整パラメータは破棄時にまとめて解除する（形態ごとに分ける）
    Hagine::GameParamOwner layoutParams_[2]{Hagine::GameParamOwner{"GameOver/Staging/Sphere"},
                                            Hagine::GameParamOwner{"GameOver/Staging/Spider"}};
};
