#include"ClearScene.h"
#include "src/UI/Pause/PauseMenu.h"
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
	// 床（ゲームシーンと同じ plane）をここで読み込む
	pObjectManager_->LoadAll("ClearScene");

	// ポーズ画面はどのシーンからでも開けるようにしてある
	PauseMenu::GetInstance()->Initialize();
	PauseMenu::GetInstance()->CloseImmediately();

	// 登場人物と画角。描画の登録より先に作っておく（描画コールバックから触るため）
	staging_ = std::make_unique<ClearStaging>();
	staging_->Init(pObjectManager_);
	staging_->RegisterParams();

	// 3Dオブジェクトの描画（ポストエフェクトあり）
	pDrawSystem_->Register("ClearScene_PreDraw", DrawLayer::PreEffect, [this](const ViewProjection& vp)
		{
			pObjectManager_->Draw(vp);
			// 散らばった殻は BaseObject ではないので、オブジェクトの後に自分で描く
			staging_->Draw(vp);
		});

	// 散らばった殻（メタボール）をGPUで作り直す。
	// ゲームシーンと同じく、シャドウ・G-Buffer より前のコンピュートフェーズで走らせる
	pDrawSystem_->Register("ClearScene_MetaBallCompute", DrawSystem::kGPUParticleCompute,
		[this](const ViewProjection&)
		{
			staging_->DispatchCompute();
		});

	// スプライトの描画（ポストエフェクトなし）
	pDrawSystem_->Register("ClearScene_PostDraw", DrawLayer::PostEffect, [this](const ViewProjection& vp)
		{
			pSpriteManager_->DrawAll();
		});

	// ポーズ画面（スプライトより手前に出したいので後から登録する）
	pDrawSystem_->Register("ClearScene_PauseMenu", DrawLayer::PostEffect, [](const ViewProjection& vp)
		{
			PauseMenu::GetInstance()->Draw();
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

	// コントローラーのメニュー（START）ボタン、キーボードは ESC で開閉する
	PauseMenu::GetInstance()->Update();

	CameraUpdate();

}

void ClearScene::AddObjectSetting()
{
	/// ===================================================
	/// オブジェクト設定（デバッグ）
	/// ===================================================
	if (staging_) {
		staging_->DrawImGui();
	}
}

void ClearScene::AddSceneSetting() {
	/// ===================================================
	///シーン設定(デバッグ)
	/// ===================================================
	DrawDebugCameraImGui();
	camera_->ShowDebugWindow();

	PauseMenu::GetInstance()->DrawImGui();
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

	// 構図は演出側が決める。デバッグカメラで見回している間は触らない
	// （触ると毎フレーム引き戻してしまい、視点を動かせなくなる）
	if (!IsDebugCameraActive()) {
		staging_->UpdateCamera(camera_);
	}
}

void ClearScene::ChangeScene() {
	/// ===================================================
	/// シーン切り替え
	/// ===================================================

	//pSceneManager_->NextSceneReservation();
}