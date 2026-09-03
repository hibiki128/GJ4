#pragma once
#include "Application/Character/Player/Core/PlayerContext.h"

class PlayerJumpComponent {
public:
	// ジャンプした瞬間の処理
	void Jump(PlayerContext& context, float jumpForce);
	// ジャンプ中の処理
	void UpdateJump(PlayerContext& context);
	// ジャンプ中かどうかを取得
	bool IsJumping() const { return isJumping_; }
private:
	bool isJumping_ = false;
};
