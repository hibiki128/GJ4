#include "Player.h"

void Player::Init(const std::string objectName) {
	BaseObject::Init(objectName);
	CreatePrimitiveModel(Hagine::PrimitiveType::Cube);
}

void Player::Update() {
	BaseObject::Update();
}

void Player::Draw(const Hagine::ViewProjection& viewProjection) {
	BaseObject::Draw(viewProjection);
}
