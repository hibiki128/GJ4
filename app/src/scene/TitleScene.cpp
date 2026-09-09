#include"TitleScene.h"
#include "MyMath.h"
#include "frame/Frame.h"
#include "src/UI/Pause/PauseMenu.h"
#include <numbers>
#include <utility/scene/SceneManager.h>
#include <utility/scene/SceneRegistry.h>

namespace {

/// <summary>
/// 画面の後処理。ゲーム中と同じものを読む。
/// 輪郭線はこの絵づくりの要なので、タイトルだけ素の絵になると別のゲームに見える
/// </summary>
constexpr const char *kPostEffectDataName = "GameScenePostEffect";

} // namespace

REGISTER_SCENE("TITLE", TitleScene)

using namespace Hagine;

void TitleScene::Initialize()
{
	/// ===================================================
	/// 初期化
	/// ===================================================
	BaseScene::Initialize();
	// 床・空・外周の柱をここで読み込む。
	// 中身はゲームシーンのシーンデータをそのまま複製したものなので、
	// 床の材質も空の色も柱の並びもゲーム中と同じになる
	pObjectManager_->LoadAll("TitleScene");

	// ポーズ画面はどのシーンからでも開けるようにしてある
	PauseMenu::GetInstance()->Initialize();
	PauseMenu::GetInstance()->CloseImmediately();

	// 3Dオブジェクトの描画（ポストエフェクトあり）
	pDrawSystem_->Register("TitleScene_PreDraw", DrawLayer::PreEffect, [this](const ViewProjection& vp)
		{
			pObjectManager_->Draw(vp);
		});

	// ボスの殻（メタボール）をGPUで作り直す。
	// これを回さないと、殻の脈打ちが止まったまま固まって見える
	pDrawSystem_->Register("TitleScene_MetaBallCompute", DrawSystem::kGPUParticleCompute,
		[this](const ViewProjection&)
		{
			if (boss_) {
				boss_->DispatchShellCompute();
			}
		});


	// スプライトの描画（ポストエフェクトなし）
	pDrawSystem_->Register("TitleScene_PostDraw", DrawLayer::PostEffect, [this](const ViewProjection& vp)
		{
			pSpriteManager_->DrawAll();
		});

	// ポーズ画面（スプライトより手前に出したいので後から登録する）
	pDrawSystem_->Register("TitleScene_PauseMenu", DrawLayer::PostEffect, [](const ViewProjection& vp)
		{
			PauseMenu::GetInstance()->Draw();
		});

	/// ===================================================
	/// 画づくり（ゲーム中と同じものをそのまま置く）
	/// ===================================================

	// 戦う場所の外周。名前をゲーム中と同じにしてあるので、広さも線の色も
	// 同じ調整値（ゲームパラメータ > Field）を見る。片方だけ広さが違うことにならない
	field_ = std::make_unique<Field>();
	field_->Init();

	// その外側を囲む飾りの柱。行動範囲の外は床だけで背景が素通しなので、ここで塞ぐ。
	// 柱はこのシーンのオブジェクトとして SceneData/TitleScene/ObjectDatas に保存されており、
	// 上の LoadAll で並んだものをそのまま引き取る（クリア・ゲームオーバー画面と同じ立て付け）
	fieldSurround_ = std::make_unique<FieldSurround>();
	fieldSurround_->Init(field_->GetRadius(), pObjectManager_, "TitleScene");

	// 球体形態のボス。ゲーム中と同じクラスをそのまま置く。
	//
	// 生成直後は登場演出の始まりで、殻の球が周囲へ大きく散らされている。
	// そのまま止めると殻が無いように見えるので、演出の最終状態へそろえてから止める。
	// 止めているあいだも殻の見た目は作り直されるので、脈打ちはそのまま動く
	boss_ = std::make_unique<Boss>();
	boss_->Init("TitleBoss");
	boss_->EndAppear();
	boss_->SetPaused(true);
	pObjectManager_->RegisterExternal(boss_.get());

	// プレイヤー役。揺れの計算はゲーム中と同じ PlayerComponentReaction を使っている
	player_ = std::make_unique<TitlePlayerActor>();
	player_->SetPalette(boss_->GetPalette());
	if (!boss_->GetUsedColors().empty()) {
		player_->SetColorId(boss_->GetUsedColors().front());
	}
	player_->Init("TitlePlayer");
	player_->RegisterParams();
	pObjectManager_->RegisterExternal(player_.get());

	// 構図はデバッグUIから触れるようにしておく
	params_.Register("BossPosition", &bossPosition_, {0.1f});
	params_.Register("PlayerPosition", &playerPosition_, {0.1f});
	params_.Register("CameraPosition", &cameraPosition_, {0.1f});
	params_.Register("CameraRotation", &cameraRotation_, {0.5f});
	params_.Register("CameraFovDegrees", &cameraFovDegrees_, {0.5f, 10.0f, 120.0f});
	params_.Register("BossSpinSpeed", &bossSpinSpeed_, {0.5f, -180.0f, 180.0f});

	ApplyLayout();

	// 画面の後処理（輪郭線）。ゲーム中と同じデータを読む
	pOffScreen_->LoadData(kPostEffectDataName);
}

void TitleScene::Finalize()
{
	/// ===================================================
	/// 終了処理
	/// ===================================================
	BaseScene::Finalize();
}

void TitleScene::Update()
{
	/// ===================================================
	/// 更新処理
	/// ===================================================

	// コントローラーのメニュー（START）ボタン、キーボードは ESC で開閉する
	PauseMenu::GetInstance()->Update();

	// 置き場所は毎フレーム入れ直す。調整UIで動かした値がその場で効くようにするため
	ApplyLayout();

	// 外周の柱の揺れ。オブジェクトの更新より前に置いて、置いた揺れをその場で使わせる
	if (fieldSurround_ && !PauseMenu::GetInstance()->IsPaused()) {
		fieldSurround_->Update();
	}

	// ボスはゆっくり回す。止めていると殻の同じ面ばかり見えて置物に見えるので、
	// 死角の球が少しずつ正面へ回ってくるようにする（ゲーム中の待機と同じ考え方）
	if (boss_) {
		boss_->AddSpin(bossSpinSpeed_ * Frame::DeltaTime());
	}

	// 外周の線を積む（線はフレーム単位で積み直される）
	if (field_) {
		field_->DrawLine();
	}

	CameraUpdate();

}

void TitleScene::ApplyLayout()
{
	if (boss_) {
		// 地面に乗る高さはボスの外周半径から決まる。大きさを変えても浮かない。
		// 接地高さの微調整もゲーム中と同じ値を足して、立ち方をそろえる
		const float groundHeight =
			boss_->GetBodyRadius() + boss_->GetParameters().Shell().groundOffset;
		boss_->SetBossPosition(Vector3{bossPosition_.x, groundHeight, bossPosition_.z});
	}
	if (player_) {
		player_->SetGroundPosition(Vector3{playerPosition_.x, 0.0f, playerPosition_.z});
		// ボスのほうを向かせる（揺れの潰れる向きがそろう）
		player_->LookAt(Vector3{bossPosition_.x, 0.0f, bossPosition_.z});
	}
}

void TitleScene::AddSceneSetting() {
	/// ===================================================
	///シーン設定(デバッグ)
	/// ===================================================
	DrawDebugCameraImGui();
	camera_->ShowDebugWindow();

	PauseMenu::GetInstance()->DrawImGui();
}

void TitleScene::AddObjectSetting()
{
	/// ===================================================
	/// オブジェクト設定（デバッグ）
	/// ===================================================

	if (field_) {
		field_->DrawImGui();
	}

	// その外側を囲む飾りの柱
	if (fieldSurround_) {
		fieldSurround_->DrawImGui();
	}

	if (boss_) {
		// ゲーム中と同じパネル。殻の脈打ち（メタボール）もここから触れる
		boss_->DrawGameplayImGui();
	}
}

void TitleScene::AddParticleSetting()
{
	/// ===================================================
	/// パーティクル設定（デバッグ）
	/// ===================================================
}

void TitleScene::CameraUpdate()
{
	/// ===================================================
	/// カメラ更新
	/// ===================================================

	// 注視点は持たせない。ボスが回っても構図が揺れないよう、位置と向きをそのまま置く
	if (camera_) {
		constexpr float toRadian = std::numbers::pi_v<float> / 180.0f;
		camera_->SetPosition(cameraPosition_);
		camera_->SetRotation(Vector3{cameraRotation_.x * toRadian, cameraRotation_.y * toRadian,
									 cameraRotation_.z * toRadian});
		camera_->SetFovYDegrees(cameraFovDegrees_);
	}

	UpdateDebugCamera();
}

void TitleScene::ChangeScene() {
	/// ===================================================
	/// シーン切り替え
	/// ===================================================

	//pSceneManager_->NextSceneReservation();
}