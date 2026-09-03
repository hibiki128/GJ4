#include "FollowCamera.h"
#include <Input.h>
#include <MyMath.h>
#include <camera/CameraManager.h>
#include <cmath>
#include <data/DataHandler.h>
#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

using namespace Hagine;

void FollowCamera::Init(const std::string& cameraName)
{
	// カメラ本体は CameraManager が所有する（名前で切り替えられるようにするため）
	pCamera_ = CameraManager::GetInstance()->Create(cameraName);
	pCamera_->SetClipRange(0.1f, 1100.0f);

	// 追従パラメータの初期化
	yaw_ = 0.0f;

	// ImGui で調整して保存した値があればそれを使う
	Load();
}

void FollowCamera::Update()
{
	// 追従対象が存在する場合のみ処理
	if (!pCamera_ || !pTarget_)
	{
		return;
	}

	// ユーザー入力によるカメラ回転の更新
	Move();

	// 回転と調整値をカメラへ反映する
	ApplyToCamera();
}

void FollowCamera::Activate()
{
	if (!pCamera_)
	{
		return;
	}

	// 切り替えた瞬間から追従した構図になるように、先に位置を合わせておく
	ApplyToCamera();

	// 以降はこのカメラで描画される
	CameraManager::GetInstance()->SetActive(pCamera_);
}

void FollowCamera::DrawImGui()
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("追従カメラ##followcamera"))
	{
		return; // 折りたたみ中は中身を描かない
	}

	// 別のカメラを見ている状態から戻ってくる用
	if (ImGui::Button("このカメラに切り替え##followactivate"))
	{
		Activate();
	}

	// 追従の見え方を決める値
	ImGui::DragFloat("ターゲットからの距離##followdistance", &distanceFromTarget_, 0.1f, 0.1f, 100.0f, "%.2f");
	ImGui::DragFloat("カメラの高さ##followheight", &heightOffset_, 0.1f, -50.0f, 50.0f, "%.2f");
	ImGui::DragFloat("注視点の高さ##followlookat", &lookAtHeightOffset_, 0.1f, -50.0f, 50.0f, "%.2f");

	// 回転量はラジアンで持っているので、度に直して見せる
	float rotateSpeedDegrees = radiansToDegrees(rotateSpeed_);
	if (ImGui::DragFloat("回転速度(度/フレーム)##followrotatespeed", &rotateSpeedDegrees, 0.05f, 0.0f, 30.0f, "%.2f"))
	{
		rotateSpeed_ = degreesToRadians(rotateSpeedDegrees);
	}

	// 今の向き（左右キーで回した結果）
	ImGui::Text("ヨー角: %.1f 度", radiansToDegrees(yaw_));
	ImGui::SameLine();
	if (ImGui::SmallButton("正面に戻す##followresetyaw"))
	{
		yaw_ = 0.0f;
	}

	if (ImGui::Button("保存##followsave"))
	{
		Save();
	}
	ImGui::SameLine();
	if (ImGui::Button("読み込み##followload"))
	{
		Load();
	}

	// 画角やクリップ距離を触りたいとき用（位置と回転は追従で毎フレーム上書きされる）
	if (pCamera_ && ImGui::TreeNode("カメラ本体の設定##followcamerabody"))
	{
		pCamera_->DrawImGui();
		ImGui::TreePop();
	}
#endif // USE_IMGUI
}

void FollowCamera::Save()
{
	if (!pCamera_)
	{
		return;
	}

	// デストラクタでファイルへ書き出される
	DataHandler data("FollowCamera", pCamera_->GetName());
	data.Save("distanceFromTarget", distanceFromTarget_);
	data.Save("heightOffset", heightOffset_);
	data.Save("lookAtHeightOffset", lookAtHeightOffset_);
	data.Save("rotateSpeed", rotateSpeed_);
}

void FollowCamera::Load()
{
	if (!pCamera_)
	{
		return;
	}

	// キーが無い場合は今の値がそのまま返るので、未保存でも既定値のまま動く
	DataHandler data("FollowCamera", pCamera_->GetName());
	distanceFromTarget_ = data.Load("distanceFromTarget", distanceFromTarget_);
	heightOffset_ = data.Load("heightOffset", heightOffset_);
	lookAtHeightOffset_ = data.Load("lookAtHeightOffset", lookAtHeightOffset_);
	rotateSpeed_ = data.Load("rotateSpeed", rotateSpeed_);
}

void FollowCamera::Move()
{
	// キー入力に応じてヨー角を更新(左右回転)
	if (Input::GetInstance()->PushKey(DIK_LEFT))
	{
		yaw_ -= rotateSpeed_;
	}
	if (Input::GetInstance()->PushKey(DIK_RIGHT))
	{
		yaw_ += rotateSpeed_;
	}
}

void FollowCamera::ApplyToCamera()
{
	if (!pCamera_ || !pTarget_)
	{
		return;
	}

	// ターゲットの位置に基づいて、極座標系からカメラの座標を計算（ヨー角0でターゲットの真後ろ）
	const Vector3 targetPosition = pTarget_->translation_;
	Vector3 cameraPosition;
	cameraPosition.x = targetPosition.x - std::sin(yaw_) * distanceFromTarget_;
	cameraPosition.z = targetPosition.z - std::cos(yaw_) * distanceFromTarget_;
	cameraPosition.y = targetPosition.y + heightOffset_;

	// 注視点はターゲットから高さだけずらせるようにしておく
	Vector3 lookAtPosition = targetPosition;
	lookAtPosition.y += lookAtHeightOffset_;

	// 位置を決めてターゲットを向く（行列の計算はカメラ側が行う）
	pCamera_->SetPosition(cameraPosition);
	pCamera_->SetTarget(lookAtPosition);
}
