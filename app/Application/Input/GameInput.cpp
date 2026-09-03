#include "GameInput.h"
#include "Input/Input.h"

void GameInput::UpdateInputState() {
	auto input = Hagine::Input::GetInstance();

	context_.move = false;
	context_.dir = Hagine::Vector2{0.0f, 0.0f};
	if (input->PushKey(DIK_W)) {
		context_.move = true;
		context_.dir.y += 1.0f;
	} if (input->PushKey(DIK_S)) {
		context_.move = true;
		context_.dir.y -= 1.0f;
	} if (input->PushKey(DIK_A)) {
		context_.move = true;
		context_.dir.x -= 1.0f;
	} if (input->PushKey(DIK_D)) {
		context_.move = true;
		context_.dir.x += 1.0f;
	}

	if (input->TriggerKey(DIK_SPACE)) {
		context_.jump = true;
	} else {
		context_.jump = false;
	}

	if (input->PushKey(DIK_J)) {
		context_.attack = true;
	} else {
		context_.attack = false;
	}

	if (input->PushKey(DIK_K)) {
		context_.dash = true;
	} else {
		context_.dash = false;
	}
}