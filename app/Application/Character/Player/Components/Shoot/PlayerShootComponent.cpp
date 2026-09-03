#include "PlayerShootComponent.h"
#include "Frame/Frame.h"

void PlayerShootComponent::Update(PlayerContext& context) {
    cooldown_ -= Hagine::Frame::DeltaTime();

    if (!context.input_.attack) {
        return;
    }

    if (cooldown_ > 0.0f) {
        return;
    }

    if (!weapon_ || !context.bullets) {
        return;
    }

    weapon_->Fire(*context.bullets, context.transform_->translation_, {0.0f, 0.0f, 1.0f});

    cooldown_ = 1.0f;
}
