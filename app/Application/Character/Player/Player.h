#pragma once
#include "Object/Base/BaseObject.h"

class Player : public Hagine::BaseObject{
public:
	Player() = default;
	~Player() = default;

	void Init(const std::string objectName) override;
	void Update() override;
	void Draw(const Hagine::ViewProjection& viewProjection) override;
};

