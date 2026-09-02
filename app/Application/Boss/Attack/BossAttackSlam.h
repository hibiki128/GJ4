#pragma once
#include "Application/Boss/Attack/IBossAttack.h"
#include "Application/Boss/Data/BossParameters.h"
#include "object/base/BaseObject.h"
#include "type/Vector3.h"
#include <memory>

/// <summary>
/// 攻撃2: 飛び上がり→頭上落下（複数回）。
///
/// 上空へ飛び上がり → 落下地点を追尾 → 落下 → 着弾、を slamCount 回くり返す。
/// 落下地点は追尾のあいだだけ動き、落下開始時に固定される。
/// 地面に出す輪（Ringプリミティブ）が着弾位置の予告になる。
/// </summary>
class BossAttackSlam final : public IBossAttack {
public:
    /// <summary>
    /// コンストラクタ
    /// </summary>
    /// <param name="params">パラメータ（ボスが保持するものを参照する）</param>
    /// <param name="exposureParams">露出度スケーリングの係数</param>
    BossAttackSlam(const BossSlamAttackParams *params, const BossExposureParams *exposureParams)
        : pParams_(params), pExposureParams_(exposureParams) {}

    /// ===================================================
    /// IBossAttack
    /// ===================================================

    const char *GetName() const override { return "落下攻撃"; }
    void Start(const BossAttackContext &context) override;
    void Update(const BossAttackContext &context) override;
    bool IsFinished() const override { return phase_ == Phase::Finished; }
    void Cancel(const BossAttackContext &context) override;
    void Draw(const Hagine::ViewProjection &viewProjection) override;
    const char *GetPhaseName() const override;

    /// <summary>この一連の攻撃で落下する回数（露出度から決まる。デバッグ表示用）</summary>
    int GetSlamCount() const { return slamCount_; }

private:
    /// <summary>攻撃の進行段階</summary>
    enum class Phase {
        Rise,    // 飛び上がり
        Aim,     // 落下地点の追尾
        Fall,    // 落下
        Impact,  // 着弾（静止）
        Recover, // 最後の着弾後の硬直
        Finished
    };

    /// ===================================================
    /// private method
    /// ===================================================

    void UpdateRise(const BossAttackContext &context);
    void UpdateAim(const BossAttackContext &context);
    void UpdateFall(const BossAttackContext &context);
    void UpdateImpact(const BossAttackContext &context);
    void UpdateRecover(const BossAttackContext &context);

    /// <summary>落下地点をプレイヤーの真下へ更新する</summary>
    void UpdateLandingPoint(const BossAttackContext &context);

    /// <summary>着弾予告の輪を用意する（初回のみ生成）</summary>
    void EnsureMarker();

    /// <summary>着弾予告の輪の表示を更新する</summary>
    /// <param name="visible">表示するか</param>
    /// <param name="scale">輪の半径</param>
    void UpdateMarker(bool visible, float scale);

    /// ===================================================
    /// private variables
    /// ===================================================

    const BossSlamAttackParams *pParams_ = nullptr;
    const BossExposureParams *pExposureParams_ = nullptr;
    Phase phase_ = Phase::Finished;
    float timer_ = 0.0f;             // 現在段階の経過時間
    int slamIndex_ = 0;              // 何回目の落下か

    // 露出度から決まる値。1回の攻撃の途中で変わらないよう開始時に確定させる
    int slamCount_ = 2;              // 落下回数（露出度が上がるほど多い）
    float scaledAimTime_ = 0.5f;     // 狙いの時間（露出度が上がるほど短い）

    Hagine::Vector3 landingPoint_{}; // 落下地点（着弾時のボス中心）
    Hagine::Vector3 phaseStart_{};   // 現在段階の開始位置
    Hagine::Vector3 apexPosition_{}; // 上空での位置

    std::unique_ptr<Hagine::BaseObject> marker_; // 着弾予告の輪
};
