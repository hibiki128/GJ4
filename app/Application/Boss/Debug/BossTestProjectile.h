#pragma once
#include "object/base/BaseObject.h"
#include <functional>
#include <string>

/// <summary>
/// 殻への付着を検証するための疑似弾（デバッグ専用）。
///
/// プレイヤーの射撃は別担当のため、本実装が入るまでの検証手段としてボス側に置いている。
/// 本番の弾も「前フレームの位置→現在位置の線分をボスへ渡す」という同じ経路
/// （IBossTargetQuery::RaycastAttach）を通るので、この弾はそのまま参照実装として使える。
///
/// コライダーは持たない。弾は1フレームで球の直径以上進むため、
/// 重なり判定ではすり抜けるのに対し、線分なら確実に当たる。
/// </summary>
class BossTestProjectile final : public Hagine::BaseObject {
public:
    /// <summary>ロックオン対象の現在位置を取得する関数（対象が消えたら false を返す）</summary>
    using TargetPositionGetter = std::function<bool(Hagine::Vector3 &)>;

    /// <summary>
    /// 移動した線分でボスへ着弾を問い合わせる関数。
    /// 弾を消してよい場合に true を返す
    /// </summary>
    using HitTester = std::function<bool(const Hagine::Vector3 &from, const Hagine::Vector3 &to)>;

    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>弾を生成する</summary>
    /// <param name="objectName">オブジェクト名（一意）</param>
    /// <param name="rgba">表示色</param>
    /// <param name="radius">弾の半径</param>
    void InitProjectile(const std::string &objectName, const Hagine::Vector4 &rgba, float radius);

    /// <summary>
    /// 発射する
    /// </summary>
    /// <param name="origin">発射位置</param>
    /// <param name="direction">初速の向き</param>
    /// <param name="speed">速度（単位/秒）</param>
    /// <param name="lifeTime">寿命（秒）</param>
    /// <param name="correctionRate">軌道補正の強さ（1秒あたりの補正割合）</param>
    void Fire(const Hagine::Vector3 &origin, const Hagine::Vector3 &direction,
              float speed, float lifeTime, float correctionRate);

    /// <summary>更新（軌道補正・移動・着弾問い合わせ・寿命）</summary>
    void Update() override;

    /// <summary>役目を終えたか（着弾・寿命切れ）</summary>
    bool IsFinished() const { return isFinished_; }

    /// ===================================================
    /// setter
    /// ===================================================

    void SetTargetPositionGetter(TargetPositionGetter getter) { targetPositionGetter_ = std::move(getter); }
    void SetHitTester(HitTester tester) { hitTester_ = std::move(tester); }

private:
    /// <summary>着弾・寿命切れで動きを止める</summary>
    void Finish();

    /// ===================================================
    /// private variables
    /// ===================================================

    Hagine::Vector3 direction_{0.0f, 0.0f, 1.0f}; // 進行方向（正規化済み）
    float speed_ = 40.0f;                          // 速度（単位/秒）
    float lifeTime_ = 3.0f;                        // 残り寿命（秒）
    float correctionRate_ = 8.0f;                  // 軌道補正の強さ
    bool isFinished_ = false;                      // 役目を終えたか

    TargetPositionGetter targetPositionGetter_{}; // ロックオン対象の追従先
    HitTester hitTester_{};                       // 着弾の問い合わせ先
};
