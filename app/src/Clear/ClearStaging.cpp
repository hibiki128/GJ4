#include "ClearStaging.h"
#include "camera/Camera.h"
#include "object/base/BaseObjectManager.h"
#include <numbers>
#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

using namespace Hagine;

void ClearStaging::Init(BaseObjectManager *objectManager) {
    // 転がったボス（コア＋散らばった殻）
    bossRemains_ = std::make_unique<ClearBossRemains>();
    bossRemains_->Init("ClearBossRemains");

    // 喜んでいるプレイヤー。色マスタはボスの残骸が読んだものと同じ見た目にしたいので、
    // どちらも jsons/Boss/ColorMaster.json から読む
    player_ = std::make_unique<ClearPlayerActor>();
    player_->Init("ClearPlayer");
    {
        BossColorPalette palette{};
        palette.LoadMaster();
        player_->SetPalette(palette);
    }

    ApplyLayout();

    if (objectManager) {
        objectManager->RegisterExternal(bossRemains_.get());
        objectManager->RegisterExternal(player_.get());
    }
}

void ClearStaging::RegisterParams() {
    // 立ち位置を動かしたらその場で配り直す（構図をUIで詰められるように）
    GameParamHub::Options layoutOptions{};
    layoutOptions.speed = 0.1f;
    layoutOptions.onChange = [this] { ApplyLayout(); };

    params_.Register("PlayerPosition", &playerPosition_, layoutOptions);
    params_.Register("BossPosition", &bossPosition_, layoutOptions);
    params_.Register("CameraPosition", &cameraPosition_, {0.1f});
    params_.Register("CameraRotationDeg", &cameraRotation_, {0.5f});
    params_.Register("CameraFovDeg", &cameraFovDegrees_, {0.5f, 10.0f, 120.0f});

    player_->RegisterParams();
    bossRemains_->RegisterParams();

    // 保存済みの値が書き戻されても onChange は呼ばれないので、ここで反映し直す
    ApplyLayout();
}

void ClearStaging::ApplyLayout() {
    if (player_) {
        player_->SetHomePosition(playerPosition_);
    }
    if (bossRemains_) {
        bossRemains_->SetGroundPosition(bossPosition_);
    }
}

void ClearStaging::UpdateCamera(Camera *camera) {
    if (!camera) {
        return;
    }

    // 注視点は持たせない。演出中に登場人物が動いても構図が揺れないよう、
    // 位置と向きをそのまま置く
    constexpr float toRadian = std::numbers::pi_v<float> / 180.0f;
    camera->SetPosition(cameraPosition_);
    camera->SetRotation(Vector3{cameraRotation_.x * toRadian, cameraRotation_.y * toRadian,
                                cameraRotation_.z * toRadian});
    camera->SetFovYDegrees(cameraFovDegrees_);
}

void ClearStaging::DispatchCompute() {
    if (bossRemains_) {
        bossRemains_->DispatchShellCompute();
    }
}

void ClearStaging::Draw(const ViewProjection &viewProjection) {
    // コアとプレイヤーはオブジェクトマネージャーが描く。
    // 殻の破片は BaseObject ではないのでここで描く
    if (bossRemains_) {
        bossRemains_->DrawDebris(viewProjection);
    }
}

void ClearStaging::DrawImGui() {
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader("クリア演出", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::TextDisabled("構図・動きの調整は ゲームパラメータ の Clear/… から行う");
    ImGui::Text("プレイヤー: (%.2f, %.2f, %.2f)", playerPosition_.x, playerPosition_.y, playerPosition_.z);
    ImGui::Text("ボスの残骸: (%.2f, %.2f, %.2f)", bossPosition_.x, bossPosition_.y, bossPosition_.z);
    ImGui::Text("カメラ: (%.2f, %.2f, %.2f) / 向き (%.1f, %.1f, %.1f)度 / 画角 %.1f度",
                cameraPosition_.x, cameraPosition_.y, cameraPosition_.z, cameraRotation_.x,
                cameraRotation_.y, cameraRotation_.z, cameraFovDegrees_);
#endif // USE_IMGUI
}
