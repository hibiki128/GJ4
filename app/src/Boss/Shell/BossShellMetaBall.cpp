#include "BossShellMetaBall.h"
#include "src/Boss/Shell/BossSphere.h"
#include "DirectXCommon.h"
#include "camera/projection/ViewProjection.h"
#include "graphics/model/ModelManager.h"
#include "object/Object3d.h"
#include "object/base/BaseObject.h"
#include "transform/WorldTransform.h"
#include <algorithm>

using namespace Hagine;

namespace {

/// GPU 生成で使う格子の上限（1軸あたりのサンプル点数）。
/// セルを細かくしすぎた場合は、切り捨てずにセルを粗くして収める
constexpr uint32_t kMaxGridSamples = 96;

} // namespace

BossShellMetaBall::~BossShellMetaBall() {
    Release();
    gpuField_.ClearTargets();
}

void BossShellMetaBall::Init(BaseObject *parent, const BossColorPalette &palette,
                             const BossMetaBallParams &params, int maxBallCount) {
    params_ = params;
    maxBallCount_ = (std::max)(maxBallCount, 1);

    if (!transform_) {
        transform_ = std::make_unique<WorldTransform>();
        transform_->Initialize();
    }
    if (parent) {
        // メッシュはボスのローカル空間で作るので、掛けるのは位置と回転だけ。
        // コアのスケール（球の大きさ調整）は殻へ伝播させない
        transform_->pParent_ = parent->GetWorldTransform();
        transform_->inheritScale_ = false;
    }
    transform_->UpdateMatrix();

    CreateMeshes();

    for (ColorMesh &mesh : meshes_) {
        mesh.localPositions.clear();
        mesh.sphereRadius = 0.0f;
        mesh.hasMesh = false;
        mesh.dirty = true; // 球はこれから置かれるので、空の状態から作り直させる
        mesh.stats = MetaBallBuildStats{};
    }

    ApplyPalette(palette);
}

void BossShellMetaBall::CreateMeshes() {
    // GPU 生成と CPU 生成では頂点バッファの作り方が違うので、切り替わったら作り直す
    if (!meshes_[0].obj3d || createdAsGpu_ != params_.useGpu) {
        if (meshes_[0].obj3d) {
            // 描画中のバッファを消さないよう、GPU が使い終わるのを待ってから捨てる
            DirectXCommon::GetInstance()->WaitForGPU();
        }
        Release();
        // 出力先が入れ替わるので、古い頂点バッファに張った UAV は忘れさせる
        gpuField_.ClearTargets();
        createdAsGpu_ = params_.useGpu;

        for (ColorMesh &mesh : meshes_) {
            mesh.obj3d = std::make_unique<Object3d>();
            mesh.obj3d->Initialize();
            if (createdAsGpu_) {
                // 形はコンピュートシェーダーが書く。描画数は常に上限ぶんで、
                // 書かれなかった頂点は面積0の三角形として捨てられる
                const uint32_t maxVertexCount =
                    static_cast<uint32_t>((std::max)(params_.maxTrianglesPerColor, 1) * 3);
                mesh.obj3d->CreateGpuWritableModel(kBossTexturePath, maxVertexCount);
            } else {
                mesh.obj3d->CreateDynamicModel(kBossTexturePath);
            }
            // 球を1個ずつ描いていたとき（BaseObject の既定）と同じ扱いにする
            mesh.obj3d->SetBlendMode(BlendMode::Normal);
        }
    }

    if (createdAsGpu_ && !gpuReady_) {
        gpuField_.Initialize("BossShell", static_cast<uint32_t>(maxBallCount_), kMaxGridSamples);
        gpuReady_ = true;
    }
}

void BossShellMetaBall::SetElements(Color color, std::vector<Vector3> localPositions,
                                    float sphereRadius) {
    const int index = BossColorPalette::ToIndex(color);
    if (index < 0 || index >= kGameColorCount) {
        return;
    }
    ColorMesh &mesh = meshes_[index];
    mesh.localPositions = std::move(localPositions);
    mesh.sphereRadius = sphereRadius;
    mesh.dirty = true;

    // GPU 生成では毎フレーム作り直すので、ここでは「描くかどうか」だけ決まればよい
    if (params_.useGpu) {
        mesh.hasMesh = !mesh.localPositions.empty() && mesh.sphereRadius > 0.0f;
    }
}

void BossShellMetaBall::SetParams(const BossMetaBallParams &params) {
    const bool modeChanged = (params.useGpu != params_.useGpu) ||
                             (params.maxTrianglesPerColor != params_.maxTrianglesPerColor);
    params_ = params;

    if (modeChanged && meshes_[0].obj3d) {
        // 生成方法や上限が変わったら頂点バッファごと作り直す
        CreateMeshes();
        for (int index = 0; index < kGameColorCount; ++index) {
            if (meshes_[index].obj3d) {
                meshes_[index].obj3d->SetColor(colors_[index], 0);
            }
        }
    }

    for (ColorMesh &mesh : meshes_) {
        mesh.dirty = true;
        if (params_.useGpu) {
            mesh.hasMesh = !mesh.localPositions.empty() && mesh.sphereRadius > 0.0f;
        }
    }
}

void BossShellMetaBall::ApplyPalette(const BossColorPalette &palette) {
    for (int index = 0; index < kGameColorCount; ++index) {
        colors_[index] = palette.GetRgba(BossColorPalette::FromIndex(index));
        if (meshes_[index].obj3d) {
            meshes_[index].obj3d->SetColor(colors_[index], 0);
        }
    }
}

void BossShellMetaBall::Update() {
    if (params_.useGpu) {
        return; // GPU 生成は毎フレーム DispatchCompute で作り直す
    }

    float totalMilliseconds = 0.0f;
    bool rebuilt = false;

    for (int index = 0; index < kGameColorCount; ++index) {
        if (!meshes_[index].dirty) {
            continue;
        }
        RebuildOnCpu(index);
        totalMilliseconds += meshes_[index].stats.buildMilliseconds;
        rebuilt = true;
    }

    if (rebuilt) {
        lastBuildMilliseconds_ = totalMilliseconds;
    }
}

void BossShellMetaBall::DispatchCompute(float deltaTime) {
    if (!params_.useGpu || !gpuReady_) {
        return;
    }
    elapsedTime_ += deltaTime;

    DirectXCommon *dxCommon = DirectXCommon::GetInstance();
    ID3D12GraphicsCommandList *pCommandList = dxCommon->GetComputeCommandList().Get();
    if (!pCommandList) {
        return;
    }
    dxCommon->BeginComputeFrame();

    MetaBallGpuParams gpuParams{};
    gpuParams.threshold = params_.threshold;
    gpuParams.wobbleAmplitude = params_.wobbleAmplitude;
    gpuParams.wobbleSpeed = params_.wobbleSpeed;
    gpuParams.wobbleFrequency = params_.wobbleFrequency;

    for (ColorMesh &mesh : meshes_) {
        if (!mesh.obj3d) {
            continue;
        }
        // 球が無い色も一度は走らせる（前フレームの殻を消すため）。
        // 完全に空のまま2回目以降は、消し済みなので走らせなくてよい
        if (mesh.localPositions.empty() && !mesh.hasMesh) {
            continue;
        }

        gpuParams.voxelSize = (std::max)(mesh.sphereRadius * params_.voxelRatio, 0.001f);
        gpuField_.SetBalls(mesh.localPositions, mesh.sphereRadius * params_.influenceScale);
        gpuField_.Dispatch(pCommandList, mesh.obj3d.get(), gpuParams, elapsedTime_);

        mesh.hasMesh = !mesh.localPositions.empty() && mesh.sphereRadius > 0.0f;
    }
}

void BossShellMetaBall::RebuildOnCpu(int colorIndex) {
    ColorMesh &mesh = meshes_[colorIndex];
    mesh.dirty = false;
    if (!mesh.obj3d) {
        return;
    }

    // その色の球が全部消えたら空のメッシュにする（前の形が残らないように必ず入れ直す）
    if (mesh.localPositions.empty() || mesh.sphereRadius <= 0.0f) {
        mesh.hasMesh = false;
        mesh.stats = MetaBallBuildStats{};
        mesh.obj3d->RebuildDynamicMesh(MeshData{});
        return;
    }

    scratch_.clear();
    scratch_.reserve(mesh.localPositions.size());
    for (const Vector3 &localPosition : mesh.localPositions) {
        MetaBallElement element{};
        element.position = localPosition;
        // しきい値0.5では単体の見た目の半径が影響半径の半分になるので、
        // influenceScale が 2.0 のとき見た目が球の半径と一致する
        element.radius = mesh.sphereRadius * params_.influenceScale;
        element.stiffness = 1.0f;
        scratch_.push_back(element);
    }

    MetaBallWorldParams buildParams{};
    buildParams.voxelSize = (std::max)(mesh.sphereRadius * params_.voxelRatio, 0.001f);
    buildParams.threshold = params_.threshold;

    MeshData data = MetaBallBuilder::BuildClustered(scratch_, buildParams, &mesh.stats);
    mesh.hasMesh = !data.indices.empty();
    mesh.obj3d->RebuildDynamicMesh(std::move(data));
}

void BossShellMetaBall::Draw(const ViewProjection &viewProjection) {
    if (!transform_) {
        return;
    }

    // 親の行列が確定するのは全オブジェクトの更新後なので、ここで引き直す
    transform_->UpdateMatrix();

    for (ColorMesh &mesh : meshes_) {
        if (!mesh.hasMesh || !mesh.obj3d) {
            continue;
        }
        mesh.obj3d->Draw(*transform_, viewProjection, false, true, true);
    }
}

int BossShellMetaBall::GetTotalTriangleCount() const {
    int total = 0;
    for (const ColorMesh &mesh : meshes_) {
        total += static_cast<int>(mesh.stats.triangleCount);
    }
    return total;
}

void BossShellMetaBall::Release() {
    for (ColorMesh &mesh : meshes_) {
        if (!mesh.obj3d) {
            continue;
        }
        // モデルの実体は ModelManager が持っているので、Object3d を捨てるだけでは残る
        const std::string key = mesh.obj3d->GetDynamicModelKey();
        mesh.obj3d.reset();
        if (!key.empty()) {
            ModelManager::GetInstance()->RemoveModel(key);
        }
        mesh.hasMesh = false;
    }
}
