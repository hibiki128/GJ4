#pragma once
#include "src/Boss/Attack/IBossAttack.h"
#include "src/Boss/Data/BossParameters.h"
#include "type/Vector3.h"

/// <summary>
/// 攻撃1: 回転＆突進。
///
/// 予兆（その場で自転を上げる）→ 突進 → 硬直、の3段階。
/// 突進方向は予兆の途中までプレイヤーを追尾し、終盤で固定する。
/// 固定後は方向が変わらないので、プレイヤーは横へ抜けて回避できる。
/// 色とは無関係で、当たり判定は本体との距離のみ。
/// </summary>
class BossAttackSpin final : public IBossAttack {
public:
    /// <summary>
    /// コンストラクタ
    /// </summary>
    /// <param name="params">パラメータ（ボスが保持するものを参照する。実行時調整が即反映される）</param>
    /// <param name="exposureParams">露出度スケーリングの係数</param>
    BossAttackSpin(const BossSpinAttackParams *params, const BossExposureParams *exposureParams)
        : pParams_(params), pExposureParams_(exposureParams) {}

    /// ===================================================
    /// IBossAttack
    /// ===================================================

    const char *GetName() const override { return "回転突進"; }
    void Start(const BossAttackContext &context) override;
    void Update(const BossAttackContext &context) override;
    bool IsFinished() const override { return phase_ == Phase::Finished; }
    void Cancel(const BossAttackContext &context) override;
    const char *GetPhaseName() const override;

private:
    /// <summary>攻撃の進行段階</summary>
    enum class Phase {
        Telegraph, // 予兆（自転を上げる／方向を定める）
        Dash,      // 突進
        Recover,   // 硬直
        Finished
    };

    /// ===================================================
    /// private method
    /// ===================================================

    void UpdateTelegraph(const BossAttackContext &context);
    void UpdateDash(const BossAttackContext &context);
    void UpdateRecover(const BossAttackContext &context);

    /// <summary>突進方向をプレイヤーへ向け直す（水平のみ）</summary>
    void AimAtTarget(const BossAttackContext &context);

    /// <summary>突進の予告線を描く</summary>
    void DrawTelegraph(const BossAttackContext &context, float intensity) const;

    /// ===================================================
    /// private variables
    /// ===================================================

    const BossSpinAttackParams *pParams_ = nullptr;
    const BossExposureParams *pExposureParams_ = nullptr;
    Phase phase_ = Phase::Finished;
    float timer_ = 0.0f;                                 // 現在段階の経過時間
    Hagine::Vector3 dashDirection_{0.0f, 0.0f, 1.0f};    // 突進方向（水平・正規化済み）
    float spinSpeed_ = 0.0f;                             // 現在の自転速度（度/秒）
    bool hitApplied_ = false;                            // この突進で既に当てたか

    // 露出度から決まる値。1回の攻撃の途中で変わらないよう開始時に確定させる
    float scaledTelegraphTime_ = 1.2f; // 予兆時間（露出度が上がるほど短い）
    float scaledDashSpeed_ = 22.0f;    // 突進速度（露出度が上がるほど速い）
};
