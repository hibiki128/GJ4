#pragma once
#include "Application/Character/ColorStruct.h"
#include "type/Vector3.h"

namespace Hagine {
class ColliderBase;
}

/// <summary>
/// ソフトロックオンの問い合わせ内容
/// </summary>
struct LockOnRequest {
    Hagine::Vector3 origin{};                     // 射撃開始位置
    Hagine::Vector3 aimDirection{0.0f, 0.0f, 1.0f}; // 照準方向（正規化されていなくてよい）
    Color color = Color::RED;                     // 狙う色
    float maxAngleDegrees = 20.0f;                // 照準からの許容角度
    float maxDistance = 60.0f;                    // 有効距離
};

/// <summary>
/// ソフトロックオンの結果
/// </summary>
struct LockOnResult {
    int partIndex = -1;              // ロックオンしたパーツ番号（-1 は対象なし）
    Hagine::Vector3 worldPosition{}; // 対象のワールド座標
    float angleDegrees = 0.0f;       // 照準とのなす角
    float distance = 0.0f;           // 射撃開始位置からの距離

    /// <summary>有効な対象を掴んでいるか</summary>
    bool IsValid() const { return partIndex >= 0; }
};

/// <summary>
/// 命中1回分の結果（連鎖が成立したか、どれだけ削れたか）
/// </summary>
struct ChainHitResult {
    bool accepted = false;    // 命中として受理されたか（生存・色一致）
    bool destroyed = false;   // 連鎖が成立してパーツが壊れたか
    int chainSize = 0;        // 同色で繋がっていた数
    float damage = 0.0f;      // 与えたダメージ
    float staggerTime = 0.0f; // 発生した怯み時間
};

/// <summary>
/// 「撃つ側」から見たボスの窓口。ボスが実装する。
/// プレイヤーの射撃処理はこのインターフェース越しにだけボスへ触る。
/// </summary>
class IBossTargetQuery {
public:
    virtual ~IBossTargetQuery() = default;

    /// <summary>
    /// ソフトロックオンの対象を探す（生存・色一致・こちらを向いている面のみ対象）。
    /// エンジンのワールド座標取得が非constのため、この関数も非constで宣言している
    /// </summary>
    /// <param name="request">問い合わせ内容</param>
    /// <param name="out">見つかった対象</param>
    /// <returns>bool: 見つかれば true</returns>
    virtual bool FindLockOnTarget(const LockOnRequest &request, LockOnResult &out) = 0;

    /// <summary>
    /// パーツ番号で命中を通知する（連鎖判定まで行う）
    /// </summary>
    /// <param name="partIndex">命中したパーツ番号</param>
    /// <param name="shotColor">撃った弾の色</param>
    /// <returns>ChainHitResult: 命中結果</returns>
    virtual ChainHitResult ReportHit(int partIndex, Color shotColor) = 0;

    /// <summary>
    /// コライダーで命中を通知する（弾の OnCollisionEnter から直接呼べる）
    /// </summary>
    /// <param name="hitCollider">衝突相手のコライダー</param>
    /// <param name="shotColor">撃った弾の色</param>
    /// <returns>ChainHitResult: 命中結果</returns>
    virtual ChainHitResult ReportHitByCollider(const Hagine::ColliderBase *hitCollider, Color shotColor) = 0;

    /// <summary>
    /// コライダーからパーツ番号を引く
    /// </summary>
    /// <param name="collider">対象のコライダー</param>
    /// <returns>int: パーツ番号（見つからなければ -1）</returns>
    virtual int FindPartIndex(const Hagine::ColliderBase *collider) const = 0;
};
