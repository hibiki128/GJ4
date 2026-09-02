#pragma once
#include "3d/Object/Base/BaseObject.h"

/// <summary>
/// プレイヤーの弾
/// BaseObjectManager に登録して使うので、Update / Draw はマネージャー側から呼ばれる
/// （ゲーム側から自前で呼ぶと二重更新になるので注意）
/// </summary>
class PlayerBullet : public Hagine::BaseObject {
public:
	PlayerBullet() = default;
	~PlayerBullet() = default;

	void Init(const std::string objectName) override;

	/// <summary>
	/// 弾を発射する（待機中の弾を有効化する）
	/// </summary>
	void Fire(const Hagine::Vector3& position, const Hagine::Vector3& direction, float speed);

	void Update() override;
	void Draw(const Hagine::ViewProjection& viewProjection) override;

	/// <summary>
	/// 飛んでいる最中かどうか（false ならプールの空き）
	/// </summary>
	bool IsActive() const { return isActive_; }

private:
	/// <summary>
	/// 弾を待機状態に戻す（プールへ返却する）
	/// </summary>
	void Deactivate();

	// 発射してから消えるまでの秒数
	static constexpr float kLifeTime = 10.0f;

	Hagine::Vector3 velocity_ = {0.0f, 0.0f, 0.0f};
	float lifeTime_ = 0.0f;
	bool isActive_ = false;
};

