#include "BossSphereLattice.h"
#include <algorithm>

using Hagine::Vector3;

void BossSphereLattice::Configure(int subdivision, float shellRadius, float sphereRadius,
                                  int innerLayers, int outerLayers) {
    icosphere_ = BossIcosphere::Build(subdivision);
    shellRadius_ = (std::max)(0.01f, shellRadius);

    // 未指定なら、基本殻の中で隣同士がちょうど接する大きさにする（＝ハニカムが密に詰まる）
    if (sphereRadius > 0.0f) {
        sphereRadius_ = sphereRadius;
    } else {
        sphereRadius_ = icosphere_.meanEdgeLength * shellRadius_ * 0.5f;
    }

    innerLayers_ = (std::max)(0, innerLayers);
    outerLayers_ = (std::max)(0, outerLayers);
}

Vector3 BossSphereLattice::ToLocal(const ShellCell &cell) const {
    if (cell.vertex < 0 || cell.vertex >= GetVertexCount()) {
        return Vector3{0.0f, 0.0f, 0.0f};
    }
    return icosphere_.vertices[static_cast<size_t>(cell.vertex)].localDirection * GetLayerRadius(cell.layer);
}

float BossSphereLattice::GetLayerRadius(int layer) const {
    // 層の間隔は球の直径。隣の層の球とちょうど接する
    return shellRadius_ + static_cast<float>(layer) * sphereRadius_ * 2.0f;
}

bool BossSphereLattice::IsInBand(const ShellCell &cell) const {
    if (cell.vertex < 0 || cell.vertex >= GetVertexCount()) {
        return false;
    }
    if (cell.layer < -innerLayers_ || cell.layer > outerLayers_) {
        return false;
    }
    // 内側へ潜りすぎて中心を突き抜けないようにする
    return GetLayerRadius(cell.layer) > sphereRadius_;
}

int BossSphereLattice::GetNeighborCount(const ShellCell &cell) const {
    if (cell.vertex < 0 || cell.vertex >= GetVertexCount()) {
        return 0;
    }
    // 同じ層の隣（icosphereの辺：5個または6個）＋ 上下の層（2個）
    return static_cast<int>(icosphere_.vertices[static_cast<size_t>(cell.vertex)].neighbors.size()) + 2;
}

ShellCell BossSphereLattice::GetNeighbor(const ShellCell &cell, int neighborIndex) const {
    if (cell.vertex < 0 || cell.vertex >= GetVertexCount()) {
        return cell;
    }

    const std::vector<int> &sameLayerNeighbors =
        icosphere_.vertices[static_cast<size_t>(cell.vertex)].neighbors;
    const int sameLayerCount = static_cast<int>(sameLayerNeighbors.size());

    if (neighborIndex < 0) {
        return cell;
    }
    if (neighborIndex < sameLayerCount) {
        // 同じ層の隣（ハニカムの辺をたどる）
        return ShellCell{sameLayerNeighbors[static_cast<size_t>(neighborIndex)], cell.layer};
    }
    if (neighborIndex == sameLayerCount) {
        return ShellCell{cell.vertex, cell.layer + 1}; // 1つ外側
    }
    if (neighborIndex == sameLayerCount + 1) {
        return ShellCell{cell.vertex, cell.layer - 1}; // 1つ内側
    }
    return cell;
}

std::vector<ShellCell> BossSphereLattice::CollectBaseShellCells() const {
    std::vector<ShellCell> cells;
    cells.reserve(icosphere_.vertices.size());
    for (const BossIcosphereVertex &vertex : icosphere_.vertices) {
        cells.push_back(ShellCell{vertex.index, 0});
    }
    return cells;
}
