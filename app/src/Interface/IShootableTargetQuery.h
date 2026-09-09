#pragma once
#include "src/Character/ColorStruct.h"
#include "src/Interface/IBossTargetQuery.h"
#include "type/Vector3.h"

/// <summary>
/// ボス以外の的に対するソフトロックオンの結果。
/// ボスの LockOnResult とほぼ同じだが、対象の指し方が格子セル（ShellCell）ではなく
/// 通し番号になっている点だけが違う
/// </summary>
struct ShootableLockOnResult {
    bool found = false;              // 対象が見つかったか
    Hagine::Vector3 worldPosition{}; // 対象のワールド座標
    float angleDegrees = 0.0f;       // 照準とのなす角
    float distance = 0.0f;           // 射撃開始位置からの距離
    int targetId = -1;               // 対象の通し番号（強調表示の指定に使う。無効なら -1）

    bool IsValid() const { return found; }
};

/// <summary>
/// 「撃つ側」から見た、ボス以外の的の窓口（回復アイテムの膜など）。
///
/// IBossTargetQuery のうち、殻の格子（ShellCell）や色の連鎖に依らない部分だけを
/// 抜き出した形にしてある。撃つ側はこれとボスの窓口の両方へ同じ問い合わせを投げ、
/// 手前にあるほう・照準に近いほうを選ぶ。
///
/// AimHit / LockOnRequest / LockOnRange はボスの窓口と共通のものを使う。
/// 同じ意味の型を2つ持つと、片方だけ直して食い違うことになるため
/// </summary>
class IShootableTargetQuery {
public:
    virtual ~IShootableTargetQuery() = default;

    /// <summary>
    /// 弾の移動線分を渡して着弾を判定する（当たれば的へその1発ぶんを効かせる）。
    /// 弾は毎フレーム「前フレームの位置→現在位置」を渡すこと（速い弾のすり抜けを防ぐため）
    /// </summary>
    /// <param name="worldStart">線分の始点（前フレームの弾の位置）</param>
    /// <param name="worldEnd">線分の終点（現在の弾の位置）</param>
    /// <param name="color">弾の色</param>
    /// <returns>bool: 当たったら true（弾を消してよい）</returns>
    virtual bool RaycastHit(const Hagine::Vector3 &worldStart, const Hagine::Vector3 &worldEnd,
                            Color color) = 0;

    /// <summary>
    /// 線分が最初に当たる点を返すだけの問い合わせ（副作用は起こさない）。
    /// 照準と発射レティクルが着弾地点を求めるのに使う
    /// </summary>
    /// <param name="worldStart">線分の始点（ワールド）</param>
    /// <param name="worldEnd">線分の終点（ワールド）</param>
    /// <param name="color">撃とうとしている色（色によってすり抜ける相手がいる）</param>
    /// <param name="outHit">最初に当たった点と、その的の中心（ワールド）</param>
    /// <returns>bool: 当たれば true</returns>
    virtual bool RaycastPoint(const Hagine::Vector3 &worldStart, const Hagine::Vector3 &worldEnd,
                              Color color, AimHit &outHit) = 0;

    /// <summary>
    /// ソフトロックオンの対象を探す（色一致のもののみ対象）。
    /// 許容範囲はボスのデータが持っているので、問い合わせ内容は撃つ側が組み立てて渡す
    /// </summary>
    /// <param name="request">問い合わせ内容</param>
    /// <param name="out">見つかった対象</param>
    /// <returns>bool: 見つかれば true</returns>
    virtual bool FindLockOnTarget(const LockOnRequest &request, ShootableLockOnResult &out) = 0;

    /// <summary>
    /// ロックオン中の的を強調表示する（valid=false で解除）
    /// </summary>
    /// <param name="targetId">対象の通し番号</param>
    /// <param name="valid">対象が有効か</param>
    virtual void SetLockOnHighlight(int targetId, bool valid) = 0;
};
