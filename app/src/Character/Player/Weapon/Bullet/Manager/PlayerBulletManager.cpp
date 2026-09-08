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

bool PlayerBulletManager::SpawnBullet(const PlayerBullet::Shot& shot) {
	// 待機中の弾を探して撃つ
	for (auto& bullet : bullets_) {
		if (bullet->IsActive()) {
			continue;
		}

		bullet->Fire(shot);
		return true;
	}

	// 空きが無い場合は発射しない（同時に飛べる弾の上限）。
	// 撃った側は残弾を戻せるよう、撃てなかったことを戻り値で知る
	return false;
}
