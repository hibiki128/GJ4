#pragma once
#include "type/Vector2.h"

struct PlayerInput {
	Hagine::Vector2 dir;
	bool move;
	bool jump;
	bool attack;
	bool dash;
	// 選んだ色の添字（-1 なら変更なし）。ColorStruct.h の Color の並びと対応する
	int selectColorIndex = -1;
};

class GameInput {
public:
	GameInput() = default;
	~GameInput() = default;
	// 入力状態を取得
	void UpdateInputState();
	// コンテキストを取得
	const PlayerInput& GetInputContext() const { return context_; };
private:
	// コンテキストを保持
	PlayerInput context_{};
};

