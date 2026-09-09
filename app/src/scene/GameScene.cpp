#include "GameScene.h"
#include "src/Boss/Effect/BossParticles.h"
#include "src/Character/Player/Effect/PlayerParticles.h"
#include "debug/imgui/ImGuiNotification.h"
#include <frame/Frame.h>
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
            // 照準レティクルはゲーム画面のすぐ上（仕様書 11.1）。
            // 被弾の赤いマスクより先に描いて、被弾中はレティクルも一緒に赤く染まるようにする。
            // 止まっているとき（ポーズ）と倒れているときは狙いようがないので引っ込める
            if (reticle_ && player_ && !PauseMenu::GetInstance()->IsPaused() && !player_->IsDead()) {
                reticle_->Draw();
            }
            // 被弾の赤いマスクはゲーム画面の上に重ねる。黒帯より先に描いて、
            // 演出の帯やポーズ画面が赤く染まらないようにする
            if (damageVignette_) {
                damageVignette_->Draw();
            }
            // ジャスト回避の白フラッシュも同じ扱い（赤いマスクの上に重ねる）
            if (perfectDodge_) {
                perfectDodge_->Draw();
            }
            // 撃破演出の黒帯は他のUIより手前に出す
            if (defeatDirector_) {
                defeatDirector_->Draw();
            }
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

	// 戦う場所の外周。プレイヤーもボスもここへ配線して、円柱の内側から出ないようにする。
	// 中身はエンジンの円柱コライダー1本で、見た目は線だけ（大きさは ゲームパラメータ > Field）
	field_ = std::make_unique<Field>();
	field_->Init();

	// プレイヤーの生成初期化
	player_ = std::make_unique<Player>();
    player_->Init("Player");
	player_->SetFieldBounds(field_.get());

	followCamera_->Init();
	followCamera_->SetTarget(player_->GetWorldTransform());

	pObjectManager_->RegisterExternal(player_.get());

	// ボスの生成初期化（更新は BaseObjectManager が行う）
	boss_ = std::make_unique<Boss>();
	boss_->Init("Boss");
	boss_->SetFieldBounds(field_.get());
	pObjectManager_->RegisterExternal(boss_.get());

	// 撃つ相手は「変形が終わっていれば蜘蛛、そうでなければ球体形態」。
	// 撃つ側はボスの具象クラスを知らず、この判断はシーンが受け持つ
	auto activeBossTarget = [this]() -> IBossTargetQuery* {
		if (bossSpider_ && bossSpider_->IsBattleReady()) {
			return bossSpider_.get();
		}
		return boss_.get();
		};

	// プレイヤーの見た目をボスと同じ色マスタへ繋ぐ
	player_->SetColorPalette(boss_->GetPalette());

	// プレイヤーの射撃をボスへ繋ぐ（ロックオンも着弾もこの窓口を通る）
	player_->SetBossTargetProvider(activeBossTarget);

	// プレイヤーが最初に選んでいる色を、ボスが使っている色にそろえる
	if (!boss_->GetUsedColors().empty()) {
		// 開始の一瞬だけ既定色のプレイヤーが見えないよう、初期色は補間せず反映する
		player_->SetSelectedColor(boss_->GetUsedColors().front(), true);
	}

	// プレイヤー連携の配線。ボス側は Player の型を知らず、この2つのラムダ越しにだけ触れる
	playerBridge_ = std::make_unique<FunctionalPlayerBridge>(
		[pPlayer = player_.get()] { return pPlayer->GetWorldPosition(); },
		[pPlayer = player_.get()] { return pPlayer->GetSelectedColor(); });
	boss_->SetPlayerBridge(playerBridge_.get());

	// 攻撃の当て先。Player が IDamageable を実装しているので、ボス側は Player の型を
	// 知らないまま（IDamageable* として）ダメージを渡せる
	boss_->SetTargetDamageSink(player_.get());

	// 倒れたプレイヤーは狙わせない（ボスの攻撃は ITargetLocator::IsTargetValid を見ている）
	playerBridge_->SetValidGetter([pPlayer = player_.get()] { return !pPlayer->IsDead(); });

	// 被弾の画面演出。プレイヤーはカメラも画面も知らないので、シーンがここで配る。
	// プレイヤー自身の反動（少し後ろへ押される）は被弾ステートが受け持つ
	damageVignette_ = std::make_unique<DamageVignette>();
	damageVignette_->Init();
	damageVignette_->RegisterParams();

	player_->SetOnDamaged([this](const DamageInfo& info) {
		(void)info;
		followCamera_->AddImpact(1.0f);
		damageVignette_->Play(1.0f);
		});

	// 回避の画面演出。飛び出した瞬間だけカメラを前へ押し出してスピード感を足す。
	// 体の伸び縮みはプレイヤー自身の演出コンポーネントが受け持つ
	player_->SetOnDodge([this](const Vector3& direction) {
		(void)direction;
		followCamera_->AddDashPush(1.0f);
		});

	// ジャスト回避の画面演出。プレイヤーは画面のことを知らないので、被弾と同じくここで配る
	perfectDodge_ = std::make_unique<PerfectDodgeDirector>();
	perfectDodge_->Init();
	perfectDodge_->RegisterParams();

	player_->SetOnPerfectDodge([this](const DamageInfo& info) {
		(void)info;
		perfectDodge_->Play();
		});

	// 照準レティクル。プレイヤーは画面もカメラも知らないので、射線を配るのと同じく
	// 「狙いがどう決まったか」を受け取って、画面座標へ落とすのはシーンの仕事
	reticle_ = std::make_unique<PlayerReticle>();
	reticle_->Init();
	reticle_->RegisterParams();

	// 通知は射撃の更新が終わった直後に来る。シーンの Update から引くと1フレーム古くなり、
	// 弾が飛ぶ先とレティクルの位置がずれてしまう（Player::SetOnAimReport のコメント参照）
	player_->SetOnAimReport([this](const PlayerAimReport& report) {
		reticle_->Update(report, *GetViewProjection(), Frame::DeltaTime());
		});

	// 第2形態（蜘蛛）。球体形態を倒したあとに出す想定で、今は未出現のまま用意しておく
	bossSpider_ = std::make_unique<BossSpider>();
	bossSpider_->SetPalette(boss_->GetPalette());
	bossSpider_->Init("BossSpider");
	bossSpider_->SetTargetLocator(playerBridge_.get());
	bossSpider_->SetFieldBounds(field_.get());

	// 蜘蛛は「当たり判定の中心・半径・ダメージ」を知らせてくるだけで、当てるかどうかは
	// 受け側の仕事（BossSpider::SetHitCallback）。プレイヤーの当たり半径は、球体形態の
	// 攻撃が見ているものと同じ playerBridge_ から引いて、形態で判定がぶれないようにする
	bossSpider_->SetHitCallback([this](const Vector3& center, float radius, float damage) {
		if (!playerBridge_->IsTargetValid()) {
			return;
		}

		const float reach = radius + playerBridge_->GetTargetRadius();
		const Vector3 difference = player_->GetWorldPosition() - center;
		if (difference.LengthSq() > reach * reach) {
			return;
		}

		DamageInfo info{};
		info.amount = damage;
		info.hitPoint = center;
		player_->ApplyDamage(info);
		});

	pObjectManager_->RegisterExternal(bossSpider_.get());

	// ボスまわりの土煙（見た目は Assets/jsons/ParticleCS 以下）。
	// エミッターの発生範囲はボスの大きさに合わせるので、倍率も渡しておく
	BossParticles::GetInstance()->Init();
	BossParticles::GetInstance()->SetMasterScale(boss_->GetParameters().GetMasterScale());

	// プレイヤーの回避で散るゼリー飛沫（同じくエンジンのGPUパーティクル）
	PlayerParticles::GetInstance()->Init();

	// 撃破演出（黒帯とカメラ寄せ）
	defeatDirector_ = std::make_unique<BossDefeatDirector>();
	defeatDirector_->Init();

	// 蜘蛛の脚へも同じ入口（IBossTargetQuery）で弾を当てられるようにする。
	// 撃つ相手の切り替えは activeBossTarget が受け持つので、ここは値をそろえるだけ
	bossSpider_->SetBattleParams(boss_->GetParameters().Chain(), boss_->GetParameters().Effect(),
	                             boss_->GetParameters().LockOn());

	// ボス戦カメラの配線。カメラはボスの具象クラスを知らないので、
	// 「今どの形態が出ているか」の判断は撃つ相手と同じくシーンが受け持つ
	followCamera_->SetFrameTargetProvider([this]() -> CameraFrameTarget {
		CameraFrameTarget frame{};
		if (bossSpider_ && bossSpider_->IsActive()) {
			// 蜘蛛は脚が大きく広がるので、脚の届く範囲を「収めたい大きさ」にする
			frame.position = bossSpider_->GetBodyPosition();
			frame.radius = bossSpider_->GetFootReach();
			frame.valid = true;
		} else if (boss_) {
			frame.position = boss_->GetBossPosition();
			frame.radius = boss_->GetBodyRadius();
			frame.valid = true;
		}
		return frame;
		});

	// カメラ衝突で当てる相手。プレイヤー・ボス・弾に当てたくないので、
	// シーンに置いた地形だけを名前で拾う（壁を足したらこの配列に名前を追加する）
	followCamera_->SetObstacleProvider([this]() {
		static const char* kObstacleNames[] = { "plane" };
		std::vector<BaseObject*> obstacles;
		for (const char* name : kObstacleNames) {
			if (BaseObject* pObject = pObjectManager_->GetObjectByName(name)) {
				obstacles.push_back(pObject);
			}
		}
		return obstacles;
		});

	// ボス戦なので、最初からプレイヤーとボスの両方を画面へ収める構図で始める
	followCamera_->SetMode(CameraMode::BossBattle);

	// 初期化した時点から追従カメラで描画されるようにする（ボスの配線後なので構図が合っている）
	followCamera_->Activate();

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

	// 外周の線はフレーム単位で積み直されるので、ポーズ中でも毎フレーム積む。
	// 止めているあいだも範囲を見ながら大きさを詰められる
	field_->DrawLine();

	// ポーズ中はゲーム側の更新を止める（カメラだけは動かしておく）
	if (PauseMenu::GetInstance()->IsPaused()) {
		CameraUpdate();
		return;
	}

	// ゲーム入力の更新
	gameInput_->UpdateInputState();

	// 被弾の赤いマスクを進める（ポーズ中は止まったままにしたいのでこの位置）
	damageVignette_->Update(Frame::DeltaTime());
	perfectDodge_->Update(Frame::DeltaTime());

	player_->CommandExecute(gameInput_->GetInputContext());

	// 第1形態を倒し切っていたら、そのコアを第2形態へ引き渡す
	UpdateFormChange();

	followCamera_->Update(gameInput_->GetCameraContext());

	// 移動の基準もカメラから作る。視点を回すと、奥へ倒したときに進む向きも一緒に回る
	player_->SetCameraYaw(followCamera_->GetYaw());

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

	// プレイヤーと敵を閉じ込めている円柱の外周
	if (field_) {
		field_->DrawImGui();
	}

	// 調整中に敵が動き回ると見づらいので、まとめて止められるようにしておく。
	// 止めているあいだも描画は続くので、位置や姿勢はそのまま観察できる
	if (ImGui::Checkbox("敵を一時停止", &isBossPaused_)) {
		boss_->SetPaused(isBossPaused_);
		bossSpider_->SetPaused(isBossPaused_);
	}
	if (isBossPaused_) {
		ImGui::SameLine();
		ImGui::TextColored(ImVec4{1.0f, 0.8f, 0.3f, 1.0f}, "停止中");
	}

	// 形態をまたいだ大きさの倍率。球体・蜘蛛・パーティクルへ同じ比率で配る。
	// 倍率そのものは球体形態が1つだけ持っていて、各パラメータには適用済みの値が入る
	ImGui::SeparatorText("ボス全体の大きさ");
	float masterScale = boss_->GetParameters().GetMasterScale();
	if (ImGui::DragFloat("倍率(xyz同時)", &masterScale, 0.01f, 0.1f, 5.0f, "%.2f 倍")) {
		const float ratio = boss_->ApplyMasterScale(masterScale);
		bossSpider_->ScaleSizesBy(ratio);
		BossParticles::GetInstance()->SetMasterScale(boss_->GetParameters().GetMasterScale());
	}
	ImGui::SameLine();
	if (ImGui::Button("等倍に戻す")) {
		const float ratio = boss_->ApplyMasterScale(1.0f);
		bossSpider_->ScaleSizesBy(ratio);
		BossParticles::GetInstance()->SetMasterScale(1.0f);
	}
	ImGui::TextDisabled("殻・球・コア・脚・胴・歩幅・攻撃の届く範囲・土煙の広がり・ひるみの輪が");
	ImGui::TextDisabled("まとめて変わります（時間・角度・速さ・ダメージ・フィールドの広さは据え置き）");
	// 大きさは両形態にまたがるので、保存もここでまとめて押せるようにしておく
	if (ImGui::Button("大きさを両形態とも保存")) {
		boss_->SaveParameters();
		bossSpider_->SaveParameters();
		ImGuiNotification::Post("ボスの大きさを保存しました", {0.2f, 0.8f, 0.2f, 1.0f});
	}
	ImGui::SameLine();
	ImGui::TextDisabled("（大きさ以外の値も一緒に書き出されます）");

	ImGui::Separator();
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
	// ボスの土煙まとめ。中身はエンジンのGPUパーティクルなので、
	// ここで見た目を作って保存すれば Assets/jsons/ParticleCS 以下へ残る
	BossParticles::GetInstance()->DrawImGui();
	PlayerParticles::GetInstance()->DrawImGui();
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

	// プレイヤーのHPが0になった。ダウン演出を挟んでからゲームオーバーへ繋ぐならここ
	// （被弾ステートはプレイヤーを倒れたまま留めるので、遷移の間合いはここで決められる）
	if (player_ && player_->IsDead()) {
		//pSceneManager_->NextSceneReservation("GAMEOVER");
		return;
	}

	// 第2形態の撃破演出が終わる（コアがはじけて消える）と、ここが true になる。
	// 遷移先のシーンが用意できたら pSceneManager_->NextSceneReservation() をここへ足す
	if (bossSpider_ && bossSpider_->IsDefeatFinished()) {
		return;
	}
}