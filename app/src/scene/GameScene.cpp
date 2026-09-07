#include "GameScene.h"
#include <frame/Frame.h>
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
            // 撃破演出の黒帯は他のUIより手前に出す
            if (defeatDirector_) {
                defeatDirector_->Draw();
            }
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
	// 撃破演出（黒帯とカメラ寄せ）
	defeatDirector_ = std::make_unique<BossDefeatDirector>();
	defeatDirector_->Init();

	// 蜘蛛の脚へも同じ入口（IBossTargetQuery）で弾を当てられるようにする
	bossTestDriver_->SetSpider(bossSpider_.get());
	bossSpider_->SetBattleParams(boss_->GetParameters().Chain(), boss_->GetParameters().Effect());
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

	// 第1形態を倒し切っていたら、そのコアを第2形態へ引き渡す
	UpdateFormChange();

	
	followCamera_->Update();

	CameraUpdate();

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

	UpdateDefeatDirection();
}

void GameScene::UpdateDefeatDirection()
{
	/// ===================================================
	/// 第2形態の演出（登場・撃破）で使う、黒帯とカメラ寄せ
	/// ===================================================

	const float deltaTime = Frame::DeltaTime();
	const BossSpiderDefeatParams &params = bossSpider_->GetParameters().defeat;
	const Vector3 corePosition = bossSpider_->GetBodyPosition();

	// カメラをプレイヤーへ戻している最中
	if (defeatDirector_->IsReturning()) {
		if (defeatDirector_->UpdateReturn(deltaTime, followCamera_->GetViewProjection().translation_,
		                                  player_->GetWorldPosition(), params)) {
			followCamera_->Activate();
		}
		return;
	}

	// 撃破演出：最後まで出しっぱなし（この先はシーン遷移へ繋ぐ）
	if (bossSpider_->IsDefeated()) {
		if (!defeatDirector_->IsActive()) {
			defeatDirector_->Begin(corePosition, followCamera_->GetViewProjection().translation_);
		}
		defeatDirector_->Update(deltaTime, corePosition, bossSpider_->GetBodyYaw(), params);
		return;
	}

	// 登場演出：崩れ落ちて起き上がるあいだだけ。カメラは座標を動かさず、
	// 落ちたコアの高さに構えたまま、浮き上がるコアを見上げる
	if (bossSpider_->IsIntroCinematic()) {
		if (!defeatDirector_->IsActive()) {
			const BossSpiderParams &spider = bossSpider_->GetParameters();
			defeatDirector_->Begin(corePosition, followCamera_->GetViewProjection().translation_, true,
			                       spider.introCameraDistance, spider.introCameraHeight);
		}
		defeatDirector_->Update(deltaTime, corePosition, bossSpider_->GetBodyYaw(), params);
		return;
	}

	// 演出が終わった（変形しきった・デバッグで戻した）ので、カメラを返しにいく
	if (defeatDirector_->IsActive()) {
		defeatDirector_->BeginReturn();
	}
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

	// 第2形態の撃破演出が終わる（コアがはじけて消える）と、ここが true になる。
	// 遷移先のシーンが用意できたら pSceneManager_->NextSceneReservation() をここへ足す
	if (bossSpider_ && bossSpider_->IsDefeatFinished()) {
		return;
	}
}