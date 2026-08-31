#include "GameScene.h"
#include <utility/scene/SceneManager.h>
#include <utility/scene/SceneRegistry.h>

REGISTER_SCENE("GAME", GameScene)

using namespace Hagine;

void GameScene::Initialize()
{
    /// ===================================================
    /// 初期化
    /// ===================================================
    BaseScene::Initialize();
    vp_.Initialize();

    debugCamera_ = std::make_unique<DebugCamera>();
    debugCamera_->Initialize(&vp_);

    // 3Dオブジェクトの描画（ポストエフェクトあり）
    pDrawSystem_->Register("GameScene_PreDraw", DrawLayer::PreEffect, [this](const ViewProjection& vp)
        {
            pObjectManager_->Draw(vp);
        });

    // スプライトの描画（ポストエフェクトなし）
    pDrawSystem_->Register("GameScene_PostDraw", DrawLayer::PostEffect, [this](const ViewProjection& vp)
        {
            pSpriteManager_->DrawAll();
        });
}

void GameScene::Finalize()
{
    /// ===================================================
    /// 終了処理
    /// ===================================================
    BaseScene::Finalize();
}

void GameScene::Update()
{
    /// ===================================================
    /// 更新処理
    /// ===================================================
    vp_.UpdateMatrix();
}
