#pragma once
#include <Math/type/Vector3.h>

class PlayerBulletManager;

class PlayerWeapon {
public:
    void Fire(PlayerBulletManager& bullets, const Hagine::Vector3& pos, const Hagine::Vector3& dir);
};

