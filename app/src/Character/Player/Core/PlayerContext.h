#pragma once
#include "3d/Transform/WorldTransform.h"
#include "Object/Base/BaseObject.h"
#include "src/Input/GameInput.h"

class PlayerMoveComponent;
class PlayerJumpComponent;
class PlayerShootComponent;
class PlayerComponentReaction;
class PlayerHealthComponent;
class PlayerAmmoComponent;
class PlayerBulletManager;

class PlayerContext {
public:
	Hagine::WorldTransform* transform_ = nullptr;
	PlayerMoveComponent* moveComponent_ = nullptr;
	PlayerJumpComponent* jumpComponent_ = nullptr;
	PlayerShootComponent* shootComponent_ = nullptr;
	PlayerComponentReaction* reactionComponent_ = nullptr;
	PlayerHealthComponent* healthComponent_ = nullptr;
	PlayerAmmoComponent* ammoComponent_ = nullptr;
	PlayerBulletManager* bullets = nullptr;
	PlayerInput input_;

	// 照準の射線（カメラ基準）。プレイヤーはカメラを知らないので、シーンが毎フレーム入れる。
	// 未設定のままでもプレイヤーの正面へ撃てるよう、既定はワールド前方にしてある
	Hagine::Vector3 aimOrigin_ = {0.0f, 0.0f, 0.0f};
	Hagine::Vector3 aimDirection_ = {0.0f, 0.0f, 1.0f};

	// 移動の基準になるカメラの向き(ヨー角・ラジアン)。射線と同じくシーンが毎フレーム入れる。
	// 0 のままでもワールド軸そのままで動けるようにしてある
	float cameraYaw_ = 0.0f;

	// 重力と速度は BaseObject のリジッドボディに任せる（Player が自分のものを渡す）
	Hagine::BaseObject::RigidBodyParams* rigidBody_ = nullptr;
	// 床のコライダーに触れている間 true（コライダーの衝突コールバックで更新される）
	bool isOnGround_ = false;

	// 次の回避が出せるようになるまでの残り時間（秒）。回避ステートに入っていない間も
	// 減らし続ける必要があるので、ステートではなく Player::Update が面倒を見る
	float dodgeCooldown_ = 0.0f;

	// ジャスト回避として認める残り時間（秒）。回避に入った瞬間だけ立ち、
	// この間に攻撃を無敵で弾けば「受け流した」ことにする
	float dodgeJustTimer_ = 0.0f;
};

