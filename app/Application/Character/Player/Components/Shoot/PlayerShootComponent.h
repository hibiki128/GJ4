#pragma once
#include "Application/Character/Player/Core/PlayerContext.h"

#include <Application/Character/Player/Weapon/PlayerWeapon.h>
class PlayerShootComponent {
public:
	PlayerShootComponent() = default;
	~PlayerShootComponent() = default;

    void Update(PlayerContext& context);

    // 使用する武器をセットする（実体は Player が持つ）
    void SetWeapon(PlayerWeapon* weapon) { weapon_ = weapon; }

private:

    PlayerWeapon* weapon_ = nullptr;
    float cooldown_ = 0.0f;

};

