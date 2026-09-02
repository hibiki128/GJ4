#pragma once
#include "Application/Boss/Data/BossColorPalette.h"
#include "Application/Boss/Data/BossParameters.h"
#include "Application/Boss/Data/BossPartLayout.h"
#include "Application/Boss/Part/BossPart.h"
#include "Application/Interface/IBossTargetQuery.h"
#include <memory>
#include <string>
#include <vector>

namespace Hagine {
class ColliderBase;
class ViewProjection;
} // namespace Hagine

/// <summary>
/// ボスのパーツ群と隣接グラフを保持し、連鎖マッチ判定を行う。
///
/// ・隣接関係は icosphere の辺（構築後は不変）
/// ・命中したパーツから同色隣接をBFSで辿り、minMatch以上なら一斉破壊
/// ・探索の訪問済み判定は世代スタンプ方式（毎回の配列クリアが不要）
/// </summary>
class BossPartGraph {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>
    /// パーツ群を生成して親オブジェクトへぶら下げる
    /// </summary>
    /// <param name="parent">親（ボス本体）</param>
    /// <param name="namePrefix">パーツ名の接頭辞</param>
    /// <param name="params">配置パラメータ</param>
    /// <param name="palette">色パレット</param>
    /// <param name="chain">連鎖パラメータ（初期配色が成立しないようにするため参照する）</param>
    /// <param name="colorSeed">配色シード（0なら実行ごとにランダム）</param>
    /// <param name="partTag">パーツのコライダータグ</param>
    /// <param name="bulletTag">弾のコライダータグ（衝突マスクに使う）</param>
    void Build(Hagine::BaseObject *parent, const std::string &namePrefix,
               const BossLayoutParams &params, const BossColorPalette &palette,
               const BossChainParams &chain, uint32_t colorSeed,
               const std::string &partTag, const std::string &bulletTag);

    /// <summary>
    /// 半径・パーツの大きさ・厚みだけを反映し直す（パーツは作り直さない）。
    /// 分割数を変えた場合は Build を呼ぶこと
    /// </summary>
    /// <param name="params">配置パラメータ</param>
    void ApplyLayout(const BossLayoutParams &params);

    /// <summary>全パーツを描画する（親の Draw から呼ぶ）</summary>
    /// <param name="viewProjection">ビュープロジェクション</param>
    void Draw(const Hagine::ViewProjection &viewProjection);

    /// <summary>隣接グラフをデバッグ描画する（生存パーツ間の辺）</summary>
    void DebugDrawGraph();

    /// ===================================================
    /// 連鎖マッチ
    /// ===================================================

    /// <summary>
    /// 起点パーツから同色で繋がっている生存パーツを集める（BFS）
    /// </summary>
    /// <param name="startIndex">起点のパーツ番号</param>
    /// <returns>std::vector&lt;int&gt;: 同色で連結したパーツ番号（起点を含む）</returns>
    std::vector<int> CollectSameColorCluster(int startIndex) const;

    /// <summary>指定パーツをまとめて破壊する</summary>
    /// <param name="indices">破壊するパーツ番号</param>
    /// <returns>int: 実際に破壊した数</returns>
    int BreakParts(const std::vector<int> &indices);

    /// <summary>全パーツを復活させ、配色をやり直す（リトライ・デバッグ用）</summary>
    /// <param name="palette">色パレット</param>
    /// <param name="chain">連鎖パラメータ</param>
    /// <param name="colorSeed">配色シード</param>
    void ResetAll(const BossColorPalette &palette, const BossChainParams &chain, uint32_t colorSeed);

    /// ===================================================
    /// 問い合わせ
    /// ===================================================

    /// <summary>ソフトロックオン対象を探す</summary>
    /// <param name="request">問い合わせ内容</param>
    /// <param name="requireFacing">射手側を向いている面だけを対象にするか</param>
    /// <param name="out">見つかった対象</param>
    /// <returns>bool: 見つかれば true</returns>
    bool FindLockOnTarget(const LockOnRequest &request, bool requireFacing, LockOnResult &out);

    /// <summary>コライダーからパーツ番号を引く</summary>
    /// <param name="collider">対象のコライダー</param>
    /// <returns>int: パーツ番号（見つからなければ -1）</returns>
    int FindPartIndex(const Hagine::ColliderBase *collider) const;

    /// <summary>削れたパーツの割合（露出度 0〜1）</summary>
    float GetExposure() const;

    /// <summary>指定色の生存パーツ数（色残量ミニマップ用）</summary>
    /// <param name="color">色</param>
    /// <returns>int: 生存数</returns>
    int CountAlive(Color color) const;

    /// <summary>
    /// 破壊可能な塊（minMatch以上の同色連結）の数を数える。
    /// パーツの色は変化しないため、この数が0になると連鎖では削れなくなる。
    /// 盤面が枯れていないかの確認に使う
    /// </summary>
    /// <param name="minMatch">破壊に必要な連結数</param>
    /// <param name="outPartCount">該当する塊に含まれるパーツ数の合計（省略可）</param>
    /// <returns>int: 破壊可能な塊の数</returns>
    int CountPoppableClusters(int minMatch, int *outPartCount = nullptr) const;

    int GetAliveCount() const { return aliveCount_; }
    int GetTotalCount() const { return static_cast<int>(parts_.size()); }

    /// <summary>
    /// 生成時点で「連鎖によって壊せる」パーツの総数。
    /// パーツの色は変化せず、破壊は他の塊に影響しないため、この数は生成時に決まる。
    /// 露出度を「実際に到達できる上限」で正規化するために使う
    /// </summary>
    int GetDestroyablePartCount() const { return destroyablePartCount_; }

    /// <summary>パーツを取得する（範囲外は nullptr）</summary>
    BossPart *GetPart(int index);
    const BossPart *GetPart(int index) const;

    /// <summary>ロックオン強調表示を指定パーツだけに絞る（-1で全解除）</summary>
    /// <param name="index">強調するパーツ番号</param>
    void SetHighlightedPart(int index);

private:
    /// ===================================================
    /// private method
    /// ===================================================

    /// <summary>
    /// 全パーツへ色を配る。
    /// パーツの色は後から変わらないため、開幕時点で存在する同色の塊がそのまま
    /// 「壊せる場所」になる。したがって塊を消すのではなく、
    /// 「一撃で削れすぎる巨大な塊（maxInitialCluster超）」だけを避ける
    /// </summary>
    void AssignColors(const BossColorPalette &palette, const BossChainParams &chain, uint32_t colorSeed);

    /// <summary>
    /// 破壊可能な塊（minMatch以上）が1つも無い盤面にならないよう保証する
    /// </summary>
    void EnsurePoppableCluster(const BossColorPalette &palette, const BossChainParams &chain);

    /// <summary>配色途中の判定用: 既に色が決まっているパーツだけを辿った連結数を数える</summary>
    int CountAssignedCluster(int startIndex, const std::vector<int> &assignedColors) const;

    /// <summary>パーツの大きさを決める（未指定なら平均辺長から自動算出）</summary>
    float ResolvePartScale(const BossLayoutParams &params) const;

    /// ===================================================
    /// private variables
    /// ===================================================

    std::vector<std::unique_ptr<BossPart>> parts_{};   // パーツ本体（所有）
    std::vector<std::vector<int>> adjacency_{};        // 隣接（構築後は不変）
    int aliveCount_ = 0;                               // 生存パーツ数
    int destroyablePartCount_ = 0;                     // 生成時点で壊せるパーツの総数
    float meanEdgeLength_ = 0.0f;                      // 単位球上の平均辺長（大きさの自動算出に使う）
    int highlightedIndex_ = -1;                        // ロックオン強調中のパーツ

    // BFS の訪問済み管理（世代スタンプ方式）
    mutable std::vector<uint32_t> visitStamps_{};
    mutable uint32_t currentStamp_ = 0;

    // 作業用バッファ（毎フレームの確保を避けるため使い回す）
    mutable std::vector<int> searchQueue_{};
};
