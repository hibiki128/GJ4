#include "pch.h"
#include "PlayerComponentReaction.h"
#include "Math/Easing.h"

void PlayerComponentReaction::SquashStretch(const Hagine::Vector3& scale, float easeT, float time, float amplitude, float period) {
	Hagine::EaseAmplitudeScale(scale, easeT, time, amplitude, period);
}
