#include "Player.h"
#include "States/Idle/PlayerStateIdle.h"
#include "States/Move/PlayerStateMove.h"
#include "States/Dash/PlayerStateDash.h"
#include "States/Dodge/PlayerStateDodge.h"
#include "States/Jump/PlayerStateJump.h"

void Player::Init(const std::string objectName) {
	BaseObject::Init(objectName);
	CreatePrimitiveModel(Hagine::PrimitiveType::Cube);

	// ステートを登録
	states_["Idle"] = std::make_unique<PlayerStateIdle>();
	states_["Move"] = std::make_unique<PlayerStateMove>();
	states_["Dash"] = std::make_unique<PlayerStateDash>();
	states_["Dodge"] = std::make_unique<PlayerStateDodge>();
	states_["Jump"] = std::make_unique<PlayerStateJump>();
	currentState_ = states_["Idle"].get();

	context_.transform_ = GetWorldTransform();
	context_.moveComponent_ = &move_;
	context_.jumpComponent_ = &jump_;
}

void Player::Update() {
	if (currentState_) {
		currentState_->Update(*this, context_);
	}

	BaseObject::Update();
}

void Player::Draw(const Hagine::ViewProjection& viewProjection) {
	BaseObject::Draw(viewProjection);
}

void Player::ChangeState(const std::string& stateName) {
	auto it = states_.find(stateName);
	if (it != states_.end()) {
		if (currentState_) {
			currentState_->Exit(*this, context_);
		}
		currentState_ = it->second.get();
		currentState_->Enter(*this, context_);
	}
}
