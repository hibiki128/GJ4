#pragma once
#include "Object/Base/BaseObject.h"
#include "src/Input/GameInput.h"
#include "src/Interface/IColorProvider.h"
#include "src/Interface/IDamageable.h"
#include "src/Interface/IFieldBounds.h"

#include "States/Base/PlayerStateBase.h"
#include "Core/PlayerContext.h"
#include "Components/Move/PlayerMoveComponent.h"
#include "Components/Jump/PlayerJumpComponent.h"
#include "Components/Shoot/PlayerShootComponent.h"
#include "Components/Reaction/PlayerComponentReaction.h"
#include "Components/Color/PlayerColorComponent.h"
#include "Components/Health/PlayerHealthComponent.h"

#include "src/Character/Player/Weapon/PlayerWeapon.h"
#include "src/Character/Player/Weapon/Bullet/Manager/PlayerBulletManager.h"

class Player : public Hagine::BaseObject, public IColorProvider, public IDamageable {
public:
	Player() = default;
	~Player() = default;

	void Init(const std::string objectName) override;

	void Update() override;
	void Draw(const Hagine::ViewProjection& viewProjection) override;
	// 入力の処理
	void CommandExecute(const PlayerInput& input) { context_.input_ = input; };

	// 照準の射線を渡す（カメラを見ているシーン側から毎フレーム）
	void SetAim(const Hagine::Vector3& origin, const Hagine::Vector3& direction) {
		context_.aimOrigin_ = origin;
		context_.aimDirection_ = direction;
	}

	// 移動の基準になるカメラの向きを渡す（射線と同じくシーン側から毎フレーム）
	void SetCameraYaw(float yaw) { context_.cameraYaw_ = yaw; }

	/// <summary>
	/// 動き回れる範囲を渡す（初期化時に一度だけ）。プレイヤーは Field の型を知らず、
	/// 更新の最後に「範囲の外にいたら戻して」と頼むだけ
	/// </summary>
	void SetFieldBounds(const IFieldBounds* field) { pFieldBounds_ = field; }

	// 撃つ相手の提供元を渡す（形態の切り替えはシーン側が判断する）
	void SetBossTargetProvider(PlayerShootComponent::TargetProvider provider) {
		shoot_.SetTargetProvider(std::move(provider));
	}

	/// <summary>被弾した瞬間に呼ばれる関数の型</summary>
	using DamagedCallback = std::function<void(const DamageInfo&)>;

	/// <summary>
	/// 被弾の通知先を渡す。プレイヤーはカメラも画面も知らないので、
	/// 画面演出（カメラの衝撃・赤いマスク）の配線はシーンが受け持つ。
	/// 呼ばれるのは体力が実際に減ったときだけ（無敵中の被弾では呼ばれない）
	/// </summary>
	void SetOnDamaged(DamagedCallback callback) { onDamaged_ = std::move(callback); }

	// ステートの切り替え
	void ChangeState(const std::string& stateName);

	// 色マスタを受け取る（初期化時に一度だけ。モデルの色はここから引く）
	void SetColorPalette(const BossColorPalette& palette) { color_.SetPalette(palette); }

	/// ===================================================
	/// IColorProvider
	/// ===================================================

	// いま撃つ色。ボスは Player の型を知らず、この口だけを見て色一致を判定する
	Color GetSelectedColor() const override { return shoot_.GetSelectedColor(); }
	// 選択色を変える。持ち主は射撃コンポーネントで、モデルの色は Update でそれを追いかける。
	// immediate を true にするとモデルの色を補間せずその場で切り替える（初期化時用）
	void SetSelectedColor(Color color, bool immediate = false) {
		shoot_.SetSelectedColor(color);
		color_.SetSelectedColor(color, immediate);
	}

	/// ===================================================
	/// IDamageable
	/// ===================================================

	/// <summary>
	/// ボスの攻撃を受け取る。呼ばれるのは相手（ボス）の更新の途中なので、
	/// ここでは体力を減らすだけにして、演出とステートの切り替えは自分の Update まで持ち越す。
	/// こうしておけば、ボスとプレイヤーのどちらが先に更新されても結果が変わらない
	/// </summary>
	void ApplyDamage(const DamageInfo& info) override { health_.ApplyDamage(info); }

	/// <summary>残りHP（IDamageable は float で扱うので変換して返す）</summary>
	float GetHp() const override { return static_cast<float>(health_.GetHp()); }

	/// <summary>倒れたか（HPが0）</summary>
	bool IsDead() const override { return health_.IsDead(); }

	/// <summary>体力の参照（HPゲージなど、表示側が最大値や割合を見るのに使う）</summary>
	const PlayerHealthComponent& GetHealth() const { return health_; }

	// 射撃・体力まわりの状態を表示する（シーンの「オブジェクト設定」窓から呼ぶ）
	void DrawGameplayImGui() {
		health_.DrawImGui();
		shoot_.DrawImGui();
	}

private:
	// ステートを格納
	std::unordered_map<std::string, std::unique_ptr<PlayerStateBase>> states_;
	PlayerStateBase* currentState_ = nullptr;

	// コンポーネント群
	PlayerMoveComponent move_;
	PlayerJumpComponent jump_;
	PlayerShootComponent shoot_;
	PlayerComponentReaction reaction_;
	PlayerColorComponent color_;
	PlayerHealthComponent health_;

	PlayerBulletManager bullets_;
	PlayerWeapon weapon_;

	PlayerContext context_;

	// 被弾の通知先（未配線でも被弾そのものは成立する）
	DamagedCallback onDamaged_{};

	// 動き回れる範囲（未配線ならどこまでも動ける）
	const IFieldBounds* pFieldBounds_ = nullptr;

	// ぷにぷにの中心になるスケール（Init 時のスケールを基準にする）
	Hagine::Vector3 baseScale_ = {1.0f, 1.0f, 1.0f};

	bool isJumping_ = false;
};

