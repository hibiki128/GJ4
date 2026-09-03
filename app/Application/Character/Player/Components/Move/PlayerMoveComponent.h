#pragma once
#include "Math/type/Vector2.h"
#include "Math/type/Vector3.h"
#include "Application/Character/Player/Core/PlayerContext.h"

class PlayerMoveComponent {
public:
	void Move(PlayerContext& context, const Hagine::Vector2& input, float moveSpeed);
};

