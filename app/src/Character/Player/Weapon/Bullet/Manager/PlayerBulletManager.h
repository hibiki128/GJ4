#pragma once
#include "Math/type/Vector3.h"
#include <memory>
#include <string>
#include <vector>
#include "src/Character/Player/Weapon/Bullet/PlayerBullet.h"

/// <summary>
/// プレイヤーの弾を管理する
///
/// 弾は Init で決まった数だけ作り、BaseObjectManager に「非所有登録」しておく。
/// 実体はこのクラスが unique_ptr で持ったまま、更新と描画だけをエンジンに任せる形。
/// 発射時は待機中の弾を使い回す（プール方式）。
///
/// 撃つたびに生成・登録しない理由:
/// Player::Update 自体が BaseObjectManager::Update のループの中から呼ばれているため、
/// そこで登録・解除するとマネージャーが回している最中のコンテナを書き換えることになる。
/// </summary>
class PlayerBulletManager {
public:
	PlayerBulletManager() = default;
	~PlayerBulletManager() = default;

	/// <summary>
	/// 弾のプールを生成してオブジェクトマネージャーに登録する
	/// </summary>
	/// <param name="baseName">弾の名前のもと（マネージャーのキーになるので他と被らない名前にする）</param>
	void Init(const std::string& baseName);

	// 待機中の弾を1発発射する（空きが無ければ何もしない）
	void SpawnBullet(const Hagine::Vector3& position, const Hagine::Vector3& direction, float speed);

private:
	// 同時に存在できる弾の数
	static constexpr size_t kMaxBulletCount = 32;

	std::vector<std::unique_ptr<PlayerBullet>> bullets_;
};
