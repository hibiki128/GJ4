#include "PlayerWeapon.h"
#include "Application/Character/Player/Weapon/Bullet/Manager/PlayerBulletManager.h"

void PlayerWeapon::Fire(PlayerBulletManager& bullets, const Hagine::Vector3& pos, const Hagine::Vector3& dir) {
	bullets.SpawnBullet(pos, dir, 10.0f);
}
