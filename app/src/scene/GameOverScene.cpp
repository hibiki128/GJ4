#include"GameOverScene.h"
#include "src/UI/Pause/PauseMenu.h"
#include <utility/scene/SceneManager.h>
#include <utility/scene/SceneRegistry.h>

REGISTER_SCENE("GAMEOVER", GameOverScene)

using namespace Hagine;

void GameOverScene::Initialize()
{
	/// ===================================================
	/// 初期化
	/// ===================================================
	BaseScene::Initialize();
	// 床（ゲームシーンと同じ plane）をここで読み込む
	pObjectManager_->LoadAll("GameOverScene");

	// ポーズ画面はどのシーンからでも開けるようにしてある
	PauseMenu::GetInstance()->Initialize();
	PauseMenu::GetInstance()->CloseImmediately();

	// 登場人物と画角。どちらの形態に負けたかはゲームシーンが控えている。
	// 描画の登録より先に作っておく（描画コールバックから触るため）
	staging_ = std::make_unique<GameOverStaging>();
	staging_->Init(GameOverContext::GetInstance()->GetBossForm(), pObjectManager_);
	staging_->RegisterParams();

	// 3Dオブジェクトの描画（ポストエフェクトあり）
	pDrawSystem_->Register("GameOverScene_PreDraw", DrawLayer::PreEffect, [this](const ViewProjection& vp)
		{
			pObjectManager_->Draw(vp);
		});

	// 第1形態の殻（メタボール）をGPUで作り直す。
	// ゲームシーンと同じく、シャドウ・G-Buffer より前のコンピュートフェーズで走らせる
	pDrawSystem_->Register("GameOverScene_MetaBallCompute", DrawSystem::kGPUParticleCompute,
		[this](const ViewProjection&)
		{
			staging_->DispatchCompute();
		});

	// スプライトの描画（ポストエフェクトなし）
	pDrawSystem_->Register("GameOverScene_PostDraw", DrawLayer::PostEffect, [this](const ViewProjection& vp)
		{
			pSpriteManager_->DrawAll();
		});

	// ポーズ画面（スプライトより手前に出したいので後から登録する）
	pDrawSystem_->Register("GameOverScene_PauseMenu", DrawLayer::PostEffect, [](const ViewProjection& vp)
		{
			PauseMenu::GetInstance()->Draw();
		});

}

void GameOverScene::Finalize()
{
	/// ===================================================
	/// 終了処理
	/// ===================================================
	BaseScene::Finalize();
}

void GameOverScene::Update()
{
	/// ===================================================
	/// 更新処理
	/// ===================================================

	// コントローラーのメニュー（START）ボタン、キーボードは ESC で開閉する
	PauseMenu::GetInstance()->Update();

	// ポーズ中はボスの動きも止める（ポーズ画面の裏でうごめかせない）
	if (!PauseMenu::GetInstance()->IsPaused()) {
		staging_->Update();
	}

	CameraUpdate();

}

void GameOverScene::AddSceneSetting() {
	/// ===================================================
	///シーン設定(デバッグ)
	/// ===================================================
	DrawDebugCameraImGui();
	camera_->ShowDebugWindow();

	PauseMenu::GetInstance()->DrawImGui();
}

void GameOverScene::AddObjectSetting()
{
	/// ===================================================
	/// オブジェクト設定（デバッグ）
	/// ===================================================
	if (staging_) {
		staging_->DrawImGui();
	}
}

void GameOverScene::AddParticleSetting()
{
	/// ===================================================
	/// パーティクル設定（デバッグ）
	/// ===================================================
}

void GameOverScene::CameraUpdate()
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

void GameOverScene::ChangeScene() {
	/// ===================================================
	/// シーン切り替え
	/// ===================================================

	//pSceneManager_->NextSceneReservation();
}