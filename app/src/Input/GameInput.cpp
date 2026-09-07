#include "GameInput.h"
#include "Input/Input.h"

void GameInput::UpdateInputState() {
	auto input = Hagine::Input::GetInstance();
	auto gamePad = input->GetGamePad();

	if (gamePad->IsConnected()) {
		context_.move = false;
		context_.dir = Hagine::Vector2{ 0.0f, 0.0f };
		if (gamePad->GetLeftStickY() > 0.5f) {
			context_.move = true;
			context_.dir.y += 1.0f;
		} if (gamePad->GetLeftStickY() < -0.5f) {
			context_.move = true;
			context_.dir.y -= 1.0f;
		} if (gamePad->GetLeftStickX() < -0.5f) {
			context_.move = true;
			context_.dir.x -= 1.0f;
		} if (gamePad->GetLeftStickX() > 0.5f) {
			context_.move = true;
			context_.dir.x += 1.0f;
		}

		if (gamePad->IsTrigger(XINPUT_GAMEPAD_A)) {
			context_.jump = true;
		} else {
			context_.jump = false;
		}

		if (gamePad->IsRightTriggerTriggered()) {
			context_.attack = true;
		} else {
			context_.attack = false;
		}

		if (gamePad->IsPress(XINPUT_GAMEPAD_RIGHT_THUMB)) {
			context_.dash = true;
		} else {
			context_.dash = false;
		}
	} else {
		context_.move = false;
		context_.dir = Hagine::Vector2{ 0.0f, 0.0f };
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
}