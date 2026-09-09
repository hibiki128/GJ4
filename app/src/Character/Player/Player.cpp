#include "Player.h"
#include <frame/Frame.h>
#include "States/Idle/PlayerStateIdle.h"
#include "States/Move/PlayerStateMove.h"
#include "States/Dash/PlayerStateDash.h"
#include "States/Dodge/PlayerStateDodge.h"
#include "States/Jump/PlayerStateJump.h"
#include "States/Damaged/PlayerStateDamaged.h"
#include "Utility/Debug/Param/GameParamHub.h"
#include "Effect/PlayerParticles.h"

namespace {
// 色をそのまま出すための白テクスチャ。
// 既定の uvChecker のままだと、選択色を掛けても格子模様が勝ってしまい何色か分からない
constexpr const char* kPlayerTexturePath = "debug/white1x1.png";
} // namespace

void Player::Init(const std::string objectName) {
	BaseObject::Init(objectName);
	//CreatePrimitiveModel(Hagine::PrimitiveType::Cube);
	CreateModel("slime/slime.obj");
	SetTexture(kPlayerTexturePath);
	SetOffset({ 0.0f,-0.45f,0.0f });

	// ステートを登録
	states_["Idle"] = std::make_unique<PlayerStateIdle>();
	states_["Move"] = std::make_unique<PlayerStateMove>();
	states_["Dash"] = std::make_unique<PlayerStateDash>();
	states_["Dodge"] = std::make_unique<PlayerStateDodge>();
	states_["Jump"] = std::make_unique<PlayerStateJump>();
	states_["Damaged"] = std::make_unique<PlayerStateDamaged>();
	currentState_ = states_["Idle"].get();

	// 弾のプールを生成してオブジェクトマネージャーに登録する
	// （以降、弾の更新と描画はオブジェクトマネージャーが行う）
	bullets_.Init(objectName + "Bullet");

	shoot_.SetWeapon(&weapon_);

	context_.transform_ = GetWorldTransform();
	context_.moveComponent_ = &move_;
	context_.jumpComponent_ = &jump_;
	context_.shootComponent_ = &shoot_;
	context_.reactionComponent_ = &reaction_;
	context_.healthComponent_ = &health_;
	context_.ammoComponent_ = &ammo_;
	context_.bullets = &bullets_;
	context_.rigidBody_ = &GetRigidBody();

	// ぷにぷにの基準スケールを控えておく（以降 scale_ はここを中心に揺れる）。
	// scale_ は毎フレーム上書きするので、大きさの調整はデバッグUIのこちらから行う
	baseScale_ = GetWorldTransform()->scale_;
	Hagine::GameParamHub::GetInstance()->Register("Player", "BaseScale", &baseScale_, {0.01f, 0.05f, 5.0f});

	// 調整パラメータの登録は起動時に一度だけ。
	// GameParamHub::Register は保存済みの値をこの時点で復元してくれる
	reaction_.RegisterParams();
	color_.RegisterParams();
	shoot_.RegisterParams();
	health_.RegisterParams();
	ammo_.RegisterParams();
	for (auto& [stateName, state] : states_) {
		state->RegisterParams();
	}

	// HPを満タンにするのは最大HPが復元された後（Register が保存済みの値を書き戻すため）
	health_.Init();

	// 弾を満タンにするのも最大弾数が復元された後（HPと同じ理由）
	ammo_.Init();

	// 初期ステートの初期化（コンポーネントを context_ へ繋いだ後に呼ぶこと）
	currentState_->Enter(*this, context_);

	// 接地判定はコライダーの衝突で決める（当たる相手は床だけなので、当たった＝床に乗っている）
	// OnCollision は BaseObject の押し出しが使うので、こちらは Enter と Exit を使う
	for (auto& collider : GetColliders()) {
		collider->SetOnCollisionEnter([this](Hagine::ColliderBase*) { context_.isOnGround_ = true; });
		collider->SetOnCollisionExit([this](Hagine::ColliderBase*) { context_.isOnGround_ = false; });
	}
}

void Player::Update() {
	// 読み込み直後などでフレーム間隔が極端に空いたフレームは、重力が一気に積分されて
	// 1フレームで床をすり抜けてしまう。10FPS 相当より遅いフレームは進めずに捨てる
	if (Hagine::Frame::DeltaTime() > 0.1f) {
		return;
	}

	// 回避のクールタイムを進める。回避ステートに入っていない間も減らし続けるので、
	// 時間を進めるのはステートではなくここ1か所にしてある
	if (context_.dodgeCooldown_ > 0.0f) {
		context_.dodgeCooldown_ -= Hagine::Frame::DeltaTime();
	}
	if (context_.dodgeJustTimer_ > 0.0f) {
		context_.dodgeJustTimer_ -= Hagine::Frame::DeltaTime();
	}

	// 被弾はボスの更新の途中で届くので、拾うのは自分の更新の頭でまとめて行う。
	// ここ1か所からしか被弾ステートへ入らないので、更新の順番で挙動が変わらない
	health_.Update();

	// 回避の出だしに攻撃を無敵で弾けたらジャスト回避。体力は減っていないので、
	// ここでやるのは演出の合図だけ。被弾と同じく更新の頭でまとめて拾う
	if (health_.ConsumeBlocked() && context_.dodgeJustTimer_ > 0.0f) {
		context_.dodgeJustTimer_ = 0.0f; // 1回の回避につき1回だけ
		PlayPerfectDodgeEffects(health_.GetLastBlocked());
	}
	if (health_.ConsumeHit()) {
		ChangeState("Damaged");
		// 画面まわりの演出はシーンが受け持つ。ステートを切り替えた後に知らせるので、
		// 通知を受けた側から見ればプレイヤーはもう被弾ステートに入っている
		if (onDamaged_) {
			onDamaged_(health_.GetLastDamage());
		}
	}

	if (currentState_) {
		currentState_->Update(*this, context_);
	}

	// スケールへの反映はここ1か所だけ。
	// こうしておくと、ステートを跨いでも着地のぷにっが上書きされずに最後まで再生される
	reaction_.Update();
	GetWorldTransform()->scale_ = reaction_.Apply(baseScale_);

	// ダッシュの伸びは進行方向へ効かせたいので、再生中だけ体をその向きへ向ける。
	// スライムに正面は無いので、向きが変わっても見た目に出るのは伸びの向きだけ
	if (reaction_.IsDashPlaying()) {
		const float dashYaw = reaction_.GetDashYaw();
		Hagine::WorldTransform* transform = GetWorldTransform();
		transform->quaternionRotation_ = Hagine::Quaternion::FromAxisAngle({0.0f, 1.0f, 0.0f}, dashYaw);
		transform->eulerRotation_.y = dashYaw; // オイラー角で回す設定にされていても向きが合うように
	}

	// 粒の要求（移動中の足元）もここで形にする。ステートは動いている間ずっと要求を出すだけでよい
	PlayerParticles::GetInstance()->Update();

	// 残弾の回復は撃つより先に進める。こうしておくと、回復して1発ぶん貯まったフレームに
	// そのまま撃てる。回復倍率を要求するギミックは、この Update までに呼んでおけば同じフレームで効く
	ammo_.Update();
	shoot_.Update(context_);

	// 選択色を持っているのは射撃コンポーネント。見た目はそれを追いかけるだけ。
	// モデルへ色を書くのもここ1か所だけにしてある
	color_.SetSelectedColor(shoot_.GetSelectedColor());
	color_.Update();
	SetColor(color_.GetDisplayColor());

	BaseObject::Update();

	// フィールドの外へは出さない。BaseObject::Update が速度を座標へ積分した後なので、
	// このフレームの移動結果に対して効く。壁へ押し当てても外向きの速度が消えるだけで、
	// 壁沿いの移動はそのまま残る
	if (pFieldBounds_) {
		pFieldBounds_->ClampToField(GetWorldTransform()->translation_, GetRigidBody().velocity);
	}
}

void Player::Draw(const Hagine::ViewProjection& viewProjection) {
	BaseObject::Draw(viewProjection);
}

void Player::PlayPerfectDodgeEffects(const DamageInfo& info) {
	// 攻撃から離れる向き（水平）。当たる位置が真上・真下で向きが出ないときは、
	// 見ている向きの後ろへ受け流したことにする
	Hagine::Vector3 away = GetWorldPosition() - info.hitPoint;
	away.y = 0.0f;
	if (away.LengthSq() <= 0.0001f) {
		away = Hagine::Vector3{-context_.aimDirection_.x, 0.0f, -context_.aimDirection_.z};
	}
	away = (away.LengthSq() > 0.0001f) ? away.Normalize() : Hagine::Vector3{0.0f, 0.0f, 1.0f};

	// 体を大きく受け流す形へ。スケールの書き手は反応コンポーネントのままなので、
	// 回避中の伸びに割り込んでも形が喧嘩しない
	reaction_.PlayPerfectDodge(away);

	// 衝撃を逃がす波紋と、はじけたゼリー粒
	PlayerParticles::GetInstance()->BurstPerfectDodge(GetWorldPosition(), color_.GetDisplayColor());

	// 白フラッシュやスローモーションは画面ぜんたいの話なのでシーンへ渡す
	if (onPerfectDodge_) {
		onPerfectDodge_(info);
	}
}

void Player::ChangeState(const std::string& stateName) {
	auto it = states_.find(stateName);
	if (it != states_.end()) {
		if (currentState_) {
			currentState_->Exit(*this, context_);
		}
		currentState_ = it->second.get();
		currentState_->Enter(*this, context_);
	}
}
