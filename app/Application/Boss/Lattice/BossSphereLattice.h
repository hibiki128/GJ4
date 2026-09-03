#pragma once
#include "Application/Boss/Lattice/BossIcosphere.h"
#include "type/Vector3.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

/// <summary>
/// 殻のセル座標。「ハニカム状に並んだ基本殻の頂点」と「そこから何層目か」の組で表す。
/// layer 0 が初期状態の殻で、+1 が外側（弾が付着して盛り上がる方向）、
/// -1 が内側（穴を通った弾が内壁に付く方向）。
/// </summary>
struct ShellCell {
    int32_t vertex = 0; // 基本殻の頂点番号
    int32_t layer = 0;  // 層（0＝基本殻）

    bool operator==(const ShellCell &rhs) const { return vertex == rhs.vertex && layer == rhs.layer; }
    bool operator!=(const ShellCell &rhs) const { return !(*this == rhs); }
};

/// <summary>ShellCell をハッシュコンテナのキーにするためのハッシュ関数</summary>
struct ShellCellHash {
    std::size_t operator()(const ShellCell &cell) const {
        // 頂点番号は高々数百なので、層と合わせて64bitへ素直に詰める
        const uint64_t packed = (static_cast<uint64_t>(static_cast<uint32_t>(cell.vertex)) << 32) |
                                static_cast<uint32_t>(cell.layer);
        return std::hash<uint64_t>{}(packed);
    }
};

/// <summary>
/// 球を置ける場所を定義する格子。
///
/// 基本殻は icosphere の頂点をそのまま使う（球面上に均等＝ハニカム状に並ぶ）。
/// そこから半径方向へ球の直径ずつ積み重ねたものを「層」として扱う。
///
/// 隣接は
///   ・同じ層の中では icosphere の辺（5〜6方向）
///   ・層をまたぐのは同じ頂点の上下（2方向）
/// と定義でき、層ごとに頂点数が変わらないため層間の対応を手で書く必要がない。
///
/// FCC格子でも同じことはできるが、球面で薄く切ると殻が断片化して
/// ハニカムの見た目が崩れるため、この方式を採っている。
/// </summary>
class BossSphereLattice {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>
    /// 格子を設定する
    /// </summary>
    /// <param name="subdivision">基本殻の分割回数（0→12 / 1→42 / 2→162 頂点）</param>
    /// <param name="shellRadius">基本殻の半径</param>
    /// <param name="sphereRadius">球1個の半径（0以下なら隣同士が接する大きさを自動算出）</param>
    /// <param name="innerLayers">内側へ何層まで付着を許すか（0で内側なし）</param>
    /// <param name="outerLayers">外側へ何層まで付着を許すか</param>
    void Configure(int subdivision, float shellRadius, float sphereRadius,
                   int innerLayers, int outerLayers);

    /// <summary>セル中心のローカル座標</summary>
    Hagine::Vector3 ToLocal(const ShellCell &cell) const;

    /// <summary>セルが有効な範囲にあるか</summary>
    bool IsInBand(const ShellCell &cell) const;

    /// <summary>そのセルが持つ隣接セルの数（同層の5〜6個＋上下の層）</summary>
    int GetNeighborCount(const ShellCell &cell) const;

    /// <summary>指定番目の隣接セルを返す</summary>
    /// <param name="cell">基準セル</param>
    /// <param name="neighborIndex">0 〜 GetNeighborCount()-1</param>
    ShellCell GetNeighbor(const ShellCell &cell, int neighborIndex) const;

    /// <summary>基本殻（layer 0）のセルを全て返す</summary>
    std::vector<ShellCell> CollectBaseShellCells() const;

    /// ===================================================
    /// getter
    /// ===================================================

    float GetSphereRadius() const { return sphereRadius_; }
    float GetShellRadius() const { return shellRadius_; }
    int GetVertexCount() const { return static_cast<int>(icosphere_.vertices.size()); }
    int GetInnerLayers() const { return innerLayers_; }
    int GetOuterLayers() const { return outerLayers_; }

    /// <summary>層の半径（中心からセル中心までの距離）</summary>
    float GetLayerRadius(int layer) const;

    /// <summary>一番外側の層のセル中心までの距離</summary>
    float GetOuterRadius() const { return GetLayerRadius(outerLayers_); }

private:
    /// ===================================================
    /// private variables
    /// ===================================================

    BossIcosphereResult icosphere_{}; // 基本殻の頂点と隣接（ハニカム）
    float shellRadius_ = 3.0f;        // 基本殻の半径
    float sphereRadius_ = 0.45f;      // 球1個の半径
    int innerLayers_ = 1;             // 内側の層数
    int outerLayers_ = 2;             // 外側の層数
};
