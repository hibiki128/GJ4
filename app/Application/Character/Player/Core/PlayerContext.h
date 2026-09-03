#pragma once
#include "3d/Transform/WorldTransform.h"
#include "Object/Base/BaseObject.h"
#include "Application/Input/GameInput.h"

class PlayerMoveComponent;
class PlayerJumpComponent;
class PlayerShootComponent;
class PlayerBulletManager;

class PlayerContext {
public:
	Hagine::WorldTransform* transform_ = nullptr;
	PlayerMoveComponent* moveComponent_ = nullptr;
	PlayerJumpComponent* jumpComponent_ = nullptr;
	PlayerShootComponent* shootComponent_ = nullptr;
	PlayerBulletManager* bullets = nullptr;
	PlayerInput input_;

	// 重力と速度は BaseObject のリジッドボディに任せる（Player が自分のものを渡す）
	Hagine::BaseObject::RigidBodyParams* rigidBody_ = nullptr;
	// 床のコライダーに触れている間 true（コライダーの衝突コールバックで更新される）
	bool isOnGround_ = false;
};

