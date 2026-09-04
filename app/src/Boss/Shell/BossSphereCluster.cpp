#include "BossSphereCluster.h"
#include "MyMath.h"
#include "camera/projection/ViewProjection.h"
#include "line/LineRenderer.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>
#include <unordered_set>

using namespace Hagine;

namespace {

/// <summary>同じ辺を二重に描かないための、セルの順序比較</summary>
bool IsLexicographicallyLess(const ShellCell &a, const ShellCell &b) {
    if (a.vertex != b.vertex) {
        return a.vertex < b.vertex;
    }
    return a.layer < b.layer;
}

} // namespace

void BossSphereCluster::Build(BaseObject *parent, const std::string &namePrefix,
                              const BossShellParams &shell, const BossColorPalette &palette,
                              const BossChainParams &chain, uint32_t colorSeed) {
    pParent_ = parent;

    // 基本殻はハニカム（icosphereの頂点）。その外側・内側へ層を重ねられるようにする
    lattice_.Configure(shell.subdivision, shell.shellRadius, shell.sphereRadius,
                       shell.innerLayers, shell.outerLayers);
    sphereRadius_ = lattice_.GetSphereRadius();

    // 付着ぶんの余裕。未指定なら「全部の層が埋まっても足りる数」を確保する
    const int vertexCount = lattice_.GetVertexCount();
    const int extraCapacity = (shell.extraCapacity > 0)
                                  ? shell.extraCapacity
                                  : vertexCount * (shell.innerLayers + shell.outerLayers);
    EnsurePool(parent, namePrefix, vertexCount + extraCapacity, sphereRadius_);

    // 殻の見た目（同色をまとめて融合させたメッシュ）の描画先を用意する。
    // 1色が最大で全球を占める場合を考えて、GPU バッファはプール上限ぶん確保しておく
    metaBall_.Init(parent, palette, metaBallParams_, vertexCount + extraCapacity);

    FillInitialShell(shell, palette, chain, colorSeed);
}

void BossSphereCluster::ResetAll(const BossShellParams &shell, const BossColorPalette &palette,
                                 const BossChainParams &chain, uint32_t colorSeed) {
    // 消滅演出の途中の球も片付けてから敷き直す
    FlushVanishing();

    // 置かれている球をすべてプールへ戻してから敷き直す
    for (auto &[cell, slot] : occupied_) {
        slot.sphere->Deactivate();
        freeSpheres_.push_back(slot.sphere);
    }
    occupied_.clear();
    hasHighlight_ = false;
    MarkAllColorsDirty();

    // 使用色が変わっている場合に備えて、融合メッシュの色も入れ直す
    metaBall_.ApplyPalette(palette);

    FillInitialShell(shell, palette, chain, colorSeed);
}

void BossSphereCluster::ApplyRadius(const BossShellParams &shell) {
    // 演出途中の球が中途半端な大きさで残らないよう、先に片付ける
    FlushVanishing();

    lattice_.Configure(shell.subdivision, shell.shellRadius, shell.sphereRadius,
                       shell.innerLayers, shell.outerLayers);
    sphereRadius_ = lattice_.GetSphereRadius();

    for (std::unique_ptr<BossSphere> &sphere : pool_) {
        sphere->SetSphereRadius(sphereRadius_);
    }
    // 格子定数が変わるので、置かれている球の位置も引き直す（色はそのまま）
    for (auto &[cell, slot] : occupied_) {
        slot.sphere->SetLocalPosition(lattice_.ToLocal(cell));
    }

    // 位置と半径が変わったので、殻のメッシュは全色作り直す
    MarkAllColorsDirty();
}

void BossSphereCluster::SetMetaBallParams(const BossMetaBallParams &params) {
    metaBallParams_ = params;
    metaBall_.SetParams(params);
}

void BossSphereCluster::Update() {
    // 球が増減した色だけ、融合メッシュのもとになる中心座標を集め直す。
    // ボスが回っているだけなら（メッシュはローカル空間なので）ここは何もしない
    for (int index = 0; index < kGameColorCount; ++index) {
        if (!colorDirty_[index]) {
            continue;
        }
        colorDirty_[index] = false;

        const Color color = BossColorPalette::FromIndex(index);
        std::vector<Vector3> localPositions;
        localPositions.reserve(occupied_.size());
        for (const auto &[cell, slot] : occupied_) {
            if (slot.color == color) {
                localPositions.push_back(slot.sphere->GetLocalPosition());
            }
        }
        metaBall_.SetElements(color, std::move(localPositions), sphereRadius_);
    }

    metaBall_.Update();
}

void BossSphereCluster::DispatchCompute(float deltaTime) {
    metaBall_.DispatchCompute(deltaTime);
}

void BossSphereCluster::MarkColorDirty(Color color) {
    const int index = BossColorPalette::ToIndex(color);
    if (index >= 0 && index < kGameColorCount) {
        colorDirty_[index] = true;
    }
}

void BossSphereCluster::MarkAllColorsDirty() {
    colorDirty_.fill(true);
}

void BossSphereCluster::EnsurePool(BaseObject *parent, const std::string &namePrefix,
                                   int capacity, float radius) {
    pool_.reserve(static_cast<size_t>(capacity));

    while (static_cast<int>(pool_.size()) < capacity) {
        auto sphere = std::make_unique<BossSphere>();
        sphere->InitSphere(namePrefix + std::to_string(pool_.size()), radius);
        if (parent) {
            sphere->SetParent(parent);
            // 親（コア）のスケールは球へ伝播させない。位置と向きだけ追従させる
            sphere->GetWorldTransform()->inheritScale_ = false;
        }
        freeSpheres_.push_back(sphere.get());
        pool_.push_back(std::move(sphere));
    }
}

void BossSphereCluster::FillInitialShell(const BossShellParams &shell, const BossColorPalette &palette,
                                         const BossChainParams &chain, uint32_t colorSeed) {
    const std::vector<Color> &usedColors = palette.GetUsedColors();
    if (usedColors.empty()) {
        return;
    }

    const uint32_t seed = (colorSeed != 0) ? colorSeed : std::random_device{}();
    std::mt19937 engine(seed);
    std::vector<Color> candidates = usedColors;

    // 初期状態は基本殻（layer 0）だけを埋める。外側・内側の層は弾の付着用に空けておく
    for (const ShellCell &cell : lattice_.CollectBaseShellCells()) {
        std::shuffle(candidates.begin(), candidates.end(), engine);

        // 弾を1発当てただけで消える塊を最初から作らない。
        // 上限は「消去に必要な数 - 1」が基本で、これにより盤面は必ず
        // 「プレイヤーが1個足して初めて消える」状態から始まる
        Color chosen = candidates.front();
        for (Color candidate : candidates) {
            if (chain.maxInitialCluster <= 0 ||
                CountConnectedSameColor(cell, candidate) <= chain.maxInitialCluster) {
                chosen = candidate;
                break;
            }
        }

        if (!PlaceSphere(cell, chosen, palette)) {
            break; // プールが尽きた（容量設定が足りていない）
        }
    }

    initialCount_ = static_cast<int>(occupied_.size());
}

bool BossSphereCluster::PlaceSphere(const ShellCell &cell, Color color, const BossColorPalette &palette,
                                    const Vector3 *attachFrom) {
    if (freeSpheres_.empty()) {
        // 消滅演出の途中の球しか残っていない場合は、演出を打ち切って融通する
        FlushVanishing();
        if (freeSpheres_.empty()) {
            return false;
        }
    }
    BossSphere *sphere = freeSpheres_.back();
    freeSpheres_.pop_back();

    sphere->Place(cell, lattice_.ToLocal(cell), color, palette.GetRgba(color));
    occupied_[cell] = SphereSlot{color, sphere};

    if (attachFrom) {
        // 着弾点から定位置へ吸い寄せられる見せ方にする
        sphere->BeginAttach(*attachFrom, effect_.attachTime, effect_.attachStartScale);
    } else {
        sphere->ClearMotion(sphereRadius_);
    }

    MarkColorDirty(color);
    return true;
}

void BossSphereCluster::RemoveSphere(const ShellCell &cell, float vanishDelay) {
    auto it = occupied_.find(cell);
    if (it == occupied_.end()) {
        return;
    }
    BossSphere *sphere = it->second.sphere;
    it->second.sphere->Deactivate();
    freeSpheres_.push_back(it->second.sphere);
    MarkColorDirty(it->second.color);
    occupied_.erase(it);

    if (hasHighlight_ && highlightedCell_ == cell) {
        hasHighlight_ = false;
    }

    // 占有マップからは外すが、消えるまでは描画し続ける。
    // （当たり判定は occupied_ しか見ないので、演出中の球には当たらない）
    sphere->SetHighlight(false);
    sphere->BeginVanish(effect_.vanishTime, effect_.vanishDrift, vanishDelay);
    vanishing_.push_back(sphere);
}

void BossSphereCluster::FlushVanishing() {
    for (BossSphere *sphere : vanishing_) {
        sphere->ClearMotion(sphereRadius_);
        sphere->Deactivate();
        freeSpheres_.push_back(sphere);
    }
    vanishing_.clear();
}

void BossSphereCluster::UpdateMotions(float deltaTime) {
    // 吸着中の球（占有マップの中にいる）
    for (auto &[cell, slot] : occupied_) {
        slot.sphere->UpdateMotion(deltaTime, sphereRadius_);
    }

    // 消滅中の球。消え切ったものはプールへ返す
    for (size_t index = 0; index < vanishing_.size();) {
        BossSphere *sphere = vanishing_[index];
        if (sphere->UpdateMotion(deltaTime, sphereRadius_)) {
            ++index;
            continue;
        }
        sphere->Deactivate();
        freeSpheres_.push_back(sphere);
        vanishing_[index] = vanishing_.back();
        vanishing_.pop_back();
    }
}

Matrix4x4 BossSphereCluster::MakeShellMatrix() {
    if (!pParent_) {
        return MakeIdentity4x4();
    }
    // 球は親のスケールを継承しないので、格子空間も平行移動と回転だけで作る
    return MakeAffineMatrix(Vector3{1.0f, 1.0f, 1.0f}, pParent_->GetWorldRotation(),
                            pParent_->GetWorldPosition());
}

void BossSphereCluster::BeginAppear(const BossAppearParams &appear, uint32_t seed) {
    const uint32_t actualSeed = (seed != 0) ? seed : std::random_device{}();
    std::mt19937 engine(actualSeed);
    std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
    std::uniform_real_distribution<float> delayRange(0.0f, (std::max)(0.0f, appear.spawnSpread));

    for (auto &[cell, slot] : occupied_) {
        // 球面上の一様な向きへ散らす（そのままだと軸の周りに偏るので、
        // 長さが0に近い乱数は引き直す）
        Vector3 direction{};
        for (int retry = 0; retry < 8; ++retry) {
            direction = Vector3{unit(engine), unit(engine), unit(engine)};
            if (direction.LengthSq() > 0.05f) {
                break;
            }
        }
        if (direction.LengthSq() <= 0.0001f) {
            direction = Vector3{0.0f, 1.0f, 0.0f};
        }

        slot.sphere->SetAppearStart(direction.Normalize() * appear.gatherRadius, delayRange(engine));
    }

    // 開始時点の見た目（遠くに小さく散らばった状態）へ即座に反映する
    UpdateAppear(appear, 0.0f);
}

void BossSphereCluster::UpdateAppear(const BossAppearParams &appear, float elapsed) {
    const float gatherEnd = appear.gatherTime;
    const float settleEnd = gatherEnd + appear.settleTime;

    // 遅れてから動き出す球があるので、遅れの分だけ移動時間を短くして
    // 「gatherTime で全球が到着し終わる」ようにそろえる
    const float travelTime = (std::max)(0.01f, appear.gatherTime - appear.spawnSpread);

    for (auto &[cell, slot] : occupied_) {
        BossSphere *sphere = slot.sphere;
        const Vector3 target = lattice_.ToLocal(cell);

        if (elapsed < gatherEnd) {
            // --- 集束: 遠くから吸い寄せられる ---
            const float travel = std::clamp((elapsed - sphere->GetAppearDelay()) / travelTime, 0.0f, 1.0f);
            // 動き出しで加速し、定位置の手前で減速して収まる（到着時に急停止しない）
            sphere->SetLocalPosition(ApplyEasing(EasingType::InOutCubic, sphere->GetAppearStart(), target, travel, 1.0f));
            sphere->SetSphereRadius(sphereRadius_ *
                                    Lerp(appear.startScale, appear.arriveScale, travel));
            continue;
        }

        sphere->SetLocalPosition(target);

        if (elapsed < settleEnd) {
            // --- 到着後、回転が収まるまでは小さいまま待つ ---
            sphere->SetSphereRadius(sphereRadius_ * appear.arriveScale);
            continue;
        }

        // --- 膨張: 少し行き過ぎてから落ち着く（OutBack）---
        const float expand = std::clamp((elapsed - settleEnd) / (std::max)(0.01f, appear.expandTime), 0.0f, 1.0f);
        sphere->SetSphereRadius(ApplyEasing(EasingType::OutBack, sphereRadius_ * appear.arriveScale,
                                            sphereRadius_, expand, 1.0f));
    }
}

void BossSphereCluster::FinishAppear() {
    for (auto &[cell, slot] : occupied_) {
        slot.sphere->SetLocalPosition(lattice_.ToLocal(cell));
        slot.sphere->SetSphereRadius(sphereRadius_);
    }
}

void BossSphereCluster::Draw(const ViewProjection &viewProjection) {
    // 殻は色ごとに1枚の融合メッシュとして描く（同じ色同士だけがくっつく）
    metaBall_.Draw(viewProjection);

    // ロックオン中の球だけは実体を重ねて出す。
    // 融合メッシュは色を1つしか持てないので、1個だけ色を変えられないため
    if (hasHighlight_) {
        auto it = occupied_.find(highlightedCell_);
        if (it != occupied_.end()) {
            it->second.sphere->Draw(viewProjection);
        }
    }
}

void BossSphereCluster::DebugDraw() {
    LineRenderer *lineRenderer = LineRenderer::GetInstance();

    for (auto &[cell, slot] : occupied_) {
        const int neighborCount = lattice_.GetNeighborCount(cell);
        for (int index = 0; index < neighborCount; ++index) {
            const ShellCell neighbor = lattice_.GetNeighbor(cell, index);
            if (!IsLexicographicallyLess(cell, neighbor)) {
                continue; // 同じ辺を2回描かない
            }
            auto it = occupied_.find(neighbor);
            if (it == occupied_.end()) {
                continue;
            }
            const bool sameColor = it->second.color == slot.color;
            const Vector4 color = sameColor ? Vector4{1.0f, 1.0f, 1.0f, 1.0f}
                                            : Vector4{0.25f, 0.25f, 0.30f, 1.0f};
            lineRenderer->AddLine(slot.sphere->GetWorldPosition(), it->second.sphere->GetWorldPosition(), color);
        }
    }
}

bool BossSphereCluster::RaycastLocal(const Vector3 &localStart, const Vector3 &localEnd,
                                     ShellCell &outCell, Vector3 &outHitPoint) const {
    const Vector3 segment = localEnd - localStart;
    const float length = segment.Length();
    if (length <= 0.0001f) {
        return false;
    }
    const Vector3 direction = segment / length;
    const float radiusSq = sphereRadius_ * sphereRadius_;

    float nearestT = length;
    bool found = false;

    // 球数は数百個規模なので、まずは総当たり（統計を見てから最適化する）
    for (const auto &[cell, slot] : occupied_) {
        const Vector3 toStart = localStart - slot.sphere->GetLocalPosition();
        const float b = toStart.Dot(direction);
        const float c = toStart.LengthSq() - radiusSq;
        const float discriminant = b * b - c;
        if (discriminant < 0.0f) {
            continue;
        }

        const float root = std::sqrt(discriminant);
        float t = -b - root;
        if (t < 0.0f) {
            t = -b + root; // 始点が球の内側にある場合
        }
        if (t < 0.0f || t > nearestT) {
            continue;
        }

        nearestT = t;
        outCell = cell;
        found = true;
    }

    if (found) {
        outHitPoint = localStart + direction * nearestT;
    }
    return found;
}

bool BossSphereCluster::FindSnapCell(const ShellCell &hitCell, const Vector3 &localHitPoint,
                                     ShellCell &outCell) const {
    float nearestDistanceSq = 0.0f;
    bool found = false;

    const int neighborCount = lattice_.GetNeighborCount(hitCell);
    for (int index = 0; index < neighborCount; ++index) {
        const ShellCell candidate = lattice_.GetNeighbor(hitCell, index);
        if (occupied_.count(candidate) > 0 || !lattice_.IsInBand(candidate)) {
            continue;
        }

        const float distanceSq = (lattice_.ToLocal(candidate) - localHitPoint).LengthSq();
        if (!found || distanceSq < nearestDistanceSq) {
            nearestDistanceSq = distanceSq;
            outCell = candidate;
            found = true;
        }
    }
    return found;
}

BulletHitResult BossSphereCluster::RaycastAttach(const Vector3 &worldStart, const Vector3 &worldEnd,
                                                 Color color, const BossChainParams &chain,
                                                 const BossColorPalette &palette) {
    BulletHitResult result{};
    if (occupied_.empty()) {
        return result;
    }

    // ワールド→ローカルの変換は着弾判定1回につき逆行列1回だけ（球の数には比例しない）
    const Matrix4x4 shellMatrix = MakeShellMatrix();
    const Matrix4x4 inverseMatrix = Inverse(shellMatrix);
    const Vector3 localStart = Transformation(worldStart, inverseMatrix);
    const Vector3 localEnd = Transformation(worldEnd, inverseMatrix);

    ShellCell hitCell{};
    Vector3 localHitPoint{};
    if (!RaycastLocal(localStart, localEnd, hitCell, localHitPoint)) {
        return result; // 穴を通り抜けた（球が無いセルはレイが素通りする）
    }

    result.hit = true;
    result.hitPoint = Transformation(localHitPoint, shellMatrix);

    ShellCell snapCell{};
    if (!FindSnapCell(hitCell, localHitPoint, snapCell)) {
        return result; // 当たったが置ける隣が無い（弾は消えるだけ）
    }
    // 着弾点から定位置へ吸い寄せられる演出付きで置く
    if (!PlaceSphere(snapCell, color, palette, &localHitPoint)) {
        return result; // プールが尽きた
    }
    result.attached = true;

    const std::vector<ShellCell> cluster = CollectSameColorCluster(snapCell);
    result.clusterSize = static_cast<int>(cluster.size());
    if (result.clusterSize < chain.minMatch) {
        return result; // 付着しただけ。まだ消えない
    }

    // 幅優先で集めた順＝着弾点から近い順なので、少しずつ遅らせると
    // 当てたところから外へ波が広がるように消える
    for (size_t index = 0; index < cluster.size(); ++index) {
        RemoveSphere(cluster[index], static_cast<float>(index) * effect_.vanishSpread);
    }

    const int overMatch = result.clusterSize - chain.minMatch;
    result.destroyed = true;
    // まとめて消したほど長く怯む（HPは持たず、殻を削り切ることが撃破条件）
    result.staggerTime = chain.staggerBase + chain.staggerPerPart * static_cast<float>(overMatch);
    return result;
}

std::vector<ShellCell> BossSphereCluster::CollectSameColorCluster(const ShellCell &start) const {
    std::vector<ShellCell> cluster;

    auto startIt = occupied_.find(start);
    if (startIt == occupied_.end()) {
        return cluster;
    }
    const Color targetColor = startIt->second.color;

    std::unordered_set<ShellCell, ShellCellHash> visited;
    visited.insert(start);

    searchQueue_.clear();
    searchQueue_.push_back(start);

    for (size_t head = 0; head < searchQueue_.size(); ++head) {
        const ShellCell current = searchQueue_[head];
        cluster.push_back(current);

        const int neighborCount = lattice_.GetNeighborCount(current);
        for (int index = 0; index < neighborCount; ++index) {
            const ShellCell neighbor = lattice_.GetNeighbor(current, index);
            if (visited.count(neighbor) > 0) {
                continue;
            }
            auto it = occupied_.find(neighbor);
            if (it == occupied_.end() || it->second.color != targetColor) {
                continue;
            }
            visited.insert(neighbor);
            searchQueue_.push_back(neighbor);
        }
    }

    return cluster;
}

int BossSphereCluster::CountConnectedSameColor(const ShellCell &start, Color color) const {
    // start はまだ置かれていなくてよい（これから置く色を仮定して数える）
    std::unordered_set<ShellCell, ShellCellHash> visited;
    visited.insert(start);

    searchQueue_.clear();
    searchQueue_.push_back(start);

    int count = 0;
    for (size_t head = 0; head < searchQueue_.size(); ++head) {
        const ShellCell current = searchQueue_[head];
        ++count;

        const int neighborCount = lattice_.GetNeighborCount(current);
        for (int index = 0; index < neighborCount; ++index) {
            const ShellCell neighbor = lattice_.GetNeighbor(current, index);
            if (visited.count(neighbor) > 0) {
                continue;
            }
            auto it = occupied_.find(neighbor);
            if (it == occupied_.end() || it->second.color != color) {
                continue;
            }
            visited.insert(neighbor);
            searchQueue_.push_back(neighbor);
        }
    }
    return count;
}

bool BossSphereCluster::FindLockOnTarget(const LockOnRequest &request, bool requireFacing, LockOnResult &out) {
    out = LockOnResult{};

    if (request.aimDirection.LengthSq() <= 0.0f) {
        return false;
    }
    const Vector3 aimDirection = request.aimDirection.Normalize();
    const float maxAngleCos = std::cos(request.maxAngleDegrees * std::numbers::pi_v<float> / 180.0f);

    float bestCos = -2.0f;
    for (auto &[cell, slot] : occupied_) {
        if (slot.color != request.color) {
            continue;
        }

        const Vector3 spherePosition = slot.sphere->GetWorldPosition();
        const Vector3 toSphere = spherePosition - request.origin;
        const float distance = toSphere.Length();
        if (distance <= 0.0001f || distance > request.maxDistance) {
            continue;
        }

        const Vector3 toSphereDirection = toSphere / distance;
        const float angleCos = aimDirection.Dot(toSphereDirection);
        if (angleCos < maxAngleCos) {
            continue; // 照準の許容角度から外れている
        }

        if (requireFacing && slot.sphere->GetWorldNormal().Dot(toSphereDirection) >= 0.0f) {
            continue; // 殻の裏側は狙わない
        }

        if (angleCos > bestCos) {
            bestCos = angleCos;
            out.found = true;
            out.cell = cell;
            out.worldPosition = spherePosition;
            out.distance = distance;
            out.angleDegrees = std::acos(std::clamp(angleCos, -1.0f, 1.0f)) * 180.0f / std::numbers::pi_v<float>;
        }
    }

    return out.found;
}

bool BossSphereCluster::TryGetCellWorldPosition(const ShellCell &cell, Vector3 &out) {
    auto it = occupied_.find(cell);
    if (it == occupied_.end()) {
        return false;
    }
    out = it->second.sphere->GetWorldPosition();
    return true;
}

void BossSphereCluster::SetHighlightedCell(const ShellCell &cell, bool valid) {
    if (hasHighlight_ && highlightedCell_ == cell && valid) {
        return;
    }

    if (hasHighlight_) {
        auto previous = occupied_.find(highlightedCell_);
        if (previous != occupied_.end()) {
            previous->second.sphere->SetHighlight(false, metaBallParams_.highlightScale);
        }
        hasHighlight_ = false;
    }

    if (!valid) {
        return;
    }
    auto current = occupied_.find(cell);
    if (current != occupied_.end()) {
        current->second.sphere->SetHighlight(true, metaBallParams_.highlightScale);
        highlightedCell_ = cell;
        hasHighlight_ = true;
    }
}

float BossSphereCluster::GetExposure() const {
    if (initialCount_ <= 0) {
        return 0.0f;
    }
    const int removed = initialCount_ - static_cast<int>(occupied_.size());
    return std::clamp(static_cast<float>(removed) / static_cast<float>(initialCount_), 0.0f, 1.0f);
}

int BossSphereCluster::CountAlive(Color color) const {
    int count = 0;
    for (const auto &[cell, slot] : occupied_) {
        if (slot.color == color) {
            ++count;
        }
    }
    return count;
}
