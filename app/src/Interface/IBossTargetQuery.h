#pragma once
#include "src/Boss/Lattice/BossSphereLattice.h"
#include "src/Character/ColorStruct.h"
#include "type/Vector3.h"

/// <summary>
/// ソフトロックオンの問い合わせ内容
/// </summary>
struct LockOnRequest {
    Hagine::Vector3 origin{};                       // 射撃開始位置
    Hagine::Vector3 aimDirection{0.0f, 0.0f, 1.0f}; // 照準方向（正規化されていなくてよい）
    Color color = Color::RED;                       // 狙う色
    float maxAngleDegrees = 20.0f;                  // 照準からの許容角度
    float maxDistance = 60.0f;                      // 有効距離
};

/// <summary>
/// ソフトロックオンの結果
/// </summary>
struct LockOnResult {
    bool found = false;              // 対象が見つかったか
    Hagine::Vector3 worldPosition{}; // 対象のワールド座標
    float angleDegrees = 0.0f;       // 照準とのなす角
    float distance = 0.0f;           // 射撃開始位置からの距離
    ShellCell cell{};             // 対象の格子セル（飛翔中の追尾に使う。消えたら無効になる）

    bool IsValid() const { return found; }
};

/// <summary>
/// 弾1発ぶんの着弾結果
/// </summary>
struct BulletHitResult {
    bool hit = false;           // 殻の球に当たったか（false なら穴を素通りした）
    bool attached = false;      // 殻へ付着できたか（当たっても置ける隣が無ければ false）
    bool destroyed = false;     // 同色が規定数そろって消去が起きたか
    int clusterSize = 0;        // 消えた（または繋がった）球の数
    float staggerTime = 0.0f;   // 発生した怯み時間
    Hagine::Vector3 hitPoint{}; // 着弾位置（演出用）

    /// <summary>弾を消してよいか（当たったなら付着の成否によらず弾は役目を終える）</summary>
    bool ShouldConsumeBullet() const { return hit; }
};

/// <summary>
/// 「撃つ側」から見たボスの窓口。ボスが実装する。
/// プレイヤーの射撃処理はこのインターフェース越しにだけボスへ触る。
/// </summary>
class IBossTargetQuery {
public:
    virtual ~IBossTargetQuery() = default;

    /// <summary>
    /// ソフトロックオンの対象を探す（色一致・こちらを向いている面のみ対象）。
    /// エンジンのワールド座標取得が非constのため、この関数も非constで宣言している
    /// </summary>
    /// <param name="request">問い合わせ内容</param>
    /// <param name="out">見つかった対象</param>
    /// <returns>bool: 見つかれば true</returns>
    virtual bool FindLockOnTarget(const LockOnRequest &request, LockOnResult &out) = 0;

    /// <summary>
    /// 弾の移動線分を渡して着弾を判定する。
    /// 当たった球の隣へ弾を付着させ、同色が規定数そろえばまとめて消去する。
    /// 弾は毎フレーム「前フレームの位置→現在位置」を渡すこと（速い弾のすり抜けを防ぐため）
    /// </summary>
    /// <param name="worldStart">線分の始点（前フレームの弾の位置）</param>
    /// <param name="worldEnd">線分の終点（現在の弾の位置）</param>
    /// <param name="color">弾の色</param>
    /// <returns>BulletHitResult: 当たったか・付着したか・消えたか</returns>
    virtual BulletHitResult RaycastAttach(const Hagine::Vector3 &worldStart,
                                          const Hagine::Vector3 &worldEnd, Color color) = 0;

    /// <summary>
    /// 格子セルにある球の現在のワールド座標を取得する（飛翔中の弾が対象を追尾するのに使う）
    /// </summary>
    /// <param name="cell">対象の格子セル</param>
    /// <param name="out">ワールド座標</param>
    /// <returns>bool: その球がまだ存在すれば true</returns>
    virtual bool TryGetTargetPosition(const ShellCell &cell, Hagine::Vector3 &out) = 0;
};
