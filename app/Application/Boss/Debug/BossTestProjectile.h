#pragma once
#include "object/base/BaseObject.h"
#include <functional>
#include <string>

/// <summary>
/// 連鎖マッチ検証用の疑似弾（デバッグ専用）。
///
/// プレイヤーの射撃は別担当のため、本実装が入るまでの検証手段としてボス側に置いている。
/// 本番の弾も「ソフトロックオンで得た対象へ軌道補正し、コライダーで命中判定」という
/// 同じ経路（IBossTargetQuery）を通るので、この弾はそのまま参照実装として使える。
/// </summary>
class BossTestProjectile final : public Hagine::BaseObject {
public:
    /// <summary>ロックオン対象の現在位置を取得する関数（対象が失われたら false を返す）</summary>
    using TargetPositionGetter = std::function<bool(Hagine::Vector3 &)>;

    /// <summary>命中時に呼ばれる関数</summary>
    using HitHandler = std::function<void(const Hagine::ColliderBase *)>;

    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>
    /// 弾を生成する（見た目と当たり判定の準備）
    /// </summary>
    /// <param name="objectName">オブジェクト名（一意）</param>
    /// <param name="rgba">表示色</param>
    /// <param name="radius">弾の半径</param>
    /// <param name="tag">コライダーのタグ</param>
    /// <param name="collisionMask">衝突対象タグ</param>
    void InitProjectile(const std::string &objectName, const Hagine::Vector4 &rgba,
                        float radius, const std::string &tag, const std::string &collisionMask);

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

    /// <summary>更新（軌道補正・移動・寿命）</summary>
    void Update() override;

    /// <summary>役目を終えたか（命中・寿命切れ）</summary>
    bool IsFinished() const { return isFinished_; }

    /// ===================================================
    /// setter
    /// ===================================================

    void SetTargetPositionGetter(TargetPositionGetter getter) { targetPositionGetter_ = std::move(getter); }
    void SetHitHandler(HitHandler handler) { hitHandler_ = std::move(handler); }

private:
    /// <summary>命中・寿命切れで動きを止める</summary>
    void Finish();

    /// ===================================================
    /// private variables
    /// ===================================================

    Hagine::Vector3 direction_{0.0f, 0.0f, 1.0f}; // 進行方向（正規化済み）
    float speed_ = 40.0f;                          // 速度（単位/秒）
    float lifeTime_ = 3.0f;                        // 残り寿命（秒）
    float correctionRate_ = 8.0f;                  // 軌道補正の強さ
    bool isFinished_ = false;                      // 役目を終えたか

    Hagine::SphereCollider *collider_ = nullptr;   // 当たり判定（所有は BaseObject 側）
    TargetPositionGetter targetPositionGetter_{};  // ロックオン対象の追従先
    HitHandler hitHandler_{};                      // 命中時の処理
};
