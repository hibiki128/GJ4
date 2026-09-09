#include "PlayerWeapon.h"
#include "src/Character/Player/Weapon/Bullet/Manager/PlayerBulletManager.h"

bool PlayerWeapon::Fire(PlayerBulletManager& bullets, const FireRequest& request) {
	PlayerBullet::Shot shot{};
	shot.position = request.origin;
	shot.direction = request.direction;
	shot.rgba = request.rgba;
	shot.radius = params_.radius;
	shot.hitRadius = GetHitRadius();
	shot.speed = params_.speed;
	shot.lifeTime = params_.lifeTime;
	shot.correctionRate = params_.correctionRate;
	shot.maxTurnDegreesPerSecond = params_.maxTurnDegreesPerSecond;
	shot.targetPositionGetter = request.targetPositionGetter;
	shot.hitTester = request.hitTester;

	return bullets.SpawnBullet(shot);
}
