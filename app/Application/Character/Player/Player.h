#pragma once
#include "Object/Base/BaseObject.h"
#include "Application/Input/GameInput.h"

#include "States/Base/PlayerStateBase.h"
#include "Core/PlayerContext.h"
#include "Application/Character/Player/Components/Move/PlayerMoveComponent.h"
#include "Application/Character/Player/Components/Jump/PlayerJumpComponent.h"

class Player : public Hagine::BaseObject{
public:
	Player() = default;
	~Player() = default;

	void Init(const std::string objectName) override;

	void Update() override;
	void Draw(const Hagine::ViewProjection& viewProjection) override;
	// 入力の処理
	void CommandExecute(const PlayerInput& input) { context_.input_ = input; };
	// ステートの切り替え
	void ChangeState(const std::string& stateName);
private:
	// ステートを格納
	std::unordered_map<std::string, std::unique_ptr<PlayerStateBase>> states_;
	PlayerStateBase* currentState_ = nullptr;

	// コンポーネント群
	PlayerMoveComponent move_;
	PlayerJumpComponent jump_;

	PlayerContext context_;

	bool isJumping_ = false;
};

