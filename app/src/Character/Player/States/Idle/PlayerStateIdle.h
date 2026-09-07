#pragma once
#include "src/Character/Player/States/Base/PlayerStateBase.h"
#include "Math/type/Vector3.h"

class PlayerStateIdle : public PlayerStateBase {
public:
	PlayerStateIdle() = default;
	~PlayerStateIdle() = default;
	void RegisterParams() override;
	void Enter(Player& player, PlayerContext& context) override;
	void Update(Player& player, PlayerContext& context) override;
	void Exit(Player& player, PlayerContext& context) override;
private:
	float kAmplitude = 0.2f;  // 潰れる量
	float kPeriod = 1.0f; // 揺れの細かさ
	float kSharpness = 1.0f; // 揺れの鋭さ
	float kPhase = 0.0f; // 揺れの位相
};
