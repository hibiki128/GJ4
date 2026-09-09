#include "GameInput.h"
#include "Input/Input.h"
#include "src/Character/ColorStruct.h"

namespace {
// 色切り替えに使うキー（Color の並びと対応）
constexpr BYTE kColorKeys[kGameColorCount] = { DIK_1, DIK_2, DIK_3, DIK_4 };

// トリガーを「踏んだ」とみなす押し込み量
constexpr float kTriggerThreshold = 0.25f;

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
	context_.colorCycle = 0;

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

		// ダッシュは LT。押し込み量で見るので、半押しでは走らない
		context_.dash = (gamePad->GetLeftTrigger() > kTriggerThreshold);

		// 回避は押した瞬間だけ拾う。ダッシュと同じ LT なので、押しっぱなしにすれば
		// 最初の1回だけ回避が出て、そのままダッシュへ繋がる
		context_.dodge = gamePad->IsLeftTriggerTriggered(kTriggerThreshold);

		// 撃つ色の切り替え（十字キー 上→右→下→左 が Color の並びに対応）
		for (int i = 0; i < kGameColorCount; ++i) {
			if (gamePad->IsTrigger(kColorPadButtons[i])) {
				context_.selectColorIndex = i;
			}
		}

		// LB / RB は色を1つずつ送る。十字ボタンで直接選ぶのと併用できる
		if (gamePad->IsTrigger(XINPUT_GAMEPAD_LEFT_SHOULDER)) {
			context_.colorCycle = -1;
		} else if (gamePad->IsTrigger(XINPUT_GAMEPAD_RIGHT_SHOULDER)) {
			context_.colorCycle = 1;
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

		context_.dodge = input->TriggerKey(DIK_K);

		// 撃つ色の切り替え（[1][2][3][4]）
		for (int i = 0; i < kGameColorCount; ++i) {
			if (input->TriggerKey(kColorKeys[i])) {
				context_.selectColorIndex = i;
			}
		}

		// 色送り（LB / RB に相当）
		if (input->TriggerKey(DIK_Q)) {
			context_.colorCycle = -1;
		} else if (input->TriggerKey(DIK_E)) {
			context_.colorCycle = 1;
		}
	}

	// 視点操作の入力（カメラへ渡す）
	UpdateCameraInput();
}

void GameInput::UpdateCameraInput() {
	auto input = Hagine::Input::GetInstance();
	auto gamePad = input->GetGamePad();

	// 倒していないフレームは 0。カメラ側は「今フレームどれだけ視点を動かしたいか」だけを受け取る
	cameraContext_.look = Hagine::Vector2{ 0.0f, 0.0f };

	if (gamePad->IsConnected()) {
		// 右スティックで視点変更。デッドゾーンと感度は GamePad 側で処理済みなのでそのまま渡す
		cameraContext_.look.x = gamePad->GetRightStickX();
		cameraContext_.look.y = gamePad->GetRightStickY();
	} else {
		// 矢印キーで視点変更。キーには倒し具合が無いので、押している間は最大まで倒したものとして扱う
		if (input->PushKey(DIK_LEFT)) {
			cameraContext_.look.x -= 1.0f;
		} if (input->PushKey(DIK_RIGHT)) {
			cameraContext_.look.x += 1.0f;
		} if (input->PushKey(DIK_DOWN)) {
			cameraContext_.look.y -= 1.0f;
		} if (input->PushKey(DIK_UP)) {
			cameraContext_.look.y += 1.0f;
		}
	}
}
