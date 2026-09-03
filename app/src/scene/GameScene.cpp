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
	pObjectManager_->LoadAll("GameScene");

	followCamera_ = std::make_unique<FollowCamera>();

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

	/// ===================================================
    /// ゲームの初期化
    /// ===================================================
    
	// ゲーム入力の生成
	gameInput_ = std::make_unique<GameInput>();

	// プレイヤーの生成初期化
	player_ = std::make_unique<Player>();
    player_->Init("Player");

	followCamera_->Init();
	followCamera_->SetTarget(player_->GetWorldTransform());

	// 初期化した時点から追従カメラで描画されるようにする
	followCamera_->Activate();

	pObjectManager_->RegisterExternal(player_.get());
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
	
	// ゲーム入力の更新
	gameInput_->UpdateInputState();

	player_->CommandExecute(gameInput_->GetInputContext());
	
	followCamera_->Update();

	CameraUpdate();
	
}

void GameScene::AddSceneSetting() {
	/// ===================================================
	///シーン設定(デバッグ)
	/// ===================================================
	DrawDebugCameraImGui();
	camera_->ShowDebugWindow();

	// 追従カメラの調整（距離や高さ）
	if (followCamera_)
	{
		followCamera_->DrawImGui();
	}
}

void GameScene::AddObjectSetting()
{
	/// ===================================================
	/// オブジェクト設定（デバッグ）
	/// ===================================================
}
void GameScene::AddParticleSetting()
{
	/// ===================================================
	/// パーティクル設定（デバッグ）
	/// ===================================================
}

void GameScene::CameraUpdate()
{
	/// ===================================================
	/// カメラ更新
	/// ===================================================
	UpdateDebugCamera();
}

void GameScene::ChangeScene() {
	/// ===================================================
	/// シーン切り替え
	/// ===================================================

	//pSceneManager_->NextSceneReservation();
}