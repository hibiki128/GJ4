#pragma once
#include "3d/Transform/WorldTransform.h"
#include "Object/Base/BaseObject.h"
#include "src/Input/GameInput.h"

class PlayerMoveComponent;
class PlayerJumpComponent;
class PlayerShootComponent;
class PlayerComponentReaction;
class PlayerBulletManager;

class PlayerContext {
public:
	Hagine::WorldTransform* transform_ = nullptr;
	PlayerMoveComponent* moveComponent_ = nullptr;
	PlayerJumpComponent* jumpComponent_ = nullptr;
	PlayerShootComponent* shootComponent_ = nullptr;
	PlayerComponentReaction* reactionComponent_ = nullptr;
	PlayerBulletManager* bullets = nullptr;
	PlayerInput input_;

	// 照準の射線（カメラ基準）。プレイヤーはカメラを知らないので、シーンが毎フレーム入れる。
	// 未設定のままでもプレイヤーの正面へ撃てるよう、既定はワールド前方にしてある
	Hagine::Vector3 aimOrigin_ = {0.0f, 0.0f, 0.0f};
	Hagine::Vector3 aimDirection_ = {0.0f, 0.0f, 1.0f};

	// 重力と速度は BaseObject のリジッドボディに任せる（Player が自分のものを渡す）
	Hagine::BaseObject::RigidBodyParams* rigidBody_ = nullptr;
	// 床のコライダーに触れている間 true（コライダーの衝突コールバックで更新される）
	bool isOnGround_ = false;
};

