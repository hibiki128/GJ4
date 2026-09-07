#pragma once
#include "src/Character/Player/States/Base/PlayerStateBase.h"
#include "Math/type/Vector3.h"
#include "Utility/Debug/Param/GameParamHub.h"

class PlayerStateIdle : public PlayerStateBase {
public:
	PlayerStateIdle() = default;
	~PlayerStateIdle() = default;
	void Enter(Player& player, PlayerContext& context) override;
	void Update(Player& player, PlayerContext& context) override;
	void Exit(Player& player, PlayerContext& context) override;
private:
	Hagine::Vector3 baseScale_ = {1.0f, 1.0f, 1.0f};
	float time_ = 0.0f;

	// GameParamHub への登録用。生成時にオーナーを決めておくと、
	// 登録は名前と変数だけで済み、破棄時の解除も自動で行われる
	Hagine::GameParamOwner params_{"Player/Idle"};

	float kDuration = 2.0f; // 長さ
	float kAmplitude = 0.2f;  // 潰れる量
	float kPeriod = 1.0f; // 揺れの細かさ
};