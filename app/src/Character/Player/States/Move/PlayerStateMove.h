#pragma once
#include "src/Character/Player/States/Base/PlayerStateBase.h"
#include "Math/type/Vector3.h"
#include "Utility/Debug/Param/GameParamHub.h"

class PlayerStateMove : public PlayerStateBase {
public:
	PlayerStateMove() = default;
	~PlayerStateMove() = default;
	void RegisterParams() override;
	void Enter(Player& player, PlayerContext& context) override;
	void Update(Player& player, PlayerContext& context) override;
	void Exit(Player& player, PlayerContext& context) override;
private:
	float kAmplitude = 0.2f;  // 潰れる量
	float kPeriod = 1.0f; // 揺れの細かさ
	float kSharpness = 1.0f; // 揺れの鋭さ
	float kPhase = 0.0f; // 揺れの位相
	// 移動速度（単位/秒）。Update で DeltaTime を掛けるので、1フレームあたりの量ではない。
	// 既定値をここに置いておかないと、調整ファイルが無い環境でほとんど動かなくなる
	float kMoveSpeed = 7.5f;
};
