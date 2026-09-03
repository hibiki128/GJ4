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

	// 3Dオブジェクトの描画（ポストエフェクトあり）
	pDrawSystem_->Register("GameScene_PreDraw", DrawLayer::PreEffect, [this](const ViewProjection& vp)
		{
			pObjectManager_->Draw(vp);
			// デバッグ射撃の弾はマネージャに登録していないのでここで描く
			if (bossTestDriver_) {
				bossTestDriver_->Draw(vp);
			}
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

	pObjectManager_->RegisterExternal(player_.get());

	// ボスの生成初期化（更新は BaseObjectManager が行う）
	boss_ = std::make_unique<Boss>();
	boss_->Init("Boss");
	pObjectManager_->RegisterExternal(boss_.get());

	// 連鎖マッチ検証用のデバッグ射撃
	bossTestDriver_ = std::make_unique<BossTestDriver>();
	bossTestDriver_->Init(boss_.get());

	// プレイヤー連携の配線。ボス側は Player の型を知らず、この2つのラムダ越しにだけ触れる。
	// プレイヤーに色の取得APIが実装されたら、2つ目のラムダを差し替えるだけで本接続になる
	playerBridge_ = std::make_unique<FunctionalPlayerBridge>(
		[pPlayer = player_.get()] { return pPlayer->GetWorldPosition(); },
		[pDriver = bossTestDriver_.get()] { return pDriver->GetSelectedColor(); });
	boss_->SetPlayerBridge(playerBridge_.get());
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

	// ボス検証用のデバッグ射撃（ボス本体の更新は BaseObjectManager が行う）
	bossTestDriver_->Update(*GetViewProjection());

	CameraUpdate();

}

void GameScene::AddSceneSetting() {
	/// ===================================================
	///シーン設定(デバッグ)
	/// ===================================================
	DrawDebugCameraImGui();
	camera_->ShowDebugWindow();
}

void GameScene::AddObjectSetting()
{
	/// ===================================================
	/// オブジェクト設定（デバッグ）
	/// ===================================================
	if (bossTestDriver_) {
		bossTestDriver_->DrawImGui();
	}
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