#pragma once
#include "src/Character/Player/States/Base/PlayerStateBase.h"
#include "Math/type/Vector3.h"

class PlayerStateMove : public PlayerStateBase {
public:
	PlayerStateMove() = default;
	~PlayerStateMove() = default;
	void Enter(Player& player, PlayerContext& context) override;
	void Update(Player& player, PlayerContext& context) override;
	void Exit(Player& player, PlayerContext& context) override;
private:
	Hagine::Vector3 baseScale_ = {1.0f, 1.0f, 1.0f};
	float time_ = 0.0f;

	float kDuration = 2.0f; // 長さ
	float kAmplitude = 0.2f;  // 潰れる量
	float kPeriod = 1.0f; // 揺れの細かさ
};

