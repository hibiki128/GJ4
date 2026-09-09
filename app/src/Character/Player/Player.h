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
#include "Components/Ammo/PlayerAmmoComponent.h"
#include "Components/Voice/PlayerVoiceComponent.h"

#include "src/Character/Player/Weapon/PlayerWeapon.h"
#include "src/Character/Player/Weapon/Bullet/Manager/PlayerBulletManager.h"

class PlayerStateDefeated;

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

	/// <summary>
	/// ボス以外の的の提供元を渡す（回復アイテムの膜など）。
	/// 着弾・照準・ソフトロックオンのいずれもボスと同じように扱われる。
	/// プレイヤーはアイテムの正体を知らないので、配線はシーンが受け持つ
	/// </summary>
	void SetItemTargetProvider(PlayerShootComponent::ExtraTargetProvider provider) {
		shoot_.SetExtraTargetProvider(std::move(provider));
	}

	/// <summary>被弾した瞬間に呼ばれる関数の型</summary>
	using DamagedCallback = std::function<void(const DamageInfo&)>;

	/// <summary>
	/// 被弾の通知先を渡す。プレイヤーはカメラも画面も知らないので、
	/// 画面演出（カメラの衝撃・赤いマスク）の配線はシーンが受け持つ。
	/// 呼ばれるのは体力が実際に減ったときだけ（無敵中の被弾では呼ばれない）
	/// </summary>
	void SetOnDamaged(DamagedCallback callback) { onDamaged_ = std::move(callback); }

	/// <summary>回避に飛び出した瞬間に呼ばれる関数の型（引数は飛び出す向き・ワールド）</summary>
	using DodgeCallback = std::function<void(const Hagine::Vector3&)>;

	/// <summary>
	/// 回避の通知先を渡す。カメラの押し出しのような画面まわりの演出はプレイヤーの仕事ではないので、
	/// 被弾と同じくシーンが受け持つ
	/// </summary>
	void SetOnDodge(DodgeCallback callback) { onDodge_ = std::move(callback); }

	/// <summary>回避に飛び出したことを知らせる（回避ステートから呼ぶ）</summary>
	void NotifyDodge(const Hagine::Vector3& direction) {
		if (onDodge_) {
			onDodge_(direction);
		}
	}

	/// <summary>ジャスト回避が決まった瞬間に呼ばれる関数の型（引数は受け流した攻撃）</summary>
	using PerfectDodgeCallback = std::function<void(const DamageInfo&)>;

	/// <summary>
	/// ジャスト回避の通知先を渡す。白フラッシュやスローモーションは画面ぜんたいの話で
	/// プレイヤーの仕事ではないので、被弾と同じくシーンが受け持つ
	/// </summary>
	void SetOnPerfectDodge(PerfectDodgeCallback callback) { onPerfectDodge_ = std::move(callback); }

	/// <summary>このフレームの狙いが決まったときに呼ばれる関数の型</summary>
	using AimReportCallback = std::function<void(const PlayerAimReport&)>;

	/// <summary>
	/// 狙いの通知先を渡す。レティクルは画面の話なので、被弾や回避と同じくシーンが受け持つ。
	///
	/// 状態を持つだけなら getter で足りそうに見えるが、通知にしてあるのは更新の順番のため。
	/// エンジンは「シーンの更新 → オブジェクトの更新」の順に回すので、シーンから引くと
	/// 必ず1フレーム前の値になり、弾が飛ぶ先とレティクルの位置がずれてしまう。
	/// 射撃の更新が終わった直後に知らせれば、両者が同じフレームの値でそろう
	/// </summary>
	void SetOnAimReport(AimReportCallback callback) { onAimReport_ = std::move(callback); }

	/// <summary>
	/// 更新を止める・再開する（ポーズ中に毎フレーム渡す）。
	///
	/// 止めているあいだは BaseObject::Update も呼ばない。エンジンは
	/// 「シーンの更新 → オブジェクトの更新」の順に回すので、シーンが return するだけでは
	/// プレイヤーの更新は止まらず、入力を入れたままポーズすると、その速度のまま
	/// 座標へ積分され続けて滑っていってしまう。止めるならここで止める必要がある
	/// </summary>
	void SetPaused(bool paused) { isPaused_ = paused; }

	/// <summary>更新を止めているか</summary>
	bool IsPaused() const { return isPaused_; }

	// ステートの切り替え
	void ChangeState(const std::string& stateName);

	/// <summary>ジャスト回避が決まったときの演出をまとめて出す（Update から呼ぶ）</summary>
	/// <param name="info">受け流した攻撃（当たるはずだった位置を向きに使う）</param>
	void PlayPerfectDodgeEffects(const DamageInfo& info);

	/// <summary>
	/// やられて弾ける瞬間の演出をまとめて出す（やられステートから1回だけ）。
	/// 体を消して、体と同じ色の粒を撒き散らす
	/// </summary>
	void PlayDefeatBurst();

	/// <summary>
	/// 見た目だけを揺らす（震えの演出用）。
	///
	/// 動かすのは描画オフセットだけで、座標そのものは動かさない。
	/// こうしておくと重力・フィールドの押し戻し・当たり判定と喧嘩しない。
	/// オフセットの書き手をここ1か所にまとめてあるので、
	/// モデルの原点合わせ（Init で入れているぶん）を上書きしてしまうこともない
	/// </summary>
	/// <param name="shake">揺らす量（ワールド）。ゼロで揺れなし</param>
	void SetRenderShake(const Hagine::Vector3& shake) { SetOffset(baseOffset_ + shake); }

	/// <summary>
	/// やられ演出をやり切ったか。
	/// シーンはこれが立ってからゲームオーバーへ送る（倒れた瞬間に画面を切り替えない）
	/// </summary>
	/// <returns>bool: 震え〜はじけ〜余韻まで終わっていれば true</returns>
	bool IsDefeatFinished() const;

	/// <summary>いま見えている体の色（回避の飛沫など、演出の色合わせに使う）</summary>
	const Hagine::Vector4& GetDisplayColor() const { return color_.GetDisplayColor(); }

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

	/// <summary>残弾の参照（弾数ゲージなど、表示側が最大値や割合を見るのに使う）</summary>
	const PlayerAmmoComponent& GetAmmo() const { return ammo_; }

	/// <summary>
	/// 体力を回復する（回復アイテムを拾ったときに呼ばれる）。
	///
	/// 戻り値を返すのは、満タンで効かなかったときにアイテムを消さずに残せるようにするため。
	/// 弾の回復倍率と同じく、プレイヤーはギミックの正体を知らなくてよい
	/// </summary>
	/// <param name="amount">回復量</param>
	/// <returns>bool: 実際にHPが増えれば true（満タン・死亡後は false）</returns>
	bool Heal(int amount = 1) { return health_.Heal(amount); }

	/// <summary>
	/// この1フレームだけ弾の回復倍率を要求する。乗っている間だけ効く床のような
	/// 継続型のギミックが毎フレーム呼ぶ。プレイヤーはギミックの正体を知らなくてよい
	/// </summary>
	void RequestAmmoRegenScale(float scale) { ammo_.RequestRegenScale(scale); }

	/// <summary>
	/// この1フレームだけ、指定した色の回復倍率を要求する。
	/// 色つきの回復エリアに乗っているあいだ、その色だけが早く戻る
	/// </summary>
	void RequestAmmoRegenScale(Color color, float scale) { ammo_.RequestRegenScale(color, scale); }

	/// <summary>その色の弾が満タンか（回復エリアが役目を終えたかの判断に使う）</summary>
	bool IsAmmoFull(Color color) const { return ammo_.IsFull(color); }

	/// <summary>撃っても弾が減らないようにする（チュートリアルで補給を教えるまでのあいだ）</summary>
	/// <param name="infinite">true で減らなくなる</param>
	void SetInfiniteAmmo(bool infinite) { ammo_.SetInfinite(infinite); }

	/// <summary>残弾を直接決める（チュートリアルで「減った状態」を作るときに使う）</summary>
	/// <param name="color">色</param>
	/// <param name="amount">残弾</param>
	void SetAmmo(Color color, int amount) { ammo_.SetAmmo(color, amount); }

	/// <summary>一定時間だけ効く弾の回復倍率を足す（拾って効く時限型のギミック用）</summary>
	void AddAmmoRegenBoost(float scale, float duration) { ammo_.AddRegenBoost(scale, duration); }

	// 射撃・体力・残弾まわりの状態を表示する（シーンの「オブジェクト設定」窓から呼ぶ）
	void DrawGameplayImGui() {
		health_.DrawImGui();
		ammo_.DrawImGui();
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
	PlayerAmmoComponent ammo_;
	PlayerVoiceComponent voice_;

	PlayerBulletManager bullets_;
	PlayerWeapon weapon_;

	PlayerContext context_;

	// 被弾の通知先（未配線でも被弾そのものは成立する）
	DamagedCallback onDamaged_{};

	// 回避の通知先（未配線でも回避そのものは成立する）
	DodgeCallback onDodge_{};

	// ジャスト回避の通知先（未配線でも受け流しそのものは成立する）
	PerfectDodgeCallback onPerfectDodge_{};

	// 狙いの通知先（未配線ならレティクルが出ないだけで、射撃そのものは成立する）
	AimReportCallback onAimReport_{};

	// 動き回れる範囲（未配線ならどこまでも動ける）
	const IFieldBounds* pFieldBounds_ = nullptr;

	// 更新を止めているか（ポーズ中）。物理も演出も丸ごと止まる
	bool isPaused_ = false;

	// ぷにぷにの中心になるスケール（Init 時のスケールを基準にする）
	Hagine::Vector3 baseScale_ = {1.0f, 1.0f, 1.0f};

	// モデルの原点合わせに使っている描画オフセット（Init で入れたぶん）。
	// 震えの揺れはここへ足す形で出すので、揺れが終われば元の位置へ戻る
	Hagine::Vector3 baseOffset_ = {0.0f, 0.0f, 0.0f};

	// やられステートの実体（IsDefeatFinished から演出の進み具合を引くため）。
	// 所有は states_ 側なので、ここは参照するだけ
	PlayerStateDefeated* defeatedState_ = nullptr;

	bool isJumping_ = false;
};

