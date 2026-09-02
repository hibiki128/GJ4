#include "PlayerStateIdle.h"
#include "Application/Character/Player/Player.h"
#include "Application/Character/Player/Core/PlayerContext.h"

void PlayerStateIdle::Enter(Player& player, PlayerContext& context) {
}

void PlayerStateIdle::Update(Player& player, PlayerContext& context) {

	if (context.input_.move) {
		player.ChangeState("Move");
	}

	if (context.input_.jump) {
		player.ChangeState("Jump");
	}
}

void PlayerStateIdle::Exit(Player& player, PlayerContext& context) {
}
