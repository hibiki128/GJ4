#pragma once
#include "Application/Character/Player/States/Base/PlayerStateBase.h"

class PlayerStateJump : public PlayerStateBase {
public:
    PlayerStateJump() = default;
	~PlayerStateJump() = default;
    void Enter(Player& player, PlayerContext& context) override;
    void Update(Player& player, PlayerContext& context) override;
    void Exit(Player& player, PlayerContext& context) override;
};

