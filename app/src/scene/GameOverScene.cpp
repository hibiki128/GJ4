#include"GameOverScene.h"
#include "src/UI/Pause/PauseMenu.h"
#include <Frame.h>
#include <utility/scene/SceneManager.h>
#include <utility/scene/SceneRegistry.h>
#include "src/Audio/GameSounds.h"

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

	// 周りを囲む飾りの柱。ゲームシーンと同じ広さ・同じ見た目にして、
	// 「さっきまで戦っていた場所」がそのまま続いているように見せる。
	// 柱はこのシーンのオブジェクトとして SceneData/GameOverScene/ObjectDatas に保存され、
	// 上の LoadAll で並んだものをそのまま引き取る（無ければここで作って保存する）
	fieldSurround_ = std::make_unique<FieldSurround>();
	fieldSurround_->Init(FieldSurround::kDefaultFieldRadius, pObjectManager_, "GameOverScene");

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

	// 見出しと案内。文字は1つずつ傾き方を変えて、そろっていない＝崩れて見えるようにする
	resultUi_ = std::make_unique<ResultUi>();
	resultUi_->Init("GameOver", { "げ", "ー", "む", "お", "ー", "ば", "ー", ".", ".", "." },
		{ "もういちど", "はじめにもどる" }, ResultTitleMotion::Wobble);
	resultUi_->RegisterParams("GameOverUi");

	// スプライトの描画（ポストエフェクトなし）
	pDrawSystem_->Register("GameOverScene_PostDraw", DrawLayer::PostEffect, [this](const ViewProjection& vp)
		{
			pSpriteManager_->DrawAll();
			if (resultUi_) {
				resultUi_->Draw();
			}
		});

	// ポーズ画面（スプライトより手前に出したいので後から登録する）
	pDrawSystem_->Register("GameOverScene_PauseMenu", DrawLayer::PostEffect, [](const ViewProjection& vp)
		{
			PauseMenu::GetInstance()->Draw();
		});

	GameSounds::GetInstance()->Init();
	GameSounds::GetInstance()->StartLoop(GameSounds::Id::BgmGameOver);

}

void GameOverScene::Finalize()
{
	/// ===================================================
	/// 終了処理
	/// ===================================================
	if (resultUi_) {
		resultUi_->Finalize();
	}
	GameSounds::GetInstance()->StopAll();
	BaseScene::Finalize();
}

void GameOverScene::Update()
{
	/// ===================================================
	/// 更新処理
	/// ===================================================

	// コントローラーのメニュー（START）ボタン、キーボードは ESC で開閉する
	PauseMenu::GetInstance()->Update();

	const float deltaTime = Frame::DeltaTime();

	GameSounds::GetInstance()->Update(deltaTime);

	// ポーズ中はボスの動きも止める（ポーズ画面の裏でうごめかせない）
	if (!PauseMenu::GetInstance()->IsPaused()) {
		staging_->Update();
		// 柱の揺れ。オブジェクトの更新より前に置いて、置いた揺れをその場で使わせる
		fieldSurround_->Update();
	}

	// 見出しの動きはポーズ中も進めてよいが、選択と決定だけは拾わせない。
	// ポーズを閉じたAをそのまま決定にしてしまうため
	resultUi_->SetInputEnabled(!PauseMenu::GetInstance()->IsPaused());
	resultUi_->Update(Frame::DeltaTime());
	if (resultUi_->IsDecided() && !isChanging_) {
		isChanging_ = true;
		ChangeScene();
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

	// 周りを囲む飾りの柱
	if (fieldSurround_) {
		fieldSurround_->DrawImGui();
	}

	if (resultUi_) {
		resultUi_->DrawImGui();
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

	// 選ばれた項目で行き先が変わる
	switch (resultUi_->GetDecidedIndex()) {
	case kMenuRetry:
		pSceneManager_->NextSceneReservation("GAME");
		break;
	case kMenuReturnTitle:
	default:
		pSceneManager_->NextSceneReservation("TITLE");
		break;
	}
}