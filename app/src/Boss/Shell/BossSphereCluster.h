#pragma once
#include "src/Boss/Data/BossColorPalette.h"
#include "src/Boss/Data/BossParameters.h"
#include "src/Boss/Lattice/BossSphereLattice.h"
#include "src/Boss/Shell/BossShellMetaBall.h"
#include "src/Boss/Shell/BossSphere.h"
#include "src/Interface/IBossTargetQuery.h"
#include <array>
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
/// ・見た目は球1個ずつではなく、同色をまとめて融合させたメタボール（BossShellMetaBall）で描く
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

    /// <summary>
    /// 吸着・消滅の演出を進める（毎フレーム呼ぶ）。
    /// 消え切った球はここでプールへ返る
    /// </summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    void UpdateMotions(float deltaTime);

    /// <summary>消滅演出の途中にある球の数（演出の確認用）</summary>
    int GetVanishingCount() const { return static_cast<int>(vanishing_.size()); }

    /// ===================================================
    /// 登場演出
    /// ===================================================

    /// <summary>
    /// 登場演出を開始する。各球を周囲へ散らし、飛来元と動き出しの遅れを決める
    /// </summary>
    /// <param name="appear">演出パラメータ</param>
    /// <param name="seed">散らばり方のシード（0なら実行ごとにランダム）</param>
    void BeginAppear(const BossAppearParams &appear, uint32_t seed);

    /// <summary>
    /// 登場演出を進める（集束 → 到着 → 膨張）
    /// </summary>
    /// <param name="appear">演出パラメータ</param>
    /// <param name="elapsed">開始からの経過時間（秒）</param>
    void UpdateAppear(const BossAppearParams &appear, float elapsed);

    /// <summary>登場演出を終了し、全球を最終状態（定位置・本来の大きさ）にする</summary>
    void FinishAppear();

    /// <summary>登場演出にかかる合計時間（秒）</summary>
    static float GetAppearDuration(const BossAppearParams &appear) {
        return appear.gatherTime + appear.settleTime + appear.expandTime;
    }

    /// <summary>全球を描画する（親の Draw から呼ぶ）</summary>
    /// メタボールのパラメータを設定する（Build より前に呼ぶこと）。
    /// 変えると次の Update で殻のメッシュが作り直される
    /// </summary>
    /// <param name="params">メタボールのパラメータ</param>
    void SetMetaBallParams(const BossMetaBallParams &params);

    /// <summary>
    /// 殻の見た目を更新する（親の Update から呼ぶ）。
    /// 球が増減した色だけメッシュを作り直すので、動いているだけなら何もしない
    /// </summary>
    void Update();

    /// <summary>
    /// GPU生成のときに、殻のメッシュを作り直すコンピュートを積む。
    /// シャドウより前に走る DrawSystem のコンピュートフェーズから呼ぶこと
    /// </summary>
    /// <param name="deltaTime">経過時間（秒）。脈動の時間を進めるのに使う</param>
    void DispatchCompute(float deltaTime);

    /// <summary>殻を描画する（親の Draw から呼ぶ）</summary>
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

    /// <summary>吸着・消滅の演出設定を反映する（実行時調整を毎フレーム渡してよい）</summary>
    void SetEffectParams(const BossEffectParams &effect) { effect_ = effect; }

    /// <summary>球1個の半径（自動算出された場合の実値）</summary>
    float GetSphereRadius() const { return sphereRadius_; }

    int GetOccupiedCount() const { return static_cast<int>(occupied_.size()); }
    int GetInitialCount() const { return initialCount_; }
    int GetCapacity() const { return static_cast<int>(pool_.size()); }

    const BossSphereLattice &GetLattice() const { return lattice_; }

    /// <summary>殻の見た目（メタボール）。生成結果の統計を見るのに使う</summary>
    const BossShellMetaBall &GetMetaBall() const { return metaBall_; }

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
    /// <param name="attachFrom">吸着演出の開始位置（nullptr なら演出なしで即配置）</param>
    bool PlaceSphere(const ShellCell &cell, Color color, const BossColorPalette &palette,
                     const Hagine::Vector3 *attachFrom = nullptr);

    /// <summary>セルの球を取り除く（消滅演出へ回し、消え切ったらプールへ返す）</summary>
    /// <param name="vanishDelay">消え始めるまでの遅れ（秒）</param>
    void RemoveSphere(const ShellCell &cell, float vanishDelay = 0.0f);

    /// <summary>消滅演出中の球をすべて即座にプールへ返す</summary>
    void FlushVanishing();

    /// <summary>その色の融合メッシュを作り直させる</summary>
    /// <param name="color">色</param>
    void MarkColorDirty(Color color);

    /// <summary>全色の融合メッシュを作り直させる</summary>
    void MarkAllColorsDirty();

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
    std::vector<BossSphere *> vanishing_{};                                // 消滅演出中の球（占有マップからは外れている）
    BossEffectParams effect_{};                                            // 吸着・消滅の演出設定

    BossShellMetaBall metaBall_{};                    // 同色を融合させた殻の見た目
    BossMetaBallParams metaBallParams_{};             // メタボールのパラメータ
    std::array<bool, kGameColorCount> colorDirty_{};  // 球が増減して作り直しが要る色

    Hagine::BaseObject *pParent_ = nullptr; // 親（非所有）
    int initialCount_ = 0;                  // 初期状態の球数（露出度の基準）
    float sphereRadius_ = 0.45f;            // 球の半径

    ShellCell highlightedCell_{};  // ロックオン強調中のセル
    bool hasHighlight_ = false;

    // 探索用の作業バッファ（毎回の確保を避けるため使い回す）
    mutable std::vector<ShellCell> searchQueue_{};
};
