#pragma once
#include "3d/Transform/WorldTransform.h"
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
};

