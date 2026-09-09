#include "GameOverStaging.h"
#include "camera/Camera.h"
#include "object/base/BaseObjectManager.h"
#include <numbers>
#include <string>
#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

using namespace Hagine;

void GameOverStaging::Init(BossFormId form, BaseObjectManager *objectManager) {
    form_ = form;

    // 形態ごとの既定の構図。蜘蛛は胴だけで高さ5〜6あるので、球体と同じ画角には収まらない。
    // カメラを引いて少し見上げ、脚の張り出しごと画に入れる
    Layout &sphere = layouts_[ToIndex(BossFormId::Sphere)];
    sphere.bossPosition = Vector3{-6.0f, 0.0f, 5.0f};
    sphere.playerPosition = Vector3{6.0f, 0.6f, 5.5f};
    sphere.cameraPosition = Vector3{0.0f, 1.2f, -9.0f};
    sphere.cameraRotation = Vector3{1.0f, 0.0f, 0.0f};
    sphere.fovDegrees = 55.0f;

    Layout &spider = layouts_[ToIndex(BossFormId::Spider)];
    spider.bossPosition = Vector3{-7.5f, 0.0f, 5.0f};
    spider.playerPosition = Vector3{8.2f, 0.6f, 7.0f};
    spider.cameraPosition = Vector3{0.0f, 1.5f, -15.0f};
    // わずかに見上げる。脚の先から胴の上まで入れたうえで、大きさを感じさせる
    spider.cameraRotation = Vector3{-1.0f, 0.0f, 0.0f};
    spider.fovDegrees = 50.0f;

    // 倒れたプレイヤー。色マスタはボスと同じ jsons/Boss/ColorMaster.json から読む
    player_ = std::make_unique<GameOverPlayerActor>();
    player_->Init("GameOverPlayer");
    {
        BossColorPalette palette{};
        palette.LoadMaster();
        player_->SetPalette(palette);
    }
    player_->SetGroundPosition(CurrentLayout().playerPosition);

    // 勝ったボス。立たせる場所と向く先は、足の置き場所を決めるのに要るので先に渡す
    boss_ = std::make_unique<GameOverBossActor>();
    boss_->Init(form_, CurrentLayout().bossPosition, CurrentLayout().playerPosition, objectManager);

    if (objectManager) {
        objectManager->RegisterExternal(player_.get());
    }
}

void GameOverStaging::RegisterParams() {
    // 立ち位置を動かしたらその場で配り直す（構図をUIで詰められるように）。
    // 出していない形態の値を触っても、その形態へ切り替えたときに効く
    for (size_t index = 0; index < 2; ++index) {
        Layout &layout = layouts_[index];

        GameParamHub::Options layoutOptions{};
        layoutOptions.speed = 0.1f;
        layoutOptions.onChange = [this] { ApplyLayout(); };

        layoutParams_[index].Register("BossPosition", &layout.bossPosition, layoutOptions);
        layoutParams_[index].Register("PlayerPosition", &layout.playerPosition, layoutOptions);
        layoutParams_[index].Register("CameraPosition", &layout.cameraPosition, {0.1f});
        layoutParams_[index].Register("CameraRotationDeg", &layout.cameraRotation, {0.5f});
        layoutParams_[index].Register("CameraFovDeg", &layout.fovDegrees, {0.5f, 10.0f, 120.0f});
    }

    player_->RegisterParams();
    boss_->RegisterParams();

    // 保存済みの値が書き戻されても onChange は呼ばれないので、ここで反映し直す
    ApplyLayout();
}

void GameOverStaging::SetForm(BossFormId form) {
    if (form_ == form) {
        return;
    }
    form_ = form;

    // 立ち位置を先に配ってから形態を入れ替える。
    // 蜘蛛は入れ替えのときに、そのとき渡っている場所へ足を下ろす
    ApplyLayout();
    if (boss_) {
        boss_->SetForm(form_);
    }
}

void GameOverStaging::ApplyLayout() {
    const Layout &layout = CurrentLayout();
    if (player_) {
        player_->SetGroundPosition(layout.playerPosition);
    }
    if (boss_) {
        boss_->SetGroundPosition(layout.bossPosition);
        boss_->SetLookTarget(layout.playerPosition);
    }
}

void GameOverStaging::Update() {
    // ボスの動きはオブジェクトの更新より前に置く。
    // ここで決めた胴の位置や脚の構えを、ボス自身の更新がその場で使う
    if (boss_) {
        boss_->Update();
    }
}

void GameOverStaging::UpdateCamera(Camera *camera) {
    if (!camera) {
        return;
    }

    // 注視点は持たせない。ボスが動いても構図が揺れないよう、位置と向きをそのまま置く
    constexpr float toRadian = std::numbers::pi_v<float> / 180.0f;
    const Layout &layout = CurrentLayout();
    camera->SetPosition(layout.cameraPosition);
    camera->SetRotation(Vector3{layout.cameraRotation.x * toRadian, layout.cameraRotation.y * toRadian,
                                layout.cameraRotation.z * toRadian});
    camera->SetFovYDegrees(layout.fovDegrees);
}

void GameOverStaging::DispatchCompute() {
    if (boss_) {
        boss_->DispatchCompute();
    }
}

void GameOverStaging::DrawImGui() {
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader("ゲームオーバー演出", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    // 本番はゲームシーンで負けた形態が入る。ここは両方の絵を確かめるための切り替え
    int formIndex = static_cast<int>(ToIndex(form_));
    if (ImGui::RadioButton("第1形態（球体）", &formIndex, 0)) {
        SetForm(BossFormId::Sphere);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("第2形態（蜘蛛）", &formIndex, 1)) {
        SetForm(BossFormId::Spider);
    }
    ImGui::TextDisabled("本番は「ゲームシーンで負けた瞬間の形態」が出ます（ここでの切り替えは確認用）");

    ImGui::Separator();
    if (boss_) {
        boss_->DrawImGui();
    }

    const Layout &layout = CurrentLayout();
    ImGui::TextDisabled("構図の調整は ゲームパラメータ の GameOver/Staging/%s から行う",
                        GameOverContext::GetFormName(form_));
    ImGui::Text("ボス: (%.2f, %.2f, %.2f)", layout.bossPosition.x, layout.bossPosition.y,
                layout.bossPosition.z);
    ImGui::Text("プレイヤー: (%.2f, %.2f, %.2f)", layout.playerPosition.x, layout.playerPosition.y,
                layout.playerPosition.z);
    ImGui::Text("カメラ: (%.2f, %.2f, %.2f) / 向き (%.1f, %.1f, %.1f)度 / 画角 %.1f度",
                layout.cameraPosition.x, layout.cameraPosition.y, layout.cameraPosition.z,
                layout.cameraRotation.x, layout.cameraRotation.y, layout.cameraRotation.z,
                layout.fovDegrees);
#endif // USE_IMGUI
}
