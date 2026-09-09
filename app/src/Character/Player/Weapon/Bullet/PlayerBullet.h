#pragma once
#include "3d/Object/Base/BaseObject.h"
#include <functional>

/// <summary>
/// プレイヤーの弾
/// BaseObjectManager に登録して使うので、Update / Draw はマネージャー側から呼ばれる
/// （ゲーム側から自前で呼ぶと二重更新になるので注意）
///
/// 当たり判定はコライダーを使わず「前フレームの位置→現在位置の線分」で行う。
/// 弾は1フレームで球の直径以上進むため、重なり判定ではすり抜けてしまうのに対し、
/// 線分なら確実に当たる。判定そのものは相手（ボス）側が持っていて、
/// この弾は線分を渡して「消えてよいか」を聞くだけ。
/// </summary>
class PlayerBullet : public Hagine::BaseObject {
public:
	/// <summary>追尾先の現在位置を取得する関数（追う先が無くなったら false を返す）</summary>
	using TargetPositionGetter = std::function<bool(Hagine::Vector3&)>;

	/// <summary>
	/// 移動した線分で着弾を問い合わせる関数。
	/// 弾を消してよい場合に true を返す。
	/// radius には弾の半径を渡す（見た目どおりの太さで判定してもらうため。
	/// 太さ無しの線分だと、かすった弾が見た目に反してすり抜ける）
	/// </summary>
	using HitTester = std::function<bool(const Hagine::Vector3& from, const Hagine::Vector3& to,
	                                     float radius)>;

	/// <summary>
	/// 1発ぶんの発射内容。
	/// 「どこから・どちらへ・何色で・どう追尾して・どこへ当たりを聞くか」をまとめて渡す
	/// </summary>
	struct Shot {
		Hagine::Vector3 position = {0.0f, 0.0f, 0.0f};
		Hagine::Vector3 direction = {0.0f, 0.0f, 1.0f}; // 初速の向き（正規化されていなくてよい）
		Hagine::Vector4 rgba = {1.0f, 1.0f, 1.0f, 1.0f}; // 表示色
		float radius = 0.3f;         // 弾の半径（見た目の大きさ）
		float speed = 45.0f;         // 速度（単位/秒）
		float lifeTime = 3.0f;       // 寿命（秒）
		float correctionRate = 2.0f; // 軌道補正の強さ（1秒あたりの補正割合）
		float maxTurnDegreesPerSecond = 60.0f; // 1秒あたりに曲がってよい角度の上限

		// 追尾先。空なら補正なしで真っ直ぐ飛ぶ
		TargetPositionGetter targetPositionGetter{};
		// 着弾の問い合わせ先。空なら寿命が尽きるまで飛び続ける
		HitTester hitTester{};
	};

	PlayerBullet() = default;
	~PlayerBullet() = default;

	void Init(const std::string objectName) override;

	/// <summary>
	/// 弾を発射する（待機中の弾を有効化する）
	/// </summary>
	void Fire(const Shot& shot);

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

	/// <summary>
	/// 追尾先へ向きを寄せる。
	/// 追う先を見失ったとき、および通り過ぎたときは以降まっすぐ飛ぶ
	/// </summary>
	void ApplyTrajectoryCorrection(float deltaTime);

	Hagine::Vector3 direction_ = {0.0f, 0.0f, 1.0f};
	float radius_ = 0.0f; // 当たり判定に使う半径（見た目の大きさと同じ値）
	float speed_ = 0.0f;
	float lifeTime_ = 0.0f;
	float correctionRate_ = 0.0f;
	float maxTurnDegreesPerSecond_ = 0.0f;
	bool isActive_ = false;

	TargetPositionGetter targetPositionGetter_{};
	HitTester hitTester_{};
};
