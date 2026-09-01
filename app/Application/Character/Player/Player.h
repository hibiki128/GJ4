#pragma once
#include "Object/Base/BaseObject.h"
#include "Application/Input/GameInput.h"

class Player : public Hagine::BaseObject{
public:
	Player() = default;
	~Player() = default;

	void Init(const std::string objectName) override;

	void Update() override;
	void Draw(const Hagine::ViewProjection& viewProjection) override;
	// 
	void SetInputContext(const InputContext& context) { inputContext_ = context; }
private:
	InputContext inputContext_;

	bool isJumping_ = false;
};

