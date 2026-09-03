#pragma once
#include "src/Boss/Data/BossColorPalette.h"
#include "src/Boss/Data/BossParameters.h"
#include "src/Boss/Lattice/BossSphereLattice.h"
#include "src/Boss/Shell/BossSphere.h"
#include "src/Interface/IBossTargetQuery.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Hagine {
class ViewProjection;
}

/// <summary>
/// ボスの殻を構成する球の集合。
///
/// ・置ける場所は FCC 格子（BossSphereLattice）が定義し、埋まっているセルだけを疎に保持する
/// ・座標はすべてボスのローカル空間なので、ボスが回っても再計算は発生しない
/// ・着弾はレイと占有球の交差で判定する（穴が開くため、1個の大きな球では代用できない。
///   また弾は1フレームで球の直径以上進むため、重なり判定だとすり抜ける）
/// ・当たった球の隣接12セルのうち空いている最寄りへ弾を付着させ、
///   そこから同色を幅優先で辿って規定数以上なら消去する
/// </summary>
class BossSphereCluster {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>
    /// 殻を生成して親オブジェクトへぶら下げる
    /// </summary>
    /// <param name="parent">親（ボス本体）</param>
    /// <param name="namePrefix">球の名前の接頭辞</param>
    /// <param name="shell">殻のパラメータ</param>
    /// <param name="palette">色パレット</param>
    /// <param name="chain">連鎖パラメータ（初期配色の制限に使う）</param>
    /// <param name="colorSeed">配色シード（0なら実行ごとにランダム）</param>
    void Build(Hagine::BaseObject *parent, const std::string &namePrefix,
               const BossShellParams &shell, const BossColorPalette &palette,
               const BossChainParams &chain, uint32_t colorSeed);

    /// <summary>殻を初期状態へ戻す（配色もやり直す）</summary>
    void ResetAll(const BossShellParams &shell, const BossColorPalette &palette,
                  const BossChainParams &chain, uint32_t colorSeed);

    /// <summary>球の大きさだけを反映し直す（作り直さない）</summary>
    /// <param name="shell">殻のパラメータ</param>
    void ApplyRadius(const BossShellParams &shell);

    /// <summary>全球を描画する（親の Draw から呼ぶ）</summary>
    void Draw(const Hagine::ViewProjection &viewProjection);

    /// <summary>同色で隣接している球を線で結んで可視化する</summary>
    void DebugDraw();

    /// ===================================================
    /// 着弾
    /// ===================================================

    /// <summary>
    /// 線分（弾の前フレーム位置→現在位置）を投げ、最初に当たった球の隣へ弾を付着させる。
    /// 付着した位置から同色を辿り、規定数以上まとまっていれば消去する
    /// </summary>
    /// <param name="worldStart">線分の始点（ワールド）</param>
    /// <param name="worldEnd">線分の終点（ワールド）</param>
    /// <param name="color">弾の色</param>
    /// <param name="chain">連鎖パラメータ</param>
    /// <param name="palette">色パレット（表示色の取得に使う）</param>
    /// <returns>BulletHitResult: 当たったか・付着したか・消えたか</returns>
    BulletHitResult RaycastAttach(const Hagine::Vector3 &worldStart, const Hagine::Vector3 &worldEnd,
                                  Color color, const BossChainParams &chain,
                                  const BossColorPalette &palette);

    /// ===================================================
    /// 問い合わせ
    /// ===================================================

    /// <summary>ソフトロックオン対象を探す</summary>
    bool FindLockOnTarget(const LockOnRequest &request, bool requireFacing, LockOnResult &out);

    /// <summary>セルにある球のワールド座標を取得する（消えていれば false）</summary>
    bool TryGetCellWorldPosition(const ShellCell &cell, Hagine::Vector3 &out);

    /// <summary>ロックオン強調表示を指定セルだけに絞る</summary>
    void SetHighlightedCell(const ShellCell &cell, bool valid);

    /// <summary>削れた割合（露出度 0〜1）。初期の球数を基準にする</summary>
    float GetExposure() const;

    /// <summary>指定色の球の数（色残量ミニマップ用）</summary>
    int CountAlive(Color color) const;

    /// <summary>球1個の半径（自動算出された場合の実値）</summary>
    float GetSphereRadius() const { return sphereRadius_; }

    int GetOccupiedCount() const { return static_cast<int>(occupied_.size()); }
    int GetInitialCount() const { return initialCount_; }
    int GetCapacity() const { return static_cast<int>(pool_.size()); }

    const BossSphereLattice &GetLattice() const { return lattice_; }

private:
    /// <summary>格子セル1つに置かれた球の情報</summary>
    struct SphereSlot {
        Color color = Color::RED;
        BossSphere *sphere = nullptr; // 実体はプールが所有する
    };

    /// ===================================================
    /// private method
    /// ===================================================

    /// <summary>プールを必要数まで用意する</summary>
    void EnsurePool(Hagine::BaseObject *parent, const std::string &namePrefix, int capacity, float radius);

    /// <summary>初期の殻をランダムな色で敷き詰める</summary>
    void FillInitialShell(const BossShellParams &shell, const BossColorPalette &palette,
                          const BossChainParams &chain, uint32_t colorSeed);

    /// <summary>セルへ球を置く（プールから1つ借りる）</summary>
    bool PlaceSphere(const ShellCell &cell, Color color, const BossColorPalette &palette);

    /// <summary>セルの球を取り除く（プールへ返す）</summary>
    void RemoveSphere(const ShellCell &cell);

    /// <summary>ボスの平行移動と回転だけを持つ行列（格子空間への変換に使う。スケールは含めない）</summary>
    Hagine::Matrix4x4 MakeShellMatrix();

    /// <summary>線分と占有球の交差を調べ、最も手前の球を返す</summary>
    bool RaycastLocal(const Hagine::Vector3 &localStart, const Hagine::Vector3 &localEnd,
                      ShellCell &outCell, Hagine::Vector3 &outHitPoint) const;

    /// <summary>当たった球の隣接から、着弾点に最も近い空きセルを選ぶ</summary>
    bool FindSnapCell(const ShellCell &hitCell, const Hagine::Vector3 &localHitPoint,
                      ShellCell &outCell) const;

    /// <summary>起点から同色で繋がっているセルを集める（幅優先）</summary>
    std::vector<ShellCell> CollectSameColorCluster(const ShellCell &start) const;

    /// <summary>配色途中の判定用: 既に置かれた球だけを辿った同色連結数を数える</summary>
    int CountConnectedSameColor(const ShellCell &start, Color color) const;

    /// ===================================================
    /// private variables
    /// ===================================================

    BossSphereLattice lattice_{};                                          // 置ける場所の定義
    std::unordered_map<ShellCell, SphereSlot, ShellCellHash> occupied_{}; // 埋まっているセル
    std::vector<std::unique_ptr<BossSphere>> pool_{};                      // 球の実体（所有）
    std::vector<BossSphere *> freeSpheres_{};                              // 待機中の球

    Hagine::BaseObject *pParent_ = nullptr; // 親（非所有）
    int initialCount_ = 0;                  // 初期状態の球数（露出度の基準）
    float sphereRadius_ = 0.45f;            // 球の半径

    ShellCell highlightedCell_{};  // ロックオン強調中のセル
    bool hasHighlight_ = false;

    // 探索用の作業バッファ（毎回の確保を避けるため使い回す）
    mutable std::vector<ShellCell> searchQueue_{};
};
