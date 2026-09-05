#pragma once
#include "src/Boss/Attack/IBossAttack.h"
#include "src/Boss/Data/BossParameters.h"

/// <summary>
/// 蜘蛛の攻撃3: 脚を広げて回転しながら接近（距離を問わない・稀に出す）。
///
/// 予備動作でゆっくり脚を広げ、そのまま回りながら相手へ近づく。
/// 広げた脚の届く範囲がそのまま攻撃範囲になるので、
/// 相手は「脚の届かない外側」へ逃げるしかない。
/// 胴は地面へ寄せて、相手と同じ高さの脅威になるようにする。
/// </summary>
class BossSpiderAttackWhirl final : public IBossAttack {
public:
    /// <summary>コンストラクタ</summary>
    /// <param name="params">パラメータ（蜘蛛が保持するものを参照する）</param>
    explicit BossSpiderAttackWhirl(const BossSpiderWhirlParams *params) : pParams_(params) {}

    const char *GetName() const override { return "回転して接近"; }
    void Start(const BossAttackContext &context) override;
    void Update(const BossAttackContext &context) override;
    bool IsFinished() const override { return phase_ == Phase::Finished; }
    void Cancel(const BossAttackContext &context) override;
    const char *GetPhaseName() const override;

    /// <summary>いま脚が届く範囲（攻撃範囲。デバッグ表示用）</summary>
    float GetReachRadius() const { return reachRadius_; }

private:
    /// <summary>攻撃の進行段階</summary>
    enum class Phase {
        Telegraph, // 脚を広げる（遅め）
        Spin,      // 回転しながら接近
        Recover,   // 脚を戻す
        Finished
    };

    const BossSpiderWhirlParams *pParams_ = nullptr;
    Phase phase_ = Phase::Finished;
    float timer_ = 0.0f;         // 現在段階の経過時間
    float standHeight_ = 0.0f;   // 立っているときの胴の高さ
    float reachRadius_ = 0.0f;   // いま脚が届く範囲
};
