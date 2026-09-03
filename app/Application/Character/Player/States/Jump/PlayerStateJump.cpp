#include "PlayerStateJump.h"
#include "Application/Character/Player/Player.h"
#include "Application/Character/Player/Core/PlayerContext.h"
#include "Application/Character/Player/Components/Jump/PlayerJumpComponent.h"
#include "Application/Character/Player/Components/Move/PlayerMoveComponent.h"

void PlayerStateJump::Enter(Player& player, PlayerContext& context) {
	
}

void PlayerStateJump::Update(Player& player, PlayerContext& context) {
	if (!context.jumpComponent_->IsJumping()) {
		context.jumpComponent_->Jump(context, 1.0f);
	}
	context.jumpComponent_->UpdateJump(context);
	context.moveComponent_->Move(context, context.input_.dir, 0.2f);

	if (!context.jumpComponent_->IsJumping()) {
		player.ChangeState("Idle");
	}
}

void PlayerStateJump::Exit(Player& player, PlayerContext& context) {
}
