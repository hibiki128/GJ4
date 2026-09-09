#include "PlayerReticle.h"
#include "MyMath.h"
#include "Utility/Debug/Param/GameParamHub.h"
#include "window/WinApp.h"
#include <algorithm>

using namespace Hagine;

namespace {

/// <summary>照準レティクルの絵（色つきの十字）。images ルートからの相対パス</summary>
constexpr const char* kAimTexturePath = "Hud/Reticle/reticle.png";

/// <summary>発射レティクルの絵（水色の円）。照準と形がはっきり違うものを選んである</summary>
constexpr const char* kFiringTexturePath = "Hud/Reticle/reticle_assist.png";

/// <summary>
/// ワールドの一点をUIの座標（仮想解像度のピクセル）へ落とす。
///
/// スプライトの座標は実ウィンドウのサイズではなく仮想解像度が基準なので、
/// ウィンドウを伸ばしてもレティクルと着弾点の関係はずれない（仕様書 11.2）
/// </summary>
/// <param name="viewProjection">描画に使っているカメラ</param>
/// <param name="world">落としたいワールド座標</param>
/// <param name="outScreen">画面座標（左上原点・ピクセル）</param>
/// <returns>bool: 画面へ落とせれば true（カメラの後ろにある点は false）</returns>
bool WorldToScreen(const ViewProjection& viewProjection, const Vector3& world, Vector2& outScreen) {
	const Matrix4x4 viewProjectionMatrix = viewProjection.matView_ * viewProjection.matProjection_;

	// 透視除算する前の値が要る。除算済みの Transformation では、カメラの後ろにある点が
	// 符号の反転した「画面内の点」に化けてしまい、あらぬ場所にレティクルが出る
	const Vector4 clip = TransformationRaw(Vector4{world.x, world.y, world.z, 1.0f}, viewProjectionMatrix);
	if (clip.w <= 0.0001f) {
		return false; // カメラの後ろ、または真横。投影できない
	}

	const float ndcX = clip.x / clip.w;
	const float ndcY = clip.y / clip.w;

	outScreen.x = (ndcX * 0.5f + 0.5f) * static_cast<float>(WinApp::GetVirtualWidth());
	outScreen.y = (0.5f - ndcY * 0.5f) * static_cast<float>(WinApp::GetVirtualHeight());
	return true;
}

} // namespace

void PlayerReticle::Init() {
	if (aimSprite_.IsReady()) {
		return;
	}

	// アンカーは中心。狙っている一点にレティクルの真ん中が乗る
	aimSprite_.Initialize(kAimTexturePath, Vector2{0.5f, 0.5f});
	firingSprite_.Initialize(kFiringTexturePath, Vector2{0.5f, 0.5f});

	// 最初の狙いが届くまでの間、発射レティクルが画面の隅（座標0）に居座らないようにする
	Reset();
}

void PlayerReticle::RegisterParams() {
	const std::string paramOwnerLabel = "Player/Reticle";
	GameParamHub* hub = GameParamHub::GetInstance();

	hub->Register(paramOwnerLabel, "Enabled", &enabled_);

	hub->Register(paramOwnerLabel, "AimSize", &aimSize_, {0.5f, 4.0f, 256.0f});
	hub->Register(paramOwnerLabel, "FiringSize", &firingSize_, {0.5f, 4.0f, 256.0f});

	hub->Register(paramOwnerLabel, "DisplayThreshold", &displayThreshold_, {0.5f, 0.0f, 200.0f});
	hub->Register(paramOwnerLabel, "MoveTime", &moveTime_, {0.005f, 0.0f, 0.5f});

	GameParamHub::Options colorOptions{};
	colorOptions.speed = 0.01f;
	colorOptions.min = 0.0f;
	colorOptions.max = 1.0f;
	colorOptions.isColor = true;
	hub->Register(paramOwnerLabel, "AimColor", &aimColor_, colorOptions);
	hub->Register(paramOwnerLabel, "FiringColor", &firingColor_, colorOptions);
}

void PlayerReticle::Update(const PlayerAimReport& report, const ViewProjection& viewProjection, float deltaTime) {
	const Vector2 center = ScreenCenter();

	/// ===================================================
	/// 発射レティクルの行き先を決める
	/// ===================================================

	// 発射レティクルは常に出しておき、行き先だけを切り替える。
	//  ・画面中央 … 弾は狙ったところに当たる。円が照準レティクルを囲んだ状態
	//  ・着弾点   … 弾は別のところに当たる。円だけが離れていく
	// 着弾点へ移るには2つとも満たすこと。
	//  1. 弾が本当に何かへ当たること。当たらないなら指すべき着弾点が無い（仕様書 9.2）
	//  2. その着弾点が画面中央から閾値ぶん離れていること（仕様書 6）
	bool diverged = false;
	Vector2 target = center;

	Vector2 firePoint{};
	if (report.firePointHit && WorldToScreen(viewProjection, report.firePoint, firePoint)) {
		if ((firePoint - center).Length() >= displayThreshold_) {
			diverged = true;
			target = firePoint;
		}
	}

	// 行き先が入れ替わった瞬間だけ、いまいる場所から動かし直す。
	// 中央へ戻るときも同じ時間をかけて滑らかに戻り、いきなり跳ばない（仕様書 7）
	if (diverged != firingDiverged_) {
		firingDiverged_ = diverged;
		firingStart_ = firingCurrent_;
		firingMoveElapsed_ = 0.0f;
	}
	firingTarget_ = target;

	/// ===================================================
	/// 位置を進める
	/// ===================================================

	// 経過時間で補間しているので moveTime_ を過ぎれば必ず行き先へ届き、
	// 以降は毎フレームの着弾点をそのまま指す。
	// つまり「表示が追いつかないまま撃つ」時間は moveTime_ の間だけで済む（仕様書 7）
	firingMoveElapsed_ += deltaTime;
	const float rate = (moveTime_ > 0.0f) ? std::clamp(firingMoveElapsed_ / moveTime_, 0.0f, 1.0f) : 1.0f;
	firingCurrent_ = firingStart_ + (firingTarget_ - firingStart_) * rate;
}

void PlayerReticle::Draw() {
	if (!enabled_) {
		return;
	}

	// 発射レティクルは常に出す。ズレていなければ画面中央で照準レティクルを囲み、
	// ズレていればその着弾点へ移っている。
	// 先に描いて照準レティクルを手前に重ねるので、重なっても基準のほうが隠れない
	DrawReticle(firingSprite_, firingCurrent_, firingSize_, firingColor_);

	// 照準レティクルは常に画面中央（仕様書 3.1 / 11.2）。
	// 補正が起きても動かさないことで、プレイヤーの操作基準がぶれない
	DrawReticle(aimSprite_, ScreenCenter(), aimSize_, aimColor_);
}

void PlayerReticle::Reset() {
	firingDiverged_ = false;
	firingMoveElapsed_ = 0.0f;
	firingCurrent_ = ScreenCenter();
	firingTarget_ = firingCurrent_;
	firingStart_ = firingCurrent_;
}

Vector2 PlayerReticle::ScreenCenter() {
	return Vector2{
		static_cast<float>(WinApp::GetVirtualWidth()) * 0.5f, static_cast<float>(WinApp::GetVirtualHeight()) * 0.5f
	};
}

void PlayerReticle::DrawReticle(GameUi::UiSprite& sprite, const Vector2& center, float height,
                                const Vector4& color) {
	if (!sprite.IsReady() || height <= 0.0f) {
		return;
	}

	// 横幅はテクスチャ本来の縦横比から決める。絵を差し替えて正方形でなくなっても歪まない
	const Vector2& baseSize = sprite.GetBaseSize();
	const float aspect = (baseSize.y > 0.0f) ? (baseSize.x / baseSize.y) : 1.0f;

	sprite.Draw(center, Vector2{height * aspect, height}, color);
}
