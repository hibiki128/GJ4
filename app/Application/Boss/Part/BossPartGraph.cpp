#include "BossPartGraph.h"
#include "camera/projection/ViewProjection.h"
#include "line/LineRenderer.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>

using namespace Hagine;

namespace {
/// <summary>当たり判定の半径をパーツの見た目に対してどれだけ広げるか</summary>
constexpr float kPartColliderScale = 0.6f;

/// <summary>パーツの大きさを平均辺長から決めるときの詰め具合（隣と少し重なる値）</summary>
constexpr float kAutoPartScaleRatio = 0.60f;
} // namespace

float BossPartGraph::ResolvePartScale(const BossLayoutParams &params) const {
    if (params.partScale > 0.0f) {
        return params.partScale;
    }
    // 隣り合うパーツの間隔（平均辺長×半径）から、隙間が空かない大きさを求める
    return meanEdgeLength_ * params.radius * kAutoPartScaleRatio;
}

void BossPartGraph::ApplyLayout(const BossLayoutParams &params) {
    const float partScale = ResolvePartScale(params);
    for (std::unique_ptr<BossPart> &part : parts_) {
        part->ApplyLayout(params.radius, partScale, params.partThickness, kPartColliderScale);
    }
}

void BossPartGraph::Build(BaseObject *parent, const std::string &namePrefix,
                          const BossLayoutParams &params, const BossColorPalette &palette,
                          const BossChainParams &chain, uint32_t colorSeed,
                          const std::string &partTag, const std::string &bulletTag) {
    // 作り直しの場合、先に親から切り離す。
    // BaseObject のデストラクタは親の children_ から自分を外さないため、
    // これを省くと親側に解放済みポインタが残る
    for (std::unique_ptr<BossPart> &part : parts_) {
        part->DetachParent();
    }
    parts_.clear();
    adjacency_.clear();

    const BossPartLayoutResult layout = BossPartLayout::BuildIcosphere(params.subdivision);
    meanEdgeLength_ = layout.meanEdgeLength;

    // パーツの大きさ。未指定(0以下)なら隣とほどよく接するサイズを平均辺長から決める
    const float partScale = ResolvePartScale(params);

    parts_.reserve(layout.parts.size());
    adjacency_.reserve(layout.parts.size());

    for (const BossPartDesc &desc : layout.parts) {
        auto part = std::make_unique<BossPart>();
        part->InitPart(namePrefix + "_" + std::to_string(desc.index), desc,
                       params.radius, partScale, params.partThickness);
        part->SetupCollider(partTag, bulletTag, kPartColliderScale);
        if (parent) {
            part->SetParent(parent);
            // 親（コア）のスケールはパーツへ伝播させない。位置と向きだけ追従させる
            part->GetWorldTransform()->inheritScale_ = false;
        }
        parts_.push_back(std::move(part));
        adjacency_.push_back(desc.neighbors);
    }

    aliveCount_ = static_cast<int>(parts_.size());
    visitStamps_.assign(parts_.size(), 0);
    currentStamp_ = 0;
    highlightedIndex_ = -1;

    AssignColors(palette, chain, colorSeed);
}

void BossPartGraph::Draw(const ViewProjection &viewProjection) {
    for (std::unique_ptr<BossPart> &part : parts_) {
        if (part->IsPartAlive()) {
            part->Draw(viewProjection);
        }
    }
}

void BossPartGraph::DebugDrawGraph() {
    LineRenderer *lineRenderer = LineRenderer::GetInstance();
    for (size_t i = 0; i < parts_.size(); ++i) {
        if (!parts_[i]->IsPartAlive()) {
            continue;
        }
        const Vector3 from = parts_[i]->GetWorldPosition();
        for (int neighbor : adjacency_[i]) {
            // 同じ辺を2回描かない
            if (neighbor <= static_cast<int>(i) || !parts_[neighbor]->IsPartAlive()) {
                continue;
            }
            const bool sameColor = parts_[i]->GetPartColor() == parts_[neighbor]->GetPartColor();
            const Vector4 color = sameColor ? Vector4{1.0f, 1.0f, 1.0f, 1.0f}
                                            : Vector4{0.25f, 0.25f, 0.30f, 1.0f};
            lineRenderer->AddLine(from, parts_[neighbor]->GetWorldPosition(), color);
        }
    }
}

std::vector<int> BossPartGraph::CollectSameColorCluster(int startIndex) const {
    std::vector<int> cluster;
    const BossPart *start = GetPart(startIndex);
    if (!start || !start->IsPartAlive()) {
        return cluster;
    }

    const Color targetColor = start->GetPartColor();

    // 世代スタンプを進めることで、訪問済み配列をクリアせずに再利用する
    ++currentStamp_;
    if (currentStamp_ == 0) {
        // 一周したときだけ全体をリセットする
        std::fill(visitStamps_.begin(), visitStamps_.end(), 0);
        currentStamp_ = 1;
    }

    searchQueue_.clear();
    searchQueue_.push_back(startIndex);
    visitStamps_[startIndex] = currentStamp_;

    for (size_t head = 0; head < searchQueue_.size(); ++head) {
        const int current = searchQueue_[head];
        cluster.push_back(current);

        for (int neighbor : adjacency_[current]) {
            if (visitStamps_[neighbor] == currentStamp_) {
                continue;
            }
            const BossPart *part = parts_[neighbor].get();
            if (!part->IsPartAlive() || part->GetPartColor() != targetColor) {
                continue;
            }
            visitStamps_[neighbor] = currentStamp_;
            searchQueue_.push_back(neighbor);
        }
    }

    return cluster;
}

int BossPartGraph::BreakParts(const std::vector<int> &indices) {
    int broken = 0;
    for (int index : indices) {
        BossPart *part = GetPart(index);
        if (!part || !part->IsPartAlive()) {
            continue;
        }
        part->Break();
        ++broken;
        if (highlightedIndex_ == index) {
            highlightedIndex_ = -1;
        }
    }
    aliveCount_ -= broken;
    return broken;
}

void BossPartGraph::ResetAll(const BossColorPalette &palette, const BossChainParams &chain, uint32_t colorSeed) {
    for (std::unique_ptr<BossPart> &part : parts_) {
        part->Restore();
    }
    aliveCount_ = static_cast<int>(parts_.size());
    highlightedIndex_ = -1;
    AssignColors(palette, chain, colorSeed);
}

bool BossPartGraph::FindLockOnTarget(const LockOnRequest &request, bool requireFacing, LockOnResult &out) {
    out = LockOnResult{};

    if (request.aimDirection.LengthSq() <= 0.0f) {
        return false;
    }
    const Vector3 aimDirection = request.aimDirection.Normalize();
    const float maxAngleCos = std::cos(request.maxAngleDegrees * std::numbers::pi_v<float> / 180.0f);

    float bestCos = -2.0f;
    for (std::unique_ptr<BossPart> &part : parts_) {
        if (!part->IsPartAlive() || part->GetPartColor() != request.color) {
            continue;
        }

        const Vector3 partPosition = part->GetWorldPosition();
        const Vector3 toPart = partPosition - request.origin;
        const float distance = toPart.Length();
        if (distance <= 0.0001f || distance > request.maxDistance) {
            continue;
        }

        const Vector3 toPartDirection = toPart / distance;
        const float angleCos = aimDirection.Dot(toPartDirection);
        if (angleCos < maxAngleCos) {
            continue; // 照準の許容角度から外れている
        }

        if (requireFacing) {
            // 面がこちらを向いていない（球の裏側の）パーツは掴まない
            if (part->GetWorldNormal().Dot(toPartDirection) >= 0.0f) {
                continue;
            }
        }

        // 照準に最も近いものを選ぶ
        if (angleCos > bestCos) {
            bestCos = angleCos;
            out.partIndex = part->GetPartIndex();
            out.worldPosition = partPosition;
            out.distance = distance;
            out.angleDegrees = std::acos(std::clamp(angleCos, -1.0f, 1.0f)) * 180.0f / std::numbers::pi_v<float>;
        }
    }

    return out.IsValid();
}

int BossPartGraph::FindPartIndex(const ColliderBase *collider) const {
    if (!collider) {
        return -1;
    }
    for (const std::unique_ptr<BossPart> &part : parts_) {
        if (part->GetPartCollider() == collider) {
            return part->GetPartIndex();
        }
    }
    return -1;
}

float BossPartGraph::GetExposure() const {
    const int total = GetTotalCount();
    if (total <= 0) {
        return 0.0f;
    }
    return static_cast<float>(total - aliveCount_) / static_cast<float>(total);
}

int BossPartGraph::CountAlive(Color color) const {
    int count = 0;
    for (const std::unique_ptr<BossPart> &part : parts_) {
        if (part->IsPartAlive() && part->GetPartColor() == color) {
            ++count;
        }
    }
    return count;
}

int BossPartGraph::CountPoppableClusters(int minMatch, int *outPartCount) const {
    std::vector<bool> counted(parts_.size(), false);
    int clusterCount = 0;
    int partCount = 0;

    for (size_t i = 0; i < parts_.size(); ++i) {
        if (counted[i] || !parts_[i]->IsPartAlive()) {
            continue;
        }
        const std::vector<int> cluster = CollectSameColorCluster(static_cast<int>(i));
        for (int index : cluster) {
            counted[index] = true;
        }
        if (static_cast<int>(cluster.size()) >= minMatch) {
            ++clusterCount;
            partCount += static_cast<int>(cluster.size());
        }
    }

    if (outPartCount) {
        *outPartCount = partCount;
    }
    return clusterCount;
}

BossPart *BossPartGraph::GetPart(int index) {
    if (index < 0 || index >= static_cast<int>(parts_.size())) {
        return nullptr;
    }
    return parts_[index].get();
}

const BossPart *BossPartGraph::GetPart(int index) const {
    if (index < 0 || index >= static_cast<int>(parts_.size())) {
        return nullptr;
    }
    return parts_[index].get();
}

void BossPartGraph::SetHighlightedPart(int index) {
    if (highlightedIndex_ == index) {
        return;
    }
    if (BossPart *previous = GetPart(highlightedIndex_)) {
        previous->SetHighlight(false);
    }
    highlightedIndex_ = index;
    if (BossPart *current = GetPart(highlightedIndex_)) {
        current->SetHighlight(true);
    }
}

void BossPartGraph::AssignColors(const BossColorPalette &palette, const BossChainParams &chain, uint32_t colorSeed) {
    const std::vector<Color> &usedColors = palette.GetUsedColors();
    if (usedColors.empty() || parts_.empty()) {
        return;
    }

    const uint32_t seed = (colorSeed != 0) ? colorSeed : std::random_device{}();
    std::mt19937 engine(seed);

    // -1 = 未割り当て
    std::vector<int> assignedColors(parts_.size(), -1);
    std::vector<Color> candidates = usedColors;

    for (size_t i = 0; i < parts_.size(); ++i) {
        std::shuffle(candidates.begin(), candidates.end(), engine);

        int chosen = BossColorPalette::ToIndex(candidates.front());
        for (Color candidate : candidates) {
            assignedColors[i] = BossColorPalette::ToIndex(candidate);
            // 一撃で削れすぎる巨大な塊だけを避ける。
            // ここで「連鎖が成立しない配色」にしてしまうと、色は後から変わらないので
            // 永久に壊せない盤面になる。塊が生まれること自体はゲームの前提
            if (chain.maxInitialCluster <= 0 ||
                CountAssignedCluster(static_cast<int>(i), assignedColors) <= chain.maxInitialCluster) {
                chosen = assignedColors[i];
                break;
            }
        }
        assignedColors[i] = chosen;

        const Color color = BossColorPalette::FromIndex(chosen);
        parts_[i]->SetPartColor(color, palette.GetRgba(color));
    }

    EnsurePoppableCluster(palette, chain);

    // 配色が決まった時点で「壊せるパーツの総数」も確定する（破壊は他の塊に影響しないため）
    destroyablePartCount_ = 0;
    CountPoppableClusters(chain.minMatch, &destroyablePartCount_);
}

void BossPartGraph::EnsurePoppableCluster(const BossColorPalette &palette, const BossChainParams &chain) {
    if (parts_.empty() || chain.minMatch <= 1) {
        return;
    }

    // 破壊可能な塊が1つでもあれば何もしない
    if (CountPoppableClusters(chain.minMatch) > 0) {
        return;
    }

    // 乱数の巡り合わせで1つも無かった場合の保険として、起点とその隣を同色に塗る
    const int startIndex = 0;
    const Color color = parts_[startIndex]->GetPartColor();
    int painted = 1;
    for (int neighbor : adjacency_[startIndex]) {
        if (painted >= chain.minMatch) {
            break;
        }
        parts_[neighbor]->SetPartColor(color, palette.GetRgba(color));
        ++painted;
    }
}

int BossPartGraph::CountAssignedCluster(int startIndex, const std::vector<int> &assignedColors) const {
    const int targetColor = assignedColors[startIndex];
    if (targetColor < 0) {
        return 0;
    }

    ++currentStamp_;
    if (currentStamp_ == 0) {
        std::fill(visitStamps_.begin(), visitStamps_.end(), 0);
        currentStamp_ = 1;
    }

    searchQueue_.clear();
    searchQueue_.push_back(startIndex);
    visitStamps_[startIndex] = currentStamp_;

    int count = 0;
    for (size_t head = 0; head < searchQueue_.size(); ++head) {
        const int current = searchQueue_[head];
        ++count;
        for (int neighbor : adjacency_[current]) {
            if (visitStamps_[neighbor] == currentStamp_ || assignedColors[neighbor] != targetColor) {
                continue;
            }
            visitStamps_[neighbor] = currentStamp_;
            searchQueue_.push_back(neighbor);
        }
    }
    return count;
}
