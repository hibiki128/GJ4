#include "TutorialScene.h"
#include "MyMath.h"
#include "src/Boss/Effect/BossParticles.h"
#include "src/UI/Pause/PauseMenu.h"
#include <frame/Frame.h>
#include <utility/scene/SceneManager.h>
#include <utility/scene/SceneRegistry.h>

REGISTER_SCENE("TUTORIAL", TutorialScene)

using namespace Hagine;

namespace {

/// <summary>完了テロップを見せてから本編へ送るまでの時間（秒）</summary>
constexpr float kFinishWait = 2.6f;

} // namespace

void TutorialScene::Initialize() {
    /// ===================================================
    /// 初期化
    /// ===================================================
    BaseScene::Initialize();
    pObjectManager_->LoadAll("TutorialScene");

    PauseMenu::GetInstance()->Initialize();
    PauseMenu::GetInstance()->CloseImmediately();

    followCamera_ = std::make_unique<FollowCamera>();

    // 3Dオブジェクトの描画（ポストエフェクトあり）
    pDrawSystem_->Register("TutorialScene_PreDraw", DrawLayer::PreEffect,
                           [this](const ViewProjection &vp) { pObjectManager_->Draw(vp); });

    // ボスの殻（メタボール）をGPUで作り直す。本編と同じ場所で走らせる
    pDrawSystem_->Register("TutorialScene_MetaBallCompute", DrawSystem::kGPUParticleCompute,
                           [this](const ViewProjection &) {
                               if (boss_) {
                                   boss_->DispatchShellCompute();
                               }
                           });

    // スプライトの描画。チュートリアルのテロップはHUDより手前に出す
    pDrawSystem_->Register("TutorialScene_PostDraw", DrawLayer::PostEffect,
                           [this](const ViewProjection &) {
                               pSpriteManager_->DrawAll();
                               // レティクルはテロップより下。テロップに隠れても構わない
                               if (ShouldDrawReticle()) {
                                   reticle_->Draw();
                               }
                               if (tutorial_) {
                                   tutorial_->Draw();
                               }
                           });

    pDrawSystem_->Register("TutorialScene_PauseMenu", DrawLayer::PostEffect,
                           [](const ViewProjection &) { PauseMenu::GetInstance()->Draw(); });

    /// ===================================================
    /// ゲームの初期化（本編と同じ配線。攻撃まわりだけ省いてある）
    /// ===================================================

    gameInput_ = std::make_unique<GameInput>();

    field_ = std::make_unique<Field>();
    field_->Init();

    // 外周の柱。本編と同じ場所に見せたいので、同じ半径・同じ見た目で置く
    fieldSurround_ = std::make_unique<FieldSurround>();
    fieldSurround_->Init(field_->GetRadius(), pObjectManager_, "TutorialScene");

    player_ = std::make_unique<Player>();
    player_->Init("Player");
    player_->SetFieldBounds(field_.get());

    followCamera_->Init();
    followCamera_->SetTarget(player_->GetWorldTransform());

    pObjectManager_->RegisterExternal(player_.get());

    // 的になるボス。第1形態だけを出す。
    // 反撃はさせないが、それ以外（登場演出・怯み・撃破）は本番どおり動かす。
    // 丸ごと止めると登場状態のまま無敵になり、弾がすり抜けてしまう
    boss_ = std::make_unique<Boss>();
    boss_->Init("Boss");
    boss_->SetFieldBounds(field_.get());
    boss_->SetAttackEnabled(bossAttacks_);
    pObjectManager_->RegisterExternal(boss_.get());

    player_->SetColorPalette(boss_->GetPalette());
    player_->SetBossTargetProvider(
        [this]() -> IBossTargetQuery * { return boss_.get(); });

    if (!boss_->GetUsedColors().empty()) {
        player_->SetSelectedColor(boss_->GetUsedColors().front(), true);
    }

    // ボスは Player の型を知らないので、本編と同じくラムダ越しに繋ぐ。
    // 止めてあるので実際には撃ってこないが、配線を欠くと参照先が無い状態になる
    playerBridge_ = std::make_unique<FunctionalPlayerBridge>(
        [pPlayer = player_.get()] { return pPlayer->GetWorldPosition(); },
        [pPlayer = player_.get()] { return pPlayer->GetSelectedColor(); });
    boss_->SetPlayerBridge(playerBridge_.get());
    boss_->SetTargetDamageSink(player_.get());
    playerBridge_->SetValidGetter([pPlayer = player_.get()] { return !pPlayer->IsDead(); });

    // 殻が消えたときの土煙などは本編と同じものを使う
    BossParticles::GetInstance()->Init();
    BossParticles::GetInstance()->SetMasterScale(boss_->GetParameters().GetMasterScale());

    // カメラが柱の中へ下がらないようにする（本編と同じ）
    followCamera_->SetObstacleProvider([this]() {
        static const char *kObstacleNames[] = {"plane"};
        std::vector<BaseObject *> obstacles;
        for (const char *name : kObstacleNames) {
            if (BaseObject *pObject = pObjectManager_->GetObjectByName(name)) {
                obstacles.push_back(pObject);
            }
        }
        if (fieldSurround_) {
            fieldSurround_->AppendObstacles(obstacles);
        }
        return obstacles;
    });

    // 最初は操作を覚える段なので、ボスを画面に収めず自分だけを追う。
    // 撃つ段になったら BossBattle へ切り替える
    followCamera_->SetMode(CameraMode::Normal);
    followCamera_->Activate();

    // 照準レティクル。本番と同じく、狙いが決まった直後に通知を受けて画面座標へ落とす
    reticle_ = std::make_unique<PlayerReticle>();
    reticle_->Init();
    reticle_->RegisterParams();
    player_->SetOnAimReport([this](const PlayerAimReport &report) {
        reticle_->Update(report, *GetViewProjection(), Frame::DeltaTime());
    });

    tutorial_ = std::make_unique<TutorialDirector>();
    tutorial_->Init();
    // 文字やテロップの大きさは ゲームパラメータ の Tutorial から変えられる
    tutorial_->RegisterParams();

    previousExposure_ = boss_->GetExposure();

    pOffScreen_->LoadData("GameScenePostEffect");
}

void TutorialScene::Finalize() {
    /// ===================================================
    /// 終了処理
    /// ===================================================
    if (tutorial_) {
        tutorial_->Finalize();
    }
    BaseScene::Finalize();
}

void TutorialScene::Update() {
    /// ===================================================
    /// 更新処理
    /// ===================================================
    PauseMenu::GetInstance()->Update();

    field_->DrawLine();

    if (PauseMenu::GetInstance()->IsPaused()) {
        CameraUpdate();
        return;
    }

    const float deltaTime = Frame::DeltaTime();

    gameInput_->UpdateInputState();
    fieldSurround_->Update();

    player_->CommandExecute(gameInput_->GetInputContext());

    // 撃つ段まで来たらボスも画面に収める。それまでは自分の操作に集中させる
    if (tutorial_->IsCombatStageReached() &&
        followCamera_->GetMode() != CameraMode::BossBattle) {
        followCamera_->SetMode(CameraMode::BossBattle);
    }

    followCamera_->Update(gameInput_->GetCameraContext());
    player_->SetCameraYaw(followCamera_->GetYaw());

    CameraUpdate();
    UpdateAim();

    // 進行役へ「今フレーム何が起きたか」を渡す
    tutorial_->Update(deltaTime, CollectSignals(deltaTime));

    // 終わったら、完了テロップを少し見せてから本編へ送る
    if (tutorial_->IsFinished()) {
        if (finishWait_ < 0.0f) {
            finishWait_ = kFinishWait;
        }
        finishWait_ -= deltaTime;
        if (finishWait_ <= 0.0f) {
            finishWait_ = 1000.0f; // 二重予約を防ぐ
            pSceneManager_->NextSceneReservation("GAME");
        }
    }
}

TutorialSignals TutorialScene::CollectSignals(float deltaTime) {
    (void)deltaTime;

    const PlayerInput &input = gameInput_->GetInputContext();
    const CameraInput &camera = gameInput_->GetCameraContext();

    TutorialSignals signals{};
    signals.moveStick = input.dir;
    signals.lookStick = camera.look;
    signals.jumped = input.jump;
    signals.dashing = input.dash;
    signals.shot = input.attack;
    signals.selectedColorIndex = input.selectColorIndex;

    // 殻は同色がそろったときにしか減らないので、削れ具合が増えた＝連鎖が成立した
    const float exposure = boss_->GetExposure();
    signals.chainCleared = (exposure > previousExposure_ + 0.0001f);
    previousExposure_ = exposure;

    signals.shellCleared = boss_->IsShellCleared();
    return signals;
}

bool TutorialScene::ShouldDrawReticle() const {
    /// ===================================================
    /// レティクルを出してよい場面か
    /// ===================================================
    if (!reticle_ || !player_) {
        return false;
    }
    return !PauseMenu::GetInstance()->IsPaused() && !player_->IsDead();
}

void TutorialScene::UpdateAim() {
    /// ===================================================
    /// 照準（カメラの射線）をプレイヤーへ配る
    /// ===================================================
    const ViewProjection &viewProjection = *GetViewProjection();
    const Matrix4x4 rotateMatrix = MakeRotateXYZMatrix(viewProjection.eulerRotation_);
    const Vector3 aimDirection = TransformNormal(kWorldForward, rotateMatrix).Normalize();
    const Vector3 aimOrigin = viewProjection.translation_ + aimDirection * 1.0f;

    player_->SetAim(aimOrigin, aimDirection);
}

void TutorialScene::CameraUpdate() {
    /// ===================================================
    /// カメラ更新
    /// ===================================================
    UpdateDebugCamera();
}

void TutorialScene::AddSceneSetting() {
    /// ===================================================
    /// シーン設定（デバッグ）
    /// ===================================================
    DrawDebugCameraImGui();
    camera_->ShowDebugWindow();

    PauseMenu::GetInstance()->DrawImGui();
}

void TutorialScene::AddObjectSetting() {
    /// ===================================================
    /// オブジェクト設定（デバッグ）
    /// ===================================================
    if (tutorial_) {
        tutorial_->DrawImGui();
    }

#ifdef USE_IMGUI
    if (ImGui::Checkbox("ボスに攻撃させる", &bossAttacks_)) {
        boss_->SetAttackEnabled(bossAttacks_);
    }
    ImGui::TextDisabled("切っていても登場・怯み・撃破は動く（撃てば普通に当たる）");
#endif // USE_IMGUI

    if (player_) {
        player_->DrawGameplayImGui();
    }
    if (field_) {
        field_->DrawImGui();
    }
    if (fieldSurround_) {
        fieldSurround_->DrawImGui();
    }
}

void TutorialScene::AddParticleSetting() {
    /// ===================================================
    /// パーティクル設定（デバッグ）
    /// ===================================================
}
