#pragma once
#include "src/Boss/Attack/IBossAttack.h"
#include "src/Boss/Data/BossParameters.h"

/// <summary>
/// 蜘蛛の攻撃2: 色つきの弾を撃つ（遠距離）。
///
/// 大きめの弾をゆっくり何発か飛ばす。弾の色は蜘蛛の使う色から選ぶので、
/// プレイヤーは同じ色を当てて消せる（消す処理は BossSpider 側の当たり判定が持つ）。
/// </summary>
class BossSpiderAttackShoot final : public IBossAttack {
public:
    /// <summary>コンストラクタ</summary>
    /// <param name="params">パラメータ（蜘蛛が保持するものを参照する）</param>
    explicit BossSpiderAttackShoot(const BossSpiderShootParams *params) : pParams_(params) {}

    const char *GetName() const override { return "弾を撃つ"; }
    void Start(const BossAttackContext &context) override;
    void Update(const BossAttackContext &context) override;
    bool IsFinished() const override { return phase_ == Phase::Finished; }
    void Cancel(const BossAttackContext &context) override;
    const char *GetPhaseName() const override;

private:
    /// <summary>攻撃の進行段階</summary>
    enum class Phase {
        Telegraph, // 溜め
        Fire,      // 連続発射
        Recover,   // 撃ち終わりの硬直
        Finished
    };

    const BossSpiderShootParams *pParams_ = nullptr;
    Phase phase_ = Phase::Finished;
    float timer_ = 0.0f;      // 現在段階の経過時間
    float shotTimer_ = 0.0f;  // 次の1発までの経過時間
    int firedCount_ = 0;      // 撃った数
};
