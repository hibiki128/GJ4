#pragma once
#include "Application/Character/Player/States/Base/PlayerStateBase.h"

class PlayerStateDash : public PlayerStateBase {
public:
	PlayerStateDash() = default;
	~PlayerStateDash() = default;
	void Enter(Player& player, PlayerContext& context) override;
	void Update(Player& player, PlayerContext& context) override;
	void Exit(Player& player, PlayerContext& context) override;
};

