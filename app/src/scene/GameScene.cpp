#include "GameScene.h"
#include "MyMath.h"
#include "src/UI/Pause/PauseMenu.h"
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

	// ポーズ画面はどのシーンからでも開けるようにしてある
	PauseMenu::GetInstance()->Initialize();
	PauseMenu::GetInstance()->CloseImmediately();

	followCamera_ = std::make_unique<FollowCamera>();

	// 3Dオブジェクトの描画（ポストエフェクトあり）
	pDrawSystem_->Register("GameScene_PreDraw", DrawLayer::PreEffect, [this](const ViewProjection& vp)
		{
			pObjectManager_->Draw(vp);
		});

	// ボスの殻（メタボール）をGPUで作り直す。
	// シャドウ・G-Buffer より前のコンピュートフェーズで走らせ、描画側は完了を待ってから使う
	pDrawSystem_->Register("GameScene_MetaBallCompute", DrawSystem::kGPUParticleCompute,
		[this](const ViewProjection&)
		{
			if (boss_) {
				boss_->DispatchShellCompute();
			}
		});

    // スプライトの描画（ポストエフェクトなし）
    pDrawSystem_->Register("GameScene_PostDraw", DrawLayer::PostEffect, [this](const ViewProjection& vp)
        {
            pSpriteManager_->DrawAll();
        });

    // ポーズ画面（スプライトより手前に出したいので後から登録する）
    pDrawSystem_->Register("GameScene_PauseMenu", DrawLayer::PostEffect, [](const ViewProjection& vp)
        {
            PauseMenu::GetInstance()->Draw();
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

	// 撃つ相手は「変形が終わっていれば蜘蛛、そうでなければ球体形態」。
	// 撃つ側はボスの具象クラスを知らず、この判断はシーンが受け持つ
	auto activeBossTarget = [this]() -> IBossTargetQuery* {
		if (bossSpider_ && bossSpider_->IsBattleReady()) {
			return bossSpider_.get();
		}
		return boss_.get();
		};

	// プレイヤーの射撃をボスへ繋ぐ（ロックオンも着弾もこの窓口を通る）
	player_->SetBossTargetProvider(activeBossTarget);

	// プレイヤーが最初に選んでいる色を、ボスが使っている色にそろえる
	if (!boss_->GetUsedColors().empty()) {
		player_->SetSelectedColor(boss_->GetUsedColors().front());
	}

	// プレイヤー連携の配線。ボス側は Player の型を知らず、この2つのラムダ越しにだけ触れる
	playerBridge_ = std::make_unique<FunctionalPlayerBridge>(
		[pPlayer = player_.get()] { return pPlayer->GetWorldPosition(); },
		[pPlayer = player_.get()] { return pPlayer->GetSelectedColor(); });
	boss_->SetPlayerBridge(playerBridge_.get());

	// 第2形態（蜘蛛）。球体形態を倒したあとに出す想定で、今は未出現のまま用意しておく
	bossSpider_ = std::make_unique<BossSpider>();
	bossSpider_->SetPalette(boss_->GetPalette());
	bossSpider_->Init("BossSpider");
	bossSpider_->SetTargetLocator(playerBridge_.get());
	pObjectManager_->RegisterExternal(bossSpider_.get());
	// 蜘蛛にも球体形態と同じ連鎖・演出・ロックオンの設定をそろえる
	bossSpider_->SetBattleParams(boss_->GetParameters().Chain(), boss_->GetParameters().Effect(),
		boss_->GetParameters().LockOn());

	pOffScreen_->LoadData("GameScenePostEffect");
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

	// コントローラーのメニュー（START）ボタン、キーボードは ESC で開閉する
	PauseMenu::GetInstance()->Update();

	// ポーズ中はゲーム側の更新を止める（カメラだけは動かしておく）
	if (PauseMenu::GetInstance()->IsPaused()) {
		CameraUpdate();
		return;
	}

	// ゲーム入力の更新
	gameInput_->UpdateInputState();

	player_->CommandExecute(gameInput_->GetInputContext());

	// 第1形態を倒し切っていたら、そのコアを第2形態へ引き渡す
	UpdateFormChange();

	followCamera_->Update();

	CameraUpdate();

	// 射線はカメラから作る。カメラを動かした後に配り直すので、
	// プレイヤーは「いま見ている向き」へ撃てる
	UpdateAim();
}

void GameScene::UpdateAim()
{
	/// ===================================================
	/// 照準（カメラの射線）をプレイヤーへ配る
	/// ===================================================

	// プレイヤーはカメラを知らないので、シーンが毎フレーム射線を渡す。
	// 起点をカメラから少し前に出すのは、弾がカメラの手前で当たらないようにするため
	const ViewProjection& viewProjection = *GetViewProjection();
	const Matrix4x4 rotateMatrix = MakeRotateXYZMatrix(viewProjection.eulerRotation_);
	const Vector3 aimDirection = TransformNormal(kWorldForward, rotateMatrix).Normalize();
	const Vector3 aimOrigin = viewProjection.translation_ + aimDirection * 1.0f;

	player_->SetAim(aimOrigin, aimDirection);
}

void GameScene::UpdateFormChange()
{
	/// ===================================================
	/// 第1形態（球体）→ 第2形態（蜘蛛）への引き継ぎ
	/// ===================================================

	// 引き継ぐ元のコアの位置・大きさを毎フレーム教えておく。
	// デバッグの「変形を再生」でも、本番と同じ場所・同じ大きさから変形が始まる
	bossSpider_->SetCoreHandoff(boss_->GetCorePosition(), boss_->GetCoreRadius());

	// 色付きの球をすべて壊し、消滅演出も終わったらコアを渡す
	if (!boss_->IsCoreHandedOver() && boss_->IsShellCleared()) {
		bossSpider_->Awaken(boss_->GetCorePosition(), boss_->GetCoreRadius());
		boss_->HandOverCore();
	}

	// コアは1つしかないので、描くのはどちらか片方だけ。
	// 蜘蛛が出ているあいだは球体形態を丸ごと消す（デバッグで出したときも同じ）。
	// 両方描くと同じ場所に黒い球が2つ重なってちらつく
	boss_->SetFormVisible(!bossSpider_->IsActive());
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

	PauseMenu::GetInstance()->DrawImGui();
}

void GameScene::AddObjectSetting()
{
	/// ===================================================
	/// オブジェクト設定（デバッグ）
	/// ===================================================
	// ゲームプレイ関連のUIはここ（メニューの 表示 > ウィンドウ > オブジェクト設定 (インスペクタ)）へ出す。
	// オブジェクトを選択しなくても触れるよう、固有の項目だけを直接描いている
	if (player_) {
		player_->DrawGameplayImGui();
	}
	if (boss_) {
		boss_->DrawGameplayImGui();
	}
	if (bossSpider_) {
		bossSpider_->DrawGameplayImGui();
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