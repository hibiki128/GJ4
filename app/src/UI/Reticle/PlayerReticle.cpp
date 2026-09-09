#include "PlayerReticle.h"
#include "MyMath.h"
#include "Utility/Debug/Param/GameParamHub.h"
#include "window/WinApp.h"
#include <algorithm>

using namespace Hagine;

namespace {

/// <summary>
/// 「+」1つに使う板の枚数（明るい線2本＋アウトライン2本）
/// </summary>
constexpr int kRectsPerCross = 4;

/// <summary>
/// 1フレームに描く「+」の数（照準・発射）
/// </summary>
constexpr int kCrossCount = 2;

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
	if (initialized_) {
		return;
	}

	rects_.Initialize(kRectsPerCross * kCrossCount);
	initialized_ = true;

	// 最初の狙いが届くまでの間、発射レティクルが画面の隅（座標0）に居座らないようにする
	Reset();
}

void PlayerReticle::RegisterParams() {
	const std::string paramOwnerLabel = "Player/Reticle";
	GameParamHub* hub = GameParamHub::GetInstance();

	hub->Register(paramOwnerLabel, "Enabled", &enabled_);

	hub->Register(paramOwnerLabel, "AimSize", &aimSize_, {0.5f, 2.0f, 64.0f});
	hub->Register(paramOwnerLabel, "FiringSize", &firingSize_, {0.5f, 2.0f, 64.0f});
	hub->Register(paramOwnerLabel, "LineWidth", &lineWidth_, {0.1f, 1.0f, 8.0f});
	hub->Register(paramOwnerLabel, "OutlineWidth", &outlineWidth_, {0.1f, 0.0f, 4.0f});
	hub->Register(paramOwnerLabel, "OutlineAlpha", &outlineAlpha_, {0.01f, 0.0f, 1.0f});

	hub->Register(paramOwnerLabel, "DisplayThreshold", &displayThreshold_, {0.5f, 0.0f, 200.0f});
	hub->Register(paramOwnerLabel, "MoveTime", &moveTime_, {0.005f, 0.0f, 0.5f});
	hub->Register(paramOwnerLabel, "FadeTime", &fadeTime_, {0.005f, 0.0f, 0.5f});

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
	/// 発射レティクルを出すか決める
	/// ===================================================

	// 出す条件は2つ。
	//  1. 弾が本当に何かへ当たること。当たらないなら見せるべき着弾点が無いので出さない（仕様書 9.2）
	//  2. その着弾点が画面中央から閾値ぶん離れていること（仕様書 6）
	bool shouldShow = false;
	Vector2 firePoint{};
	if (report.firePointHit && WorldToScreen(viewProjection, report.firePoint, firePoint)) {
		shouldShow = ((firePoint - center).Length() >= displayThreshold_);
	}

	if (shouldShow) {
		firingTarget_ = firePoint;
		if (!firingVisible_) {
			// 出た瞬間。いきなり最終位置へ跳ばすと目が追えないので、
			// 画面中央から動かし始める（仕様書 7）
			firingVisible_ = true;
			firingStart_ = center;
			firingCurrent_ = center;
			firingMoveElapsed_ = 0.0f;
		}
	} else {
		firingVisible_ = false;
	}

	/// ===================================================
	/// 濃さと位置を進める
	/// ===================================================

	// ぱっと消すと点滅して見えるので、出るときも消えるときも同じ時間でフェードする
	const float fadeStep = (fadeTime_ > 0.0f) ? (deltaTime / fadeTime_) : 1.0f;
	firingAlpha_ = std::clamp(firingAlpha_ + (firingVisible_ ? fadeStep : -fadeStep), 0.0f, 1.0f);

	if (!firingVisible_) {
		return; // 消えていく間は最後の位置に置いたまま薄くする
	}

	// 画面中央から着弾点へ移動する。経過時間で補間しているので moveTime_ を過ぎれば
	// 必ず目標へ届き、以降は毎フレームの着弾点をそのまま指す。
	// つまり「表示が追いつかないまま撃つ」時間は moveTime_ の間だけで済む（仕様書 7）
	firingMoveElapsed_ += deltaTime;
	const float rate = (moveTime_ > 0.0f) ? std::clamp(firingMoveElapsed_ / moveTime_, 0.0f, 1.0f) : 1.0f;
	firingCurrent_ = firingStart_ + (firingTarget_ - firingStart_) * rate;
}

void PlayerReticle::Draw() {
	if (!enabled_ || !initialized_) {
		return;
	}

	rects_.BeginFrame();

	// 照準レティクルは常に画面中央（仕様書 3.1 / 11.2）。
	// 補正が起きても動かさないことで、プレイヤーの操作基準がぶれない
	DrawCross(ScreenCenter(), aimSize_, aimColor_, 1.0f);

	// 発射レティクルはズレが出ているときだけ（仕様書 5.1 / 5.2）
	if (firingAlpha_ > 0.0f) {
		DrawCross(firingCurrent_, firingSize_, firingColor_, firingAlpha_);
	}
}

void PlayerReticle::Reset() {
	firingVisible_ = false;
	firingAlpha_ = 0.0f;
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

void PlayerReticle::DrawCross(const Vector2& center, float size, const Vector4& color, float alpha) {
	if (alpha <= 0.0f || size <= 0.0f || lineWidth_ <= 0.0f) {
		return;
	}

	// アウトラインを先に敷く。明るい線と同じ形をひと回り大きく暗い色で描くので、
	// 白い背景でも黒い背景でもレティクルの輪郭が消えない（仕様書 3.1 の「半透明のアウトライン」）
	if (outlineWidth_ > 0.0f && outlineAlpha_ > 0.0f) {
		const float grow = outlineWidth_ * 2.0f;
		const Vector4 outlineColor = {0.0f, 0.0f, 0.0f, outlineAlpha_ * alpha};
		rects_.Draw(center, Vector2{size + grow, lineWidth_ + grow}, outlineColor);
		rects_.Draw(center, Vector2{lineWidth_ + grow, size + grow}, outlineColor);
	}

	const Vector4 lineColor = {color.x, color.y, color.z, color.w * alpha};
	rects_.Draw(center, Vector2{size, lineWidth_}, lineColor); // 横棒
	rects_.Draw(center, Vector2{lineWidth_, size}, lineColor); // 縦棒
}
