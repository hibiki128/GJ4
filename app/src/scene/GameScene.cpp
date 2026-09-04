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

	followCamera_->Init();
	followCamera_->SetTarget(player_->GetWorldTransform());

	// 初期化した時点から追従カメラで描画されるようにする
	followCamera_->Activate();

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

	// 第2形態（蜘蛛）。球体形態を倒したあとに出す想定で、今は未出現のまま用意しておく
	bossSpider_ = std::make_unique<BossSpider>();
	bossSpider_->SetPalette(boss_->GetPalette());
	bossSpider_->Init("BossSpider");
	bossSpider_->SetTargetLocator(playerBridge_.get());
	pObjectManager_->RegisterExternal(bossSpider_.get());
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
	// ボス関連のUIはここ（メニューの 表示 > ウィンドウ > オブジェクト設定 (インスペクタ)）へ出す。
	// オブジェクトを選択しなくても触れるよう、固有の項目だけを直接描いている
	if (boss_) {
		boss_->DrawGameplayImGui();
	}
	if (bossSpider_) {
		bossSpider_->DrawGameplayImGui();
	}
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