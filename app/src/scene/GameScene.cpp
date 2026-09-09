#include "GameScene.h"
#include "src/Audio/GameSounds.h"
#include "src/Boss/Effect/BossParticles.h"
#include "src/GameOver/GameOverContext.h"
#include "src/Character/Player/Effect/PlayerParticles.h"
#include "src/Item/HealItemManager.h"
#include "debug/imgui/ImGuiNotification.h"
#include "debug/param/GameParamHub.h"
#include "object/Object3dInstancing.h"
#include <frame/Frame.h>
#include "MyMath.h"
#include "src/UI/Pause/PauseMenu.h"
#include <utility/scene/SceneManager.h>
#include <utility/scene/SceneRegistry.h>
#include <cmath>

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
			// ポーズ中は回復エリアを描かない。
			// ポーズからタイトルへ抜けるとき、閉じていく幕の上へエリアの円盤が
			// 抜けて見えてしまうため（円盤はオブジェクトマネージャーに載せず
			// ここから直に描いているので、止めるのもここでよい）
			if (!PauseMenu::GetInstance()->IsPaused()) {
				recoveryZones_.Draw(vp);
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
            // HUD はゲーム画面の上、レティクルより先に描く
            if (hud_) {
                hud_->Draw();
            }
            // 照準レティクルはゲーム画面のすぐ上（仕様書 11.1）。
            // 被弾の赤いマスクより先に描いて、被弾中はレティクルも一緒に赤く染まるようにする
            if (ShouldDrawReticle()) {
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

	// その外側を囲む飾りの柱。行動範囲の外は床だけで背景が素通しなので、ここで塞ぐ。
	// 柱はシーンのオブジェクトとして SceneData/GameScene/ObjectDatas に保存されており、
	// 上の LoadAll で並んだものをそのまま引き取る（無ければここで作って保存する）
	fieldSurround_ = std::make_unique<FieldSurround>();
	fieldSurround_->Init(field_->GetRadius(), pObjectManager_, "GameScene");

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

	// 残弾の回復エリアからプレイヤーへの導線。エリア側は Player の型を知らず、
	// 「この色の回復を早めて」と頼むだけ（IAmmoRecoverySink）
	playerBridge_->SetRegenRequester([pPlayer = player_.get()](Color color, float scale) {
		pPlayer->RequestAmmoRegenScale(color, scale);
		});
	playerBridge_->SetAmmoFullGetter([pPlayer = player_.get()](Color color) {
		return pPlayer->IsAmmoFull(color);
		});

	// 被弾の画面演出。プレイヤーはカメラも画面も知らないので、シーンがここで配る。
	// プレイヤー自身の反動（少し後ろへ押される）は被弾ステートが受け持つ
	damageVignette_ = std::make_unique<DamageVignette>();
	damageVignette_->Init();
	damageVignette_->RegisterParams();

	player_->SetOnDamaged([this](const DamageInfo& info) {
		(void)info;
		followCamera_->AddImpact(1.0f);
		damageVignette_->Play(1.0f);
		GameSounds::GetInstance()->Play(GameSounds::Id::PlayerDamaged);
		if (hud_) {
			hud_->PlayDamaged();
		}
		});

	// 回避の画面演出。飛び出した瞬間だけカメラを前へ押し出してスピード感を足す。
	// 体の伸び縮みはプレイヤー自身の演出コンポーネントが受け持つ
	player_->SetOnDodge([this](const Vector3& direction) {
		(void)direction;
		followCamera_->AddDashPush(1.0f);
GameSounds::GetInstance()->Play(GameSounds::Id::PlayerDodge);
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

	// HUD（体力・ボスの体力・色と残弾）。色はボスと同じ色マスタから引く
	hud_ = std::make_unique<GameHud>();
	hud_->Init(boss_->GetPalette());
	hud_->RegisterParams();
	previousBossHp_ = boss_->GetHp();

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

	// 残弾を回復するエリア。ボスがひと続きの攻撃を終えるたびに1つ生まれる。
	// 調整値はボスデータ（Boss01.json の recoveryZone）に置いてあるので、そこを参照させる
	recoveryZones_.Init("BossRecoveryZone", &boss_->GetMutableParameters().RecoveryZone());
	recoveryZones_.SetTargetLocator(playerBridge_.get());
	recoveryZones_.SetAmmoSink(playerBridge_.get());
	recoveryZones_.SetFieldBounds(field_.get());

	// どちらの形態でも、攻撃をやり切ったところで出す。
	// 出る場所はそのときの本体の位置、色はボスが使っている色から毎回ランダム
	boss_->SetAttackFinishedCallback([this] {
		recoveryZones_.NotifyAttackFinished(boss_->GetBossPosition(), boss_->GetPalette());
		});
	bossSpider_->SetAttackFinishedCallback([this] {
		recoveryZones_.NotifyAttackFinished(bossSpider_->GetBodyPosition(), boss_->GetPalette());
		});

	// 音をまとめて読み込む。BGM はここから鳴らし始めて、シーンを抜けるときに止める
	GameSounds::GetInstance()->Init();
	GameSounds::GetInstance()->StartLoop(GameSounds::Id::Bgm);

	// プレイヤーの回避で散るゼリー飛沫（同じくエンジンのGPUパーティクル）
	PlayerParticles::GetInstance()->Init();

	// 回復アイテム。敵が落とす想定なので、出す側は Spawn(位置) を1行呼ぶだけでよい。
	// アイテムはプレイヤーもボスも知らないので、その配線をここでまとめて行う
	HealItemManager* healItems = HealItemManager::GetInstance();
	healItems->Init("HealItem");

	// 膜の黄色はボスの色マスタから引く。弾の色と同じ元をたどるので、
	// 色を調整しても「膜の黄色と弾の黄色が違う」ことにならない
	healItems->SetSealColor(boss_->GetPalette().GetRgba(Color::YELLOW));

	// 拾い手の位置。倒れているあいだは拾わせない
	healItems->SetPlayerPositionGetter([this](Vector3& out) {
		if (player_->IsDead()) {
			return false;
		}
		out = player_->GetWorldPosition();
		return true;
		});

	// 拾ったときの効果。満タンで効かなければ false が返り、アイテムはその場に残る
	healItems->SetPickupHandler([this] {
		return player_->Heal(HealItemManager::GetInstance()->GetParams().healAmount);
		});
	
	// 膜を「ボス以外の的」として撃つ側へ渡す。着弾・照準・ソフトロックオンのいずれも
	// ボスの球と同じ問い合わせを通るので、狙いを合わせれば強調表示もアシストも効く
	player_->SetItemTargetProvider([]() -> IShootableTargetQuery* {
		return HealItemManager::GetInstance();
		});

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
		// 外周の柱も遮蔽物に含める。含めないとカメラが柱の中まで下がってしまい、
		// 裏面が抜けて背景のクリアカラーが見えてしまう
		if (fieldSurround_) {
			fieldSurround_->AppendObstacles(obstacles);
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

	// HUD が抱えているスプライトを手放す（ハート・チップ・数字など）
	if (hud_) {
		hud_->Finalize();
	}

	// 出したままのアイテムを片付ける。非所有登録はシーンを切り替えても外れないので、
	// ここで捨てないとゲームオーバー画面などにアイテムが残ってしまう
	HealItemManager::GetInstance()->Finalize();
	// ゲームパラメータの登録を外す。
	//
	// GameParamHub はシーンをまたいで生き続け、登録された「変数のアドレス」を
	// そのまま持っている。ここで外さないと、このシーンのプレイヤーや画面演出が
	// 破棄されたあとも解放済みのアドレスを指したままになり、
	// 次のシーンでハブのウィンドウを描いた瞬間に落ちる。
	//
	// ボスやカメラのように GameParamOwner を持っている側は破棄時に自分で外すので、
	// ここに並べるのは「GameParamHub へ直に登録している出所」だけでよい。
	// 出所を増やしたときは、ここへも足すこと（Finalize はメンバの破棄より前に走る）
	static constexpr const char *kDirectParamOwners[] = {
		"Player",
		"Player/Ammo",
		"Player/Color",
		"Player/Damaged",
		"Player/DamageVignette",
		"Player/Dash",
		"Player/Dodge",
		"Player/Health",
		"Player/Idle",
		"Player/Jump",
		"Player/Move",
		"Player/PerfectDodge",
		"Player/Reaction",
		"Player/Shoot",
	};
	// 鳴らし続けている音（BGM・回転・ひるみ）を残したままシーンを抜けない
	GameSounds::GetInstance()->StopAll();

	for (const char *owner : kDirectParamOwners) {
		GameParamHub::GetInstance()->Unregister(owner);
	}

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

	// プレイヤーもボスも、更新はオブジェクトマネージャーが回しているので、
	// シーンが return するだけでは止まらない。入力を入れたままポーズすると滑っていったり、
	// 止まっているあいだに殴られたりするので、止める・再開するは毎フレームここで伝える
	const bool isPaused = PauseMenu::GetInstance()->IsPaused();
	player_->SetPaused(isPaused);
	ApplyBossPause(isPaused);
	// アイテムの更新もオブジェクトマネージャーが回しているので、同じくここで配る。
	// 配らないと、ポーズ中も揺れ続けたうえ寿命が減っていってしまう
	HealItemManager::GetInstance()->SetPaused(isPaused);

	// ポーズ中はゲーム側の更新を止める（カメラだけは動かしておく）
	if (isPaused) {
		CameraUpdate();
		return;
	}

	// ゲーム入力の更新
	gameInput_->UpdateInputState();

	// 外周の柱の揺れ。オブジェクトの更新より前に置いて、置いた揺れをその場で使わせる
	fieldSurround_->Update();

	// 被弾の赤いマスクを進める（ポーズ中は止まったままにしたいのでこの位置）
	damageVignette_->Update(Frame::DeltaTime());
	perfectDodge_->Update(Frame::DeltaTime());

	player_->CommandExecute(gameInput_->GetInputContext());

	// 出ている回復エリアを進める（乗っていれば、その色の回復がここで早くなる）
	GameSounds::GetInstance()->Update(Frame::DeltaTime());

recoveryZones_.Update(Frame::DeltaTime());

	// 第1形態を倒し切っていたら、そのコアを第2形態へ引き渡す
	// 入力を配るより先に呼ぶのは、ムービーが始まったフレームからもう操作を切りたいため
	UpdateFormChange();

	// ムービー中は操作を受け付けない。入力を配るのをやめるのではなく「何も入れていない」ことにして
	// 渡すので、走っている途中でも自然に減速して止まり、アイドルへ戻る。
	// 止めるのは操作だけで、重力も演出も動いたままなので、空中にいれば着地する
	player_->CommandExecute(IsCinematicPlaying() ? PlayerInput{} : gameInput_->GetInputContext());
	// 負けた瞬間に出ていた形態を控えておく。ゲームオーバー画面はこれを見て、
	// どちらの姿で見下ろしてくるかを決める（画面側からボスの中身は覗きにいかない）
	if (player_->IsDead()) {
		GameOverContext::GetInstance()->SetBossForm(
			bossSpider_->IsActive() ? BossFormId::Spider : BossFormId::Sphere);

		// やられたらボスのフレーミングをやめ、震えてはじけるプレイヤーへ寄る。
		// 一度入ったらこのシーンが終わるまで戻さない
		followCamera_->SetMode(CameraMode::Defeat);
	}

	// 倒れた後は視点操作も受け付けない。寄っていく構図を操作で崩されないようにする
	//（プレイヤーの操作を切るのと同じ考え方で、「何も入れていない」ことにして渡す）
	followCamera_->Update(player_->IsDead() ? CameraInput{} : gameInput_->GetCameraContext());

	// 移動の基準もカメラから作る。視点を回すと、奥へ倒したときに進む向きも一緒に回る
	player_->SetCameraYaw(followCamera_->GetYaw());

	CameraUpdate();

	// 射線はカメラから作る。カメラを動かした後に配り直すので、
	// プレイヤーは「いま見ている向き」へ撃てる
	UpdateAim();

	UpdateHud();

	// 勝ち負けがついていれば、間を置いて結果画面へ送る。
	// ポーズ中はここまで来ないので、止めているあいだに遷移が進むことはない
	ChangeScene();
}

void GameScene::ApplyBossPause(bool paused)
{
	/// ===================================================
	/// 敵の更新を止めるかを配る
	/// ===================================================

	// ポーズとデバッグの一時停止は別々の理由なので、どちらか一方でも立っていれば止める
	const bool stop = paused || isBossPaused_;

	if (boss_) {
		boss_->SetPaused(stop);
	}
	if (bossSpider_) {
		bossSpider_->SetPaused(stop);
	}
}

bool GameScene::IsCinematicPlaying() const
{
	/// ===================================================
	/// ムービー中か（黒帯が出ているあいだ）
	/// ===================================================

	// 演出の始まりで黒帯が出て（Begin）、カメラをプレイヤーへ返し終えたところで下りる（Stop）。
	// カメラが戻っている最中も「まだムービー」として扱うので、
	// 構図が戻りきる前に動き出したり狙えたりはしない
	return defeatDirector_ && defeatDirector_->IsActive();
}

bool GameScene::ShouldDrawReticle() const
{
	/// ===================================================
	/// レティクルを出してよい場面か
	/// ===================================================

	if (!reticle_ || !player_) {
		return false;
	}

	// 止まっているとき・ムービー中・倒れているときは狙いようがないので引っ込める
	return !PauseMenu::GetInstance()->IsPaused() && !IsCinematicPlaying() && !player_->IsDead();
}

void GameScene::UpdateHud()
{
	/// ===================================================
	/// HUD へ今フレームの値を渡す
	/// ===================================================
	if (!hud_) {
		return;
	}

	GameHudSnapshot snapshot{};
	snapshot.hp = player_->GetHealth().GetHp();
	snapshot.maxHp = player_->GetHealth().GetMaxHp();
	snapshot.playerColor = player_->GetSelectedColor();
	// 弾は色ごとに別なので、いま選んでいる色のぶんを出す
	snapshot.ammo = player_->GetAmmo().GetAmmo(snapshot.playerColor);
	snapshot.maxAmmo = player_->GetAmmo().GetMaxAmmo();

	// 回復エリアに乗っているあいだは、そのエリアの色と残弾も出す。
	// エリアの色は選んでいる色とは限らないので、これが無いと戻っているのかが分からない
	Color reloadColor = snapshot.playerColor;
	if (recoveryZones_.TryGetOccupiedColor(reloadColor)) {
		snapshot.reloadActive = true;
		snapshot.reloadColor = reloadColor;
		snapshot.reloadAmmo = player_->GetAmmo().GetAmmo(reloadColor);
	}

	// ボスの体力は「まとっている球の数」。撃った球がくっついたときは数が増えるので、
	// バーもそのぶん伸びる。第2形態は脚が球のかたまりなので、脚の本数で見る
	if (bossSpider_ && bossSpider_->IsActive()) {
		snapshot.bossHp = static_cast<float>(bossSpider_->GetAliveLegCount());
		snapshot.bossMaxHp = static_cast<float>(bossSpider_->GetParameters().legCount);
		snapshot.bossVisible = true;
	} else {
		snapshot.bossHp = boss_->GetHp();
		snapshot.bossMaxHp = boss_->GetMaxHp();
		snapshot.bossVisible = boss_->IsFormVisible();
	}

	// 球が減った瞬間だけバーを光らせる（くっついて増えたときは光らせない）
	if (snapshot.bossHp < previousBossHp_ - 0.5f) {
		hud_->PlayBossDamaged();
	}
	previousBossHp_ = snapshot.bossHp;

	if (gameInput_->GetInputContext().attack) {
		hud_->PlayShot();
	}

	hud_->Update(Frame::DeltaTime(), snapshot);
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

	// その外側を囲む飾りの柱
	if (fieldSurround_) {
		fieldSurround_->DrawImGui();
	}

	// 回復アイテム（敵が落とす導線ができるまでの確認用）
	DrawHealItemImGui();

	// 調整中に敵が動き回ると見づらいので、まとめて止められるようにしておく。
	// 止めているあいだも描画は続くので、位置や姿勢はそのまま観察できる
	if (ImGui::Checkbox("敵を一時停止", &isBossPaused_)) {
		// 押した瞬間にも効かせる。ゲームを止めているあいだは Update が回らないので、
		// 毎フレームの配り直しだけに任せると、止めた状態では切り替えられなくなる
		ApplyBossPause(PauseMenu::GetInstance()->IsPaused());
	}
	if (isBossPaused_) {
		ImGui::SameLine();
		ImGui::TextColored(ImVec4{1.0f, 0.8f, 0.3f, 1.0f}, "停止中");
	}

	// 弾を撃つとボス以外がちらつくときの切り分け用。
	// インスタンシングは全オブジェクトぶんのインスタンスを1本のアップロードバッファへ
	// 毎フレーム書き込むが、エンジンは2フレーム同時進行なので、前フレームの描画が
	// まだ読んでいる領域を書き換えてしまう。弾のように数が毎フレーム変わるものがあると
	// 書き込み位置がずれて、前フレームぶんの絵が化ける。
	// これを切ると1体ずつの描画に戻るので、ちらつきが消えれば原因はここだと分かる
	{
		bool instancing = Object3dInstancing::GetInstance()->IsEnabled();
		if (ImGui::Checkbox("インスタンシングを使う", &instancing)) {
			Object3dInstancing::GetInstance()->SetEnabled(instancing);
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("切ると全オブジェクトを1体ずつ描きます（描画コールは増えます）。\n"
				"射撃中のちらつきがこれで止まるなら、原因はインスタンスバッファの\n"
				"フレーム間の競合です");
		}
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
	ImGui::SameLine();
	if (ImGui::Button("いまの大きさを1倍にする")) {
		// 値は1つも変えず、倍率の基準だけを取り直す。
		// 倍率が1以外のときに個別の大きさを手で書き換えると、その値まで倍率ぶん割られてしまう。
		// 見えている大きさが正しいときは、ここを押して基準をそろえ直す
		boss_->RebaseMasterScale();
		BossParticles::GetInstance()->SetMasterScale(1.0f);
		ImGuiNotification::Post("いまの大きさを1倍として基準を取り直しました",
			{0.4f, 0.8f, 1.0f, 1.0f});
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("「等倍に戻す」は保存されている倍率で割り戻すので、\n"
			"倍率が1以外のときに個別の大きさをいじっていると小さくなりすぎます。\n"
			"いま見えている大きさが正しいなら、こちらで基準を取り直してください\n"
			"（大きさは変わらず、倍率の表示だけが 1.00 になります）");
	}

	// 上の落とし穴に気づけるよう、1倍でないときは個別調整を控えるよう出しておく
	if (std::abs(masterScale - 1.0f) > 0.001f) {
		ImGui::TextColored(ImVec4{1.0f, 0.8f, 0.3f, 1.0f},
			"倍率が %.2f 倍です。殻や脚の大きさを個別に触るのは 1.00 倍のときに",
			masterScale);
	}
	ImGui::TextDisabled("殻・球・コア・脚・胴・歩幅・攻撃の届く範囲・土煙の広がり・ひるみの輪が");
	ImGui::TextDisabled("まとめて変わります（時間・角度・速さ・ダメージ・フィールドの広さは据え置き）");

	// 攻撃範囲を倍率の対象へ入れる前に保存したデータは、体だけ大きくて範囲が置いていかれている。
	// 差分方式なのでスライダーを動かしても食い違いは埋まらないため、1回だけ掛け直す口を用意する
	if (ImGui::Button("攻撃範囲だけ今の倍率へそろえ直す")) {
		const float scale = boss_->GetParameters().GetMasterScale();
		boss_->ApplyMasterScaleToAttackRanges();
		bossSpider_->ScaleAttackRangesBy(scale);
		ImGuiNotification::Post("攻撃範囲を " + std::to_string(scale) + " 倍へそろえました",
			{0.4f, 0.8f, 1.0f, 1.0f});
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("倍率が攻撃範囲に効くようになる前の値を保存していた場合の埋め合わせです。\n"
			"「体は大きいのに攻撃範囲だけ元のまま」のときに1回だけ押してください。\n"
			"押すたびに掛かるので、続けて押さないこと");
	}
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
void GameScene::DrawHealItemImGui()
{
	/// ===================================================
	/// 回復アイテム（デバッグ）
	/// ===================================================

	HealItemManager* healItems = HealItemManager::GetInstance();

	ImGui::SeparatorText("回復アイテム");
	ImGui::Text("出ている数: %zu", healItems->GetActiveCount());

	// 敵が落とす導線はまだ無いので、ここから出して確かめられるようにしておく。
	// 出るのは膜に包まれた状態なので、黄色い弾を当てないと拾えない
	if (ImGui::Button("プレイヤーの前に出す")) {
		// カメラの向き（＝プレイヤーが見ている向き）の先へ置く。
		// 足元に出すと膜に埋もれて狙えないので、少し離す
		const float yaw = followCamera_->GetYaw();
		const Vector3 forward{std::sin(yaw), 0.0f, std::cos(yaw)};
		if (!healItems->Spawn(player_->GetWorldPosition() + forward * healItemSpawnDistance_)) {
			ImGuiNotification::Post("回復アイテムの空きがありません", {0.9f, 0.7f, 0.2f, 1.0f});
		}
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragFloat("出す距離", &healItemSpawnDistance_, 0.5f, 1.0f, 40.0f, "%.1f");

	HealItemParams& itemParams = healItems->GetParams();
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragInt("膜を割るのに必要な弾数", &itemParams.sealHitPoints, 0.1f, 1, 20);
	ImGui::SetItemTooltip("次に出すぶんから効きます（出ている膜の固さは変わりません）");

	// 見た目の調整。出ているアイテムにもその場で効く
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragFloat("膜の半径", &itemParams.sealRadius, 0.05f, 0.2f, 6.0f, "%.2f");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragFloat("ハートの大きさ", &itemParams.coreRadius, 0.05f, 0.1f, 4.0f, "%.2f");
	ImGui::ColorEdit4("膜の色", &itemParams.sealRgba.x, ImGuiColorEditFlags_AlphaBar);
	ImGui::ColorEdit4("ハートの色", &itemParams.coreRgba.x);

	ImGui::TextDisabled("黄色い弾を当てるたび膜が薄くなり、割れると中のハートを拾えます（拾うと %d 回復）",
	                    itemParams.healAmount);
}

void GameScene::AddParticleSetting()
{
	/// ===================================================
	/// パーティクル設定（デバッグ）
	/// ===================================================
	// ボスの土煙まとめ。中身はエンジンのGPUパーティクルなので、
	// ここで見た目を作って保存すれば Assets/jsons/ParticleCS 以下へ残る
	BossParticles::GetInstance()->DrawImGui();

	// 回復エリアの粒もここに並ぶので、置き方の調整は同じ窓でできる。
	// 保存先はボスデータなので、書き出しはボスに頼む
	GameSounds::GetInstance()->DrawImGui();

recoveryZones_.DrawImGui(boss_->GetBossPosition(), boss_->GetPalette(), [this] {
		boss_->SaveParameters();
		ImGuiNotification::Post("回復エリアの設定を保存しました", {0.2f, 0.8f, 0.2f, 1.0f});
		});
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

	// 予約済み。切り替わるまでは何もしない
	if (sceneChangeRequested_) {
		return;
	}

	// まだ決着していないので、勝ち負けがついていないかだけ見て抜ける
	if (nextSceneName_.empty()) {
		// プレイヤーが震えてはじけ飛ぶところまで終わった。
		// 倒れた瞬間（IsDead）ではなくここを見るので、演出は必ず最後まで流れる
		if (player_ && player_->IsDefeatFinished()) {
			nextSceneName_ = "GAMEOVER";
			sceneChangeWait_ = kGameOverWait;
			return;
		}

		// 第2形態の撃破演出が終わった（コアがはじけて消えた）。
		// 第1形態を倒しただけでは第2形態へ変形するだけなので、ここは通らない
		if (bossSpider_ && bossSpider_->IsDefeatFinished()) {
			nextSceneName_ = "CLEAR";
			sceneChangeWait_ = kClearWait;
		}
		return;
	}

	// 決着はついている。間を置いてから送る
	sceneChangeWait_ -= Frame::DeltaTime();
	if (sceneChangeWait_ > 0.0f) {
		return;
	}

	sceneChangeRequested_ = true;
	pSceneManager_->NextSceneReservation(nextSceneName_);
}