#pragma once
#include "3d/Transform/WorldTransform.h"
#include "Application/Input/GameInput.h"

class PlayerMoveComponent;
class PlayerJumpComponent;

class PlayerContext {
public:
	Hagine::WorldTransform* transform_ = nullptr;
	PlayerMoveComponent* moveComponent_ = nullptr;
	PlayerJumpComponent* jumpComponent_ = nullptr;
	PlayerInput input_;
};

