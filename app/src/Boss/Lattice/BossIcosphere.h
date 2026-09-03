#pragma once
#include "type/Vector3.h"
#include <vector>

/// <summary>
/// 基本殻の頂点1つ分の配置情報（純粋な幾何データ。描画やゲーム状態は持たない）
/// </summary>
struct BossIcosphereVertex {
    int index = -1;                     // 頂点番号
    Hagine::Vector3 localDirection{};   // 単位球上の方向（位置 = localDirection * 半径）
    std::vector<int> neighbors{};       // 隣接する頂点番号（icosphere の辺。5個または6個）
};

/// <summary>
/// レイアウト生成の結果
/// </summary>
struct BossIcosphereResult {
    std::vector<BossIcosphereVertex> vertices{};
    float meanEdgeLength = 0.0f; // 単位球上での平均辺長（パーツの見た目サイズの自動算出に使う）
};

/// <summary>
/// ボスの基本殻（ハニカム状の球面配置）を生成する。
/// 正二十面体を subdivision 回分割し、その「頂点」をパーツ、「辺」を隣接関係とみなす。
/// 分割数と個数の対応: 0 → 12個(5近傍) / 1 → 42個(5〜6近傍) / 2 → 162個
/// </summary>
namespace BossIcosphere {

/// <summary>
/// icosphere のパーツ配置を生成する
/// </summary>
/// <param name="subdivision">分割回数（0〜3にクランプされる）</param>
/// <returns>BossIcosphereResult: パーツ配置と平均辺長</returns>
BossIcosphereResult Build(int subdivision);

} // namespace BossIcosphere
