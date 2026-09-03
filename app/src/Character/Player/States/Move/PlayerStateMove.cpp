#include "PlayerStateMove.h"
#include "src/Character/Player/Player.h"
#include "src/Character/Player/Core/PlayerContext.h"
#include "src/Character/Player/Components/Move/PlayerMoveComponent.h"


void PlayerStateMove::Enter(Player& player, PlayerContext& context) {
}

void PlayerStateMove::Update(Player& player, PlayerContext& context) {
	context.moveComponent_->Move(context, context.input_.dir, 0.2);

	if (!context.input_.move) {
		player.ChangeState("Idle");
		return;
	}

	if (context.input_.dash) {
		player.ChangeState("Dodge");
		return;
	}

	if (context.input_.jump) {
		player.ChangeState("Jump");
		return;
	}
}

void PlayerStateMove::Exit(Player& player, PlayerContext& context) {
}