#pragma once
#include "src/Character/Player/States/Base/PlayerStateBase.h"
#include "Math/type/Vector3.h"
#include "Utility/Debug/Param/GameParamHub.h"

class PlayerStateJump : public PlayerStateBase {
public:
    PlayerStateJump() = default;
	~PlayerStateJump() = default;
	void RegisterParams() override;
    void Enter(Player& player, PlayerContext& context) override;
    void Update(Player& player, PlayerContext& context) override;
    void Exit(Player& player, PlayerContext& context) override;

private:
	float kJumpForce = 8.0f; // 踏み切りの初速
	float kRefSpeed = 8.0f; // 速度の正規化基準
	float kRiseStretch = 0.15f; // 上昇中の細さ
	float kFallStretch = 0.2f; // 落下中の細さ
};
