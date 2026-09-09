#pragma once
#include "src/Boss/Lattice/BossSphereLattice.h"
#include "src/Character/ColorStruct.h"
#include "type/Vector3.h"
#include "type/Vector4.h"
#include <vector>

/// <summary>
/// ソフトロックオンの許容範囲。
/// 値はボス側のデータ（jsons/Boss/&lt;bossId&gt;.json）が持っているので、
/// 撃つ側は GetLockOnRange() で聞いてから問い合わせを組み立てる
/// </summary>
struct LockOnRange {
    float maxAngleDegrees = 20.0f; // 照準からの許容角度
    float maxDistance = 60.0f;     // 有効距離
};

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
/// 照準の射線が最初に当たった相手。
/// 撃つ側はこれを見て「狙っている一点」と「その球の真ん中」の両方を知る
/// </summary>
struct AimHit {
    /// <summary>線分が最初に当たった点（球の表面）。ここが照準の指している一点になる</summary>
    Hagine::Vector3 point{};

    /// <summary>
    /// 当たった球の中心（ワールド）。エイムアシストはここへ狙いを寄せる。
    /// 中心が取れない相手は表面の点と同じ値が入るので、寄せても何も起きない
    /// </summary>
    Hagine::Vector3 center{};
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
///
/// ロックオンの範囲・色・強調表示までここに集めてあるので、撃つ側は
/// ボスの具象クラス（Boss / BossSpider）を include せずに射撃一式を組み立てられる。
/// 撃つ相手が球体形態から蜘蛛へ変わっても、差し替えるのはこのポインタだけで済む。
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
    /// 線分が最初に当たる点を返すだけの問い合わせ（付着・消去などの副作用は起こさない）。
    /// 照準（画面中心の射線）から着弾地点を求めるのに使う。
    /// RaycastAttach と同じ形状を見るので、「照準では当たる表示なのに弾は素通りする」ズレが出ない。
    /// エンジンのワールド座標取得が非constのため、この関数も非constで宣言している
    /// </summary>
    /// <param name="worldStart">線分の始点（ワールド）</param>
    /// <param name="worldEnd">線分の終点（ワールド）</param>
    /// <param name="color">撃とうとしている色（色によってすり抜ける相手がいるので着弾と同じ色を渡す）</param>
    /// <param name="outHit">最初に当たった点と、その球の中心（ワールド）</param>
    /// <returns>bool: 当たれば true</returns>
    virtual bool RaycastPoint(const Hagine::Vector3 &worldStart, const Hagine::Vector3 &worldEnd,
                              Color color, AimHit &outHit) = 0;

    /// <summary>
    /// 格子セルにある球の現在のワールド座標を取得する（飛翔中の弾が対象を追尾するのに使う）
    /// </summary>
    /// <param name="cell">対象の格子セル</param>
    /// <param name="out">ワールド座標</param>
    /// <returns>bool: その球がまだ存在すれば true</returns>
    virtual bool TryGetTargetPosition(const ShellCell &cell, Hagine::Vector3 &out) = 0;

    /// <summary>
    /// ソフトロックオンの許容範囲を取得する（撃つ側が LockOnRequest を組み立てるのに使う）
    /// </summary>
    /// <returns>LockOnRange: 許容角度と有効距離</returns>
    virtual LockOnRange GetLockOnRange() const = 0;

    /// <summary>
    /// ロックオン中の球を強調表示する（valid=false で解除）。
    /// 強調表示を持たない形態は何もしない
    /// </summary>
    /// <param name="cell">対象の格子セル</param>
    /// <param name="valid">対象が有効か</param>
    virtual void SetLockOnHighlight(const ShellCell &cell, bool valid) = 0;

    /// <summary>
    /// 色の表示RGBAを取得する（撃つ側が弾の色をボスの見た目に合わせるのに使う）
    /// </summary>
    /// <param name="color">色</param>
    /// <returns>Vector4: 表示色</returns>
    virtual Hagine::Vector4 GetColorRgba(Color color) const = 0;

    /// <summary>
    /// この相手が使っている色（撃つ側が選べる色を絞るのに使う）
    /// </summary>
    /// <returns>使用色のサブセット</returns>
    virtual const std::vector<Color> &GetUsedColors() const = 0;
};
