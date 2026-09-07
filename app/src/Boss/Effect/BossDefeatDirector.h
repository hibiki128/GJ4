#pragma once
#include "src/Boss/Data/BossParameters.h"
#include "2d/Sprite.h"
#include "type/Vector3.h"
#include <memory>

namespace Hagine {
class Camera;
}

/// <summary>
/// 撃破演出の画面まわりを受け持つ。
///
/// ・上下からシネマスコープのような黒帯が閉じる
/// ・専用カメラがコアへ寄り、手持ちのようにゆっくり揺れる
///
/// コア自体の動き（ふらつきながら落ちる）は BossSpider が持つ。
/// こちらは「見せ方」だけなので、演出を切ってもゲーム進行には影響しない。
/// </summary>
class BossDefeatDirector {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>専用カメラと黒帯を用意する（シーンの初期化から1回だけ呼ぶ）</summary>
    void Init();

    /// <summary>
    /// 演出を始める。ここから専用カメラに切り替わる
    /// </summary>
    /// <param name="focusPoint">寄っていく先（コアの位置）</param>
    /// <param name="from">寄り始めのカメラ位置（いまのカメラの位置を渡す）</param>
    /// <param name="holdPosition">寄りきったあと位置を動かさないか（登場演出では true）</param>
    /// <param name="holdDistance">位置を固定するときのコアからの距離</param>
    /// <param name="holdHeight">位置を固定するときのカメラの高さ（地面から）</param>
    void Begin(const Hagine::Vector3 &focusPoint, const Hagine::Vector3 &from, bool holdPosition = false,
               float holdDistance = 0.0f, float holdHeight = 0.0f);

    /// <summary>
    /// 演出を終わらせて、カメラをプレイヤーの側へ戻し始める。
    /// 黒帯もここから開いていく
    /// </summary>
    void BeginReturn();

    /// <summary>
    /// 戻り中の見た目を進める（戻りきったら true）
    /// </summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    /// <param name="cameraTo">戻り先のカメラ位置（追従カメラの現在位置）</param>
    /// <param name="lookTo">戻り先の注視点（プレイヤーの位置）</param>
    /// <param name="params">撃破演出のパラメータ</param>
    /// <returns>bool: 戻りきったら true</returns>
    bool UpdateReturn(float deltaTime, const Hagine::Vector3 &cameraTo, const Hagine::Vector3 &lookTo,
                      const BossSpiderDefeatParams &params);

    /// <summary>
    /// 1フレーム進める
    /// </summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    /// <param name="focusPoint">寄っていく先（落ちていくコアを追い続ける）</param>
    /// <param name="facingYaw">敵が向いている向き（ラジアン）。この正面へ回り込む</param>
    /// <param name="params">撃破演出のパラメータ</param>
    void Update(float deltaTime, const Hagine::Vector3 &focusPoint, float facingYaw,
                const BossSpiderDefeatParams &params);

    /// <summary>黒帯を描く（スプライトの描画フェーズから呼ぶ）</summary>
    void Draw();

    /// <summary>演出中か</summary>
    bool IsActive() const { return isActive_; }

    /// <summary>演出をやめて、黒帯を引っ込める</summary>
    void Stop();

    /// <summary>カメラをプレイヤーへ戻している最中か</summary>
    bool IsReturning() const { return isReturning_; }

private:
    /// ===================================================
    /// private variables
    /// ===================================================

    std::unique_ptr<Hagine::Sprite> topBar_;    // 上の黒帯
    std::unique_ptr<Hagine::Sprite> bottomBar_; // 下の黒帯
    Hagine::Camera *pCamera_ = nullptr;         // 演出用カメラ（CameraManager が所有）

    Hagine::Vector3 cameraFrom_{};    // 寄り始めのカメラ位置
    Hagine::Vector3 holdPosition_{};  // 位置を固定するときの、寄りきった位置
    Hagine::Vector3 returnFrom_{};    // 戻り始めのカメラ位置
    Hagine::Vector3 returnLookFrom_{}; // 戻り始めの注視点
    float holdDistance_ = 0.0f;       // 位置を固定するときのコアからの距離
    float holdHeight_ = 0.0f;         // 位置を固定するときのカメラの高さ（地面から）
    float elapsed_ = 0.0f;            // 演出を始めてからの経過時間（秒）
    float returnElapsed_ = 0.0f;      // 戻り始めてからの経過時間（秒）
    bool isActive_ = false;           // 演出中か
    bool isHoldPosition_ = false;     // 寄りきったあと位置を動かさないか
    bool isReturning_ = false;        // カメラを戻している最中か
    Hagine::Vector3 lastLookAt_{};    // 直近の注視点（戻りの始点に使う）
};
