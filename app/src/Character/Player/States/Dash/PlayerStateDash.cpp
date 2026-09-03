#include "PlayerStateDash.h"
#include "src/Character/Player/Player.h"
#include "src/Character/Player/Core/PlayerContext.h"
#include "src/Character/Player/Components/Move/PlayerMoveComponent.h"

void PlayerStateDash::Enter(Player& player, PlayerContext& context) {
}

void PlayerStateDash::Update(Player& player, PlayerContext& context) {

	context.moveComponent_->Move(context, context.input_.dir, 1.0f);

	if (!context.input_.dash) {
		player.ChangeState("Move");
	}

	if (context.input_.jump) {
		player.ChangeState("Jump");
	}
}

void PlayerStateDash::Exit(Player& player, PlayerContext& context) {
}
