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
    void Begin(const Hagine::Vector3 &focusPoint, const Hagine::Vector3 &from);

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

private:
    /// ===================================================
    /// private variables
    /// ===================================================

    std::unique_ptr<Hagine::Sprite> topBar_;    // 上の黒帯
    std::unique_ptr<Hagine::Sprite> bottomBar_; // 下の黒帯
    Hagine::Camera *pCamera_ = nullptr;         // 演出用カメラ（CameraManager が所有）

    Hagine::Vector3 cameraFrom_{}; // 寄り始めのカメラ位置
    float elapsed_ = 0.0f;         // 演出を始めてからの経過時間（秒）
    bool isActive_ = false;        // 演出中か
};
