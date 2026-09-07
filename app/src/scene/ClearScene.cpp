#include"ClearScene.h"
#include <utility/scene/SceneManager.h>
#include <utility/scene/SceneRegistry.h>

REGISTER_SCENE("CLEAR", ClearScene)

using namespace Hagine;

void ClearScene::Initialize()
{
	/// ===================================================
	/// 初期化
	/// ===================================================
	BaseScene::Initialize();
	pObjectManager_->LoadAll("ClearScene");

	// 3Dオブジェクトの描画（ポストエフェクトあり）
	pDrawSystem_->Register("ClearScene_PreDraw", DrawLayer::PreEffect, [this](const ViewProjection& vp)
		{
			pObjectManager_->Draw(vp);
		});


	// スプライトの描画（ポストエフェクトなし）
	pDrawSystem_->Register("ClearScene_PostDraw", DrawLayer::PostEffect, [this](const ViewProjection& vp)
		{
			pSpriteManager_->DrawAll();
		});

}

void ClearScene::Finalize()
{
	/// ===================================================
	/// 終了処理
	/// ===================================================
	BaseScene::Finalize();
}

void ClearScene::Update()
{
	/// ===================================================
	/// 更新処理
	/// ===================================================

	CameraUpdate();

}

void ClearScene::AddSceneSetting() {
	/// ===================================================
	///シーン設定(デバッグ)
	/// ===================================================
	DrawDebugCameraImGui();
	camera_->ShowDebugWindow();
}

void ClearScene::AddObjectSetting()
{
	/// ===================================================
	/// オブジェクト設定（デバッグ）
	/// ===================================================

}

void ClearScene::AddParticleSetting()
{
	/// ===================================================
	/// パーティクル設定（デバッグ）
	/// ===================================================
}

void ClearScene::CameraUpdate()
{
	/// ===================================================
	/// カメラ更新
	/// ===================================================
	UpdateDebugCamera();
}

void ClearScene::ChangeScene() {
	/// ===================================================
	/// シーン切り替え
	/// ===================================================

	//pSceneManager_->NextSceneReservation();
}