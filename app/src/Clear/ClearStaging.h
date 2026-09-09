#pragma once
#include "src/Clear/ClearBossRemains.h"
#include "src/Clear/ClearPlayerActor.h"
#include "debug/param/GameParamHub.h"
#include "type/Vector3.h"
#include <memory>

namespace Hagine {
class BaseObjectManager;
class Camera;
class ViewProjection;
} // namespace Hagine

/// <summary>
/// クリア画面の絵作りをまとめて受け持つクラス。
///
/// 置くもの（喜んでいるプレイヤー・転がったボスのコアと殻）と、それを
/// どの画角で写すかを1か所に集めてある。シーン側は生成して毎フレーム
/// 呼ぶだけでよく、構図を詰める作業はここと ゲームパラメータ の Clear/… で完結する。
///
/// 構図は「地面すれすれの低いカメラで、手前左にプレイヤー・奥右にボスの残骸」。
/// カメラは注視点を持たせず位置と向きを直接置くので、演出の絵が動いてぶれない。
/// </summary>
class ClearStaging {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>
    /// 登場人物を作ってオブジェクトマネージャーへ預ける（更新と描画はそちらが行う）
    /// </summary>
    /// <param name="objectManager">登録先</param>
    void Init(Hagine::BaseObjectManager *objectManager);

    /// <summary>調整パラメータをデバッグUIへ登録する（Init の後に一度だけ）</summary>
    void RegisterParams();

    /// <summary>カメラを構図の位置へ置く（毎フレーム）</summary>
    /// <param name="camera">動かすカメラ</param>
    void UpdateCamera(Hagine::Camera *camera);

    /// <summary>殻の破片の形をGPUで作り直す（コンピュートフェーズから呼ぶ）</summary>
    void DispatchCompute();

    /// <summary>オブジェクトマネージャーが描かないもの（殻の破片）を描く</summary>
    /// <param name="viewProjection">ビュープロジェクション</param>
    void Draw(const Hagine::ViewProjection &viewProjection);

    /// <summary>構図の確認用UI（シーンの「オブジェクト設定」窓から呼ぶ）</summary>
    void DrawImGui();

    /// ===================================================
    /// getter
    /// ===================================================

    ClearPlayerActor *GetPlayer() const { return player_.get(); }
    ClearBossRemains *GetBossRemains() const { return bossRemains_.get(); }

private:
    /// <summary>調整した立ち位置を登場人物へ配る</summary>
    void ApplyLayout();

    std::unique_ptr<ClearPlayerActor> player_;
    std::unique_ptr<ClearBossRemains> bossRemains_;

    // --- 構図の調整パラメータ ---
    // 手前左のプレイヤーと奥右のボス。既定値は「カメラが -Z 側から +Z を向く」前提
    Hagine::Vector3 playerPosition_ = {-2.6f, 0.0f, -4.5f};
    Hagine::Vector3 bossPosition_ = {3.8f, 0.0f, 6.0f};
    Hagine::Vector3 cameraPosition_ = {0.0f, 1.3f, -10.0f};
    // カメラの向き（オイラー角・度）。x が正で見下ろし
    Hagine::Vector3 cameraRotation_ = {3.0f, 0.0f, 0.0f};
    float cameraFovDegrees_ = 55.0f;

    // 登録した調整パラメータは破棄時にまとめて解除する
    Hagine::GameParamOwner params_{"Clear/Staging"};
};
