#pragma once
#include"camera/Camera.h"
#include <string>
#include <transform/WorldTransform.h>

/// <summary>
/// 基本追従カメラクラス
/// ターゲットを追従するカメラの基底機能を提供する
/// カメラ本体（位置・向き・行列）は Camera が持ち、このクラスは追従のしかただけを決める
/// </summary>
class FollowCamera
{
public:
	// ===================================================
	// 公開メソッド
	// ===================================================

	/// <summary>
	/// 初期化（カメラを CameraManager へ登録する）
	/// 保存済みの調整値があれば読み込む
	/// </summary>
	/// <param name="cameraName">登録するカメラ名</param>
	void Init(const std::string& cameraName = "追従カメラ");

	/// <summary>
	/// 更新処理
	/// </summary>
	void Update();

	/// <summary>
	/// このカメラを描画に使うカメラにする
	/// 追従位置へ移動させてから切り替えるので、切り替えた瞬間から構図が合っている
	/// </summary>
	void Activate();

	/// <summary>
	/// ImGuiによるデバッグ表示
	/// </summary>
	void DrawImGui();

	/// <summary>
	/// 追従の調整値を保存する（Assets/jsons/FollowCamera/カメラ名.json）
	/// </summary>
	void Save();

	/// <summary>
	/// 保存済みの追従の調整値を読み込む（無ければ既定値のまま）
	/// </summary>
	void Load();

	/// <summary>
	/// ヨー角を取得
	/// </summary>
	float GetYaw() { return yaw_; }

	/// <summary>
	/// カメラを取得（所有は CameraManager）
	/// </summary>
	Hagine::Camera* GetCamera() const { return pCamera_; }

	/// <summary>
	/// 描画へ渡すビュープロジェクションを取得
	/// </summary>
	Hagine::ViewProjection& GetViewProjection() { return pCamera_->GetViewProjection(); }

	/// <summary>
	/// 追従対象を設定
	/// </summary>
	void SetTarget(const Hagine::WorldTransform* pTarget) { pTarget_ = pTarget; }

private:
	// ===================================================
	// 非公開メソッド
	// ===================================================

	/// <summary>
	/// カメラの移動計算
	/// </summary>
	void Move();

	/// <summary>
	/// 追従の調整値からカメラの位置と注視点を決める
	/// </summary>
	void ApplyToCamera();

private:
	// ===================================================
	// メンバ変数
	// ===================================================

	Hagine::Camera* pCamera_ = nullptr;               // カメラ本体（所有は CameraManager）
	const Hagine::WorldTransform* pTarget_ = nullptr; // 追従対象のワールド変換
	float yaw_ = 0.0f;                        // ヨー角(左右回転)

	// ここから下は ImGui で調整する値（Save / Load の対象）
	float distanceFromTarget_ = 7.0f;         // ターゲットから後ろへ下がる距離
	float heightOffset_ = 1.5f;               // カメラの高さ（ターゲットからの差）
	float lookAtHeightOffset_ = 0.0f;         // 注視点の高さ（ターゲットからの差）
	float rotateSpeed_ = 0.04f;               // 1フレームあたりの回転量(ラジアン)
};
