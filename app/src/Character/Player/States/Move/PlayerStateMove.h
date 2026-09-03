#pragma once
#include "src/Character/Player/States/Base/PlayerStateBase.h"

class PlayerStateMove : public PlayerStateBase {
public:
	PlayerStateMove() = default;
	~PlayerStateMove() = default;
	void Enter(Player& player, PlayerContext& context) override;
	void Update(Player& player, PlayerContext& context) override;
	void Exit(Player& player, PlayerContext& context) override;
};

