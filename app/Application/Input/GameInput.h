#pragma once
#include "type/Vector2.h"

struct InputContext {
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
	const InputContext& GetInputContext() const { return context_; };
private:
	// コンテキストを保持
	InputContext context_{};
};

