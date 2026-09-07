#pragma once
#include "Math/type/Vector3.h"
#include "src/Character/Player/Core/PlayerContext.h"

class PlayerComponentReaction {
public:
	Hagine::Vector3 SquashStretch(const Hagine::Vector3& scale, float easeT, float time, float amplitude, float period);
};

