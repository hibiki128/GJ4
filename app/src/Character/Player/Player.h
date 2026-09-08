#pragma once
#include "Object/Base/BaseObject.h"
#include "src/Input/GameInput.h"

#include "States/Base/PlayerStateBase.h"
#include "Core/PlayerContext.h"
#include "Components/Move/PlayerMoveComponent.h"
#include "Components/Jump/PlayerJumpComponent.h"
#include "Components/Shoot/PlayerShootComponent.h"
#include "Components/Reaction/PlayerComponentReaction.h"
#include "Components/Color/PlayerColorComponent.h"

#include "src/Character/Player/Weapon/PlayerWeapon.h"
#include "src/Character/Player/Weapon/Bullet/Manager/PlayerBulletManager.h"

class Player : public Hagine::BaseObject{
public:
	Player() = default;
	~Player() = default;

	void Init(const std::string objectName) override;

	void Update() override;
	void Draw(const Hagine::ViewProjection& viewProjection) override;
	// 入力の処理
	void CommandExecute(const PlayerInput& input) { context_.input_ = input; };
	// ステートの切り替え
	void ChangeState(const std::string& stateName);

	// 色マスタを受け取る（初期化時に一度だけ。この時点の選択色はそのまま反映される）
	void SetColorPalette(const BossColorPalette& palette) { color_.SetPalette(palette); }
	// 選択中の色を伝える。モデルの色はここから滑らかに切り替わる
	// immediate を true にすると補間せずその場で切り替える
	void SetSelectedColor(Color color, bool immediate = false) { color_.SetSelectedColor(color, immediate); }
	// 選択中の色
	Color GetSelectedColor() const { return color_.GetSelectedColor(); }
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

	PlayerBulletManager bullets_;
	PlayerWeapon weapon_;

	PlayerContext context_;

	// ぷにぷにの中心になるスケール（Init 時のスケールを基準にする）
	Hagine::Vector3 baseScale_ = {1.0f, 1.0f, 1.0f};

	bool isJumping_ = false;
};

