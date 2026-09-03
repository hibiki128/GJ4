#include "PlayerBulletManager.h"
#include "3d/Object/Base/BaseObjectManager.h"

void PlayerBulletManager::Init(const std::string& baseName) {
	bullets_.clear();
	bullets_.reserve(kMaxBulletCount);

	for (size_t i = 0; i < kMaxBulletCount; ++i) {
		auto bullet = std::make_unique<PlayerBullet>();

		// 名前が BaseObjectManager のキーになるので、必ず一意にする
		bullet->Init(baseName + "_" + std::to_string(i));

		// 実体はこのクラスが持ったまま、参照だけマネージャーに渡す。
		// これで弾の Update / Draw はマネージャーが回してくれる。
		// 登録解除は BaseObject のデストラクタが自動でやってくれる
		Hagine::BaseObjectManager::GetInstance()->RegisterExternal(bullet.get());

		bullets_.push_back(std::move(bullet));
	}
}

void PlayerBulletManager::SpawnBullet(const Hagine::Vector3& position, const Hagine::Vector3& direction, float speed) {
	// 待機中の弾を探して撃つ
	for (auto& bullet : bullets_) {
		if (bullet->IsActive()) {
			continue;
		}

		bullet->Fire(position, direction, speed);
		return;
	}

	// 空きが無い場合は発射しない（弾数の上限）
}
