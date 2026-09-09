#pragma once
#include "src/Character/Player/States/Base/PlayerStateBase.h"

class PlayerStateDash : public PlayerStateBase {
public:
	PlayerStateDash() = default;
	~PlayerStateDash() = default;
	void RegisterParams() override;
	void Enter(Player& player, PlayerContext& context) override;
	void Update(Player& player, PlayerContext& context) override;
	void Exit(Player& player, PlayerContext& context) override;
private:
	// 走る速度（単位/秒）。歩きと同じく DeltaTime を掛けて使う
	float kMoveSpeed = 15.0f;
	float kAmplitude = 0.2f;  // 潰れる量
	float kPeriod = 1.0f; // 揺れの細かさ
	float kSharpness = 1.0f; // 揺れの鋭さ
	float kPhase = 0.0f; // 揺れの位相
};

