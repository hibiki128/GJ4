#pragma once
#include "src/Character/Player/Core/PlayerContext.h"

class PlayerJumpComponent {
public:
	// ジャンプした瞬間の処理
	void Jump(PlayerContext& context, float jumpForce);
	// ジャンプ中の処理
	void UpdateJump(PlayerContext& context);
	// ジャンプ中かどうかを取得
	bool IsJumping() const { return isJumping_; }
	// 着地したかどうかを消費する
	bool ConsumeLanded();
	// 着地時のスピードを取得
	float GetLandingSpeed() const { return landingSpeed_; }	
private:
	bool isJumping_ = false;
	float fallSpeed_ = 0.0f;
	float landingSpeed_ = 0.0f;
	bool justLanded_ = false;
};
