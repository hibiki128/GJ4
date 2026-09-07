#include "pch.h"
#include "PlayerComponentReaction.h"
#include "Math/Easing.h"

Hagine::Vector3 PlayerComponentReaction::SquashStretch(const Hagine::Vector3& scale, float easeT, float time, float amplitude, float period) {
	return Hagine::EaseAmplitudeScale(scale, easeT, time, amplitude, period);
}
