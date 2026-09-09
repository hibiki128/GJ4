#include"ClearScene.h"
#include "src/Field/FieldParticles.h"
#include "src/UI/Pause/PauseMenu.h"
#include <Frame.h>
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

	// 周りを囲む飾りの柱。ゲームシーンと同じ広さ・同じ見た目にして、
	// 「さっきまで戦っていた場所」がそのまま続いているように見せる。
	// 柱はこのシーンのオブジェクトとして SceneData/ClearScene/ObjectDatas に保存され、
	// 上の LoadAll で並んだものをそのまま引き取る（無ければここで作って保存する）
	fieldSurround_ = std::make_unique<FieldSurround>();
	fieldSurround_->Init(FieldSurround::kDefaultFieldRadius, pObjectManager_, "ClearScene");

	// フィールドに漂う粒。ゲームシーンと同じものを同じ場所へ出して、
	// 戦っていた場所がそのまま続いているように見せる
	FieldParticles::Spawn();

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

	// 見出しと案内。見出しは1文字ずつ動かすので、まとめて ResultUi が受け持つ。
	// 文字はゲームの4色で塗る（まわりの柱と同じ色づかいにそろえる）
	resultUi_ = std::make_unique<ResultUi>();
	resultUi_->Init("Clear", {"く", "り", "あ", "！", "！"}, {"はじめにもどる"},
	                ResultTitleMotion::Bounce);
	resultUi_->RegisterParams("ClearUi");

	// スプライトの描画（ポストエフェクトなし）
	pDrawSystem_->Register("ClearScene_PostDraw", DrawLayer::PostEffect, [this](const ViewProjection& vp)
		{
			pSpriteManager_->DrawAll();
			if (resultUi_) {
				resultUi_->Draw();
			}
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
	if (resultUi_) {
		resultUi_->Finalize();
	}
	BaseScene::Finalize();
}

void ClearScene::Update()
{
	/// ===================================================
	/// 更新処理
	/// ===================================================

	// コントローラーのメニュー（START）ボタン、キーボードは ESC で開閉する
	PauseMenu::GetInstance()->Update();

	// 柱の揺れ。オブジェクトの更新より前に置いて、置いた揺れをその場で使わせる
	if (!PauseMenu::GetInstance()->IsPaused()) {
		fieldSurround_->Update();
	}

	// 見出しの動きはポーズ中も進めてよい（ポーズ画面の下でひっそり動いているだけ）が、
	// 決定だけは拾わせない。ポーズを閉じたAをそのまま決定にしてしまうため
	resultUi_->SetInputEnabled(!PauseMenu::GetInstance()->IsPaused());
	resultUi_->Update(Frame::DeltaTime());
	if (resultUi_->IsDecided() && !isChanging_) {
		isChanging_ = true;
		ChangeScene();
	}

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

	// 周りを囲む飾りの柱
	if (fieldSurround_) {
		fieldSurround_->DrawImGui();
	}

	if (resultUi_) {
		resultUi_->DrawImGui();
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

	// 案内は「はじめにもどる」の1つだけなので、行き先も1つ
	pSceneManager_->NextSceneReservation("TITLE");
}