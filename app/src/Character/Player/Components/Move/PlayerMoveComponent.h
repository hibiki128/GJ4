#pragma once
#include "Math/type/Vector2.h"
#include "Math/type/Vector3.h"
#include "src/Character/Player/Core/PlayerContext.h"

class PlayerMoveComponent {
public:
	void Move(PlayerContext& context, const Hagine::Vector2& input, float moveSpeed);

	/// <summary>
	/// すでにワールドの向きになっている方向へ、この1フレームぶんの距離だけ進める。
	/// 入力ではなく向きを受け取るので、途中でカメラが回っても軌道が曲がらない
	/// （回避のように、飛び出す向きを決めてから動かしたいとき用）
	/// </summary>
	/// <param name="worldDirection">進む向き（正規化しておくこと）</param>
	/// <param name="distance">このフレームで進む距離</param>
	void MoveWorld(PlayerContext& context, const Hagine::Vector3& worldDirection, float distance);
};
