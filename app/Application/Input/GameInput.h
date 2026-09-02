#pragma once
#include "type/Vector2.h"

struct PlayerInput {
	Hagine::Vector2 dir;
	bool move;
	bool jump;
	bool attack;
	bool dash;
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

