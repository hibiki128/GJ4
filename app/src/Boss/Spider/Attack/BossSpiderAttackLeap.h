#pragma once
#include "src/Boss/Attack/IBossAttack.h"
#include "src/Boss/Data/BossParameters.h"
#include "type/Vector3.h"

/// <summary>
/// 蜘蛛の攻撃1: 跳ねまわる（距離を問わない）。
///
/// 沈み込み（助走のような予備動作）→ 着地点の真上まで飛び上がる → 真下へ落ちる → 着地、
/// を hopCount 回くり返す。着地点は相手のぴったり上ではなく「付近」に散らすので、
/// 相手は範囲から逃げられるが、当てずっぽうには避けられない。
/// 空中では脚を胴の下へ畳む。
/// </summary>
class BossSpiderAttackLeap final : public IBossAttack {
public:
    /// <summary>コンストラクタ</summary>
    /// <param name="params">パラメータ（蜘蛛が保持するものを参照する）</param>
    explicit BossSpiderAttackLeap(const BossSpiderLeapParams *params) : pParams_(params) {}

    const char *GetName() const override { return "跳ねまわる"; }
    void Start(const BossAttackContext &context) override;
    void Update(const BossAttackContext &context) override;
    bool IsFinished() const override { return phase_ == Phase::Finished; }
    void Cancel(const BossAttackContext &context) override;
    const char *GetPhaseName() const override;

private:
    /// <summary>攻撃の進行段階</summary>
    enum class Phase {
        Crouch,  // 沈み込み（予備動作）
        Rise,    // 着地点の真上まで飛び上がる
        Fall,    // 真下へ落ちる
        Impact,  // 着地（静止）
        Recover, // 最後の着地後の硬直
        Finished
    };

    /// ===================================================
    /// private method
    /// ===================================================

    /// <summary>次の着地点を決める（相手の付近へ散らす）</summary>
    void PickLandingPoint(const BossAttackContext &context);

    /// ===================================================
    /// private variables
    /// ===================================================

    const BossSpiderLeapParams *pParams_ = nullptr;
    Phase phase_ = Phase::Finished;
    float timer_ = 0.0f;             // 現在段階の経過時間
    int hopIndex_ = 0;               // 何回目の跳躍か

    Hagine::Vector3 phaseStart_{};   // 現在段階の開始位置
    Hagine::Vector3 landingPoint_{}; // 着地点（地面の高さ）
    Hagine::Vector3 apexPosition_{}; // 着地点の真上（飛び上がりの終点）
    float standHeight_ = 0.0f;       // 立っているときの胴の高さ
};
