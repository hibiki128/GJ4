#include "GameInput.h"
#include "Input/Input.h"
#include "src/Character/ColorStruct.h"

namespace {
// 色切り替えに使うキー（Color の並びと対応）
constexpr BYTE kColorKeys[kGameColorCount] = { DIK_1, DIK_2, DIK_3, DIK_4 };

// 色切り替えに使う十字キー（Color の並びと対応）
constexpr WORD kColorPadButtons[kGameColorCount] = {
	XINPUT_GAMEPAD_DPAD_UP,
	XINPUT_GAMEPAD_DPAD_RIGHT,
	XINPUT_GAMEPAD_DPAD_DOWN,
	XINPUT_GAMEPAD_DPAD_LEFT,
};
} // namespace

void GameInput::UpdateInputState() {
	auto input = Hagine::Input::GetInstance();
	auto gamePad = input->GetGamePad();

	// 押されていないフレームは「変更なし」。押した瞬間だけ色を差し替える
	context_.selectColorIndex = -1;

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

		// 撃つ色の切り替え（十字キー 上→右→下→左 が Color の並びに対応）
		for (int i = 0; i < kGameColorCount; ++i) {
			if (gamePad->IsTrigger(kColorPadButtons[i])) {
				context_.selectColorIndex = i;
			}
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

		// 撃つ色の切り替え（[1][2][3][4]）
		for (int i = 0; i < kGameColorCount; ++i) {
			if (input->TriggerKey(kColorKeys[i])) {
				context_.selectColorIndex = i;
			}
		}
	}
}
