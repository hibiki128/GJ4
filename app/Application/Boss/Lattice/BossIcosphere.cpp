#include "BossIcosphere.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>

using Hagine::Vector3;

namespace {

/// <summary>頂点2つの組から中点キャッシュ用のキーを作る（順序非依存）</summary>
uint64_t MakeEdgeKey(int a, int b) {
    const uint64_t lo = static_cast<uint64_t>((std::min)(a, b));
    const uint64_t hi = static_cast<uint64_t>((std::max)(a, b));
    return (lo << 32) | hi;
}

/// <summary>辺の中点を単位球へ射影して追加する（既にあれば使い回す）</summary>
int GetMidPoint(int a, int b, std::vector<Vector3> &vertices, std::map<uint64_t, int> &cache) {
    const uint64_t key = MakeEdgeKey(a, b);
    auto it = cache.find(key);
    if (it != cache.end()) {
        return it->second;
    }

    const Vector3 mid = ((vertices[a] + vertices[b]) * 0.5f).Normalize();
    const int index = static_cast<int>(vertices.size());
    vertices.push_back(mid);
    cache.emplace(key, index);
    return index;
}

} // namespace

BossIcosphereResult BossIcosphere::Build(int subdivision) {
    subdivision = std::clamp(subdivision, 0, 3);

    // --- 正二十面体の12頂点（黄金比 t を使った定番の構成）---
    const float t = (1.0f + std::sqrt(5.0f)) * 0.5f;
    std::vector<Vector3> vertices = {
        Vector3{-1.0f, t, 0.0f}, Vector3{1.0f, t, 0.0f}, Vector3{-1.0f, -t, 0.0f}, Vector3{1.0f, -t, 0.0f},
        Vector3{0.0f, -1.0f, t}, Vector3{0.0f, 1.0f, t}, Vector3{0.0f, -1.0f, -t}, Vector3{0.0f, 1.0f, -t},
        Vector3{t, 0.0f, -1.0f}, Vector3{t, 0.0f, 1.0f}, Vector3{-t, 0.0f, -1.0f}, Vector3{-t, 0.0f, 1.0f}};
    for (Vector3 &vertex : vertices) {
        vertex = vertex.Normalize();
    }

    // --- 正二十面体の20面 ---
    std::vector<std::array<int, 3>> faces = {
        {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
        {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
        {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
        {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}};

    // --- 分割（各辺の中点を取り、1面を4面へ）---
    for (int step = 0; step < subdivision; ++step) {
        std::vector<std::array<int, 3>> nextFaces;
        nextFaces.reserve(faces.size() * 4);
        std::map<uint64_t, int> midPointCache;

        for (const std::array<int, 3> &face : faces) {
            const int a = GetMidPoint(face[0], face[1], vertices, midPointCache);
            const int b = GetMidPoint(face[1], face[2], vertices, midPointCache);
            const int c = GetMidPoint(face[2], face[0], vertices, midPointCache);

            nextFaces.push_back({face[0], a, c});
            nextFaces.push_back({face[1], b, a});
            nextFaces.push_back({face[2], c, b});
            nextFaces.push_back({a, b, c});
        }
        faces.swap(nextFaces);
    }

    // --- 面の辺から隣接関係を作る（重複は set で潰す）---
    const size_t vertexCount = vertices.size();
    std::vector<std::set<int>> neighborSets(vertexCount);
    for (const std::array<int, 3> &face : faces) {
        for (int i = 0; i < 3; ++i) {
            const int from = face[i];
            const int to = face[(i + 1) % 3];
            neighborSets[from].insert(to);
            neighborSets[to].insert(from);
        }
    }

    // --- 結果へ詰め替え、ついでに平均辺長を測る ---
    BossIcosphereResult result;
    result.vertices.resize(vertexCount);

    double edgeLengthSum = 0.0;
    int edgeCount = 0;

    for (size_t i = 0; i < vertexCount; ++i) {
        BossIcosphereVertex &part = result.vertices[i];
        part.index = static_cast<int>(i);
        part.localDirection = vertices[i];
        part.neighbors.assign(neighborSets[i].begin(), neighborSets[i].end());

        for (int neighbor : part.neighbors) {
            // 同じ辺を2回数えないよう、番号が小さい側からのみ加算する
            if (neighbor > static_cast<int>(i)) {
                edgeLengthSum += (vertices[neighbor] - vertices[i]).Length();
                ++edgeCount;
            }
        }
    }

    result.meanEdgeLength = (edgeCount > 0) ? static_cast<float>(edgeLengthSum / edgeCount) : 0.0f;
    return result;
}
