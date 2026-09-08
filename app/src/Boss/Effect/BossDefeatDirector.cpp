#include "BossDefeatDirector.h"
#include "MyMath.h"
#include "camera/Camera.h"
#include "camera/CameraManager.h"
#include "src/Boss/Data/BossEasing.h"
#include "window/WinApp.h"
#include <algorithm>
#include <cmath>
#include <numbers>

using namespace Hagine;

namespace {

/// <summary>黒帯に使うテクスチャ（白い1x1を黒く塗って使う）</summary>
constexpr const char *kBarTexturePath = "debug/white1x1.png";

} // namespace

void BossDefeatDirector::Init() {
    if (topBar_) {
        return;
    }

    // 画面いっぱいの帯を2枚。位置と大きさは演出中に毎フレーム決める
    topBar_ = std::make_unique<Sprite>();
    topBar_->Initialize(kBarTexturePath, Vector2{0.0f, 0.0f});
    topBar_->SetColor(Vector3{0.0f, 0.0f, 0.0f});

    bottomBar_ = std::make_unique<Sprite>();
    bottomBar_->Initialize(kBarTexturePath, Vector2{0.0f, 0.0f});
    bottomBar_->SetColor(Vector3{0.0f, 0.0f, 0.0f});

    pCamera_ = CameraManager::GetInstance()->Create("BossDefeatCamera");
}

void BossDefeatDirector::Begin(const Vector3 &focusPoint, const Vector3 &from, bool holdPosition,
                              float holdDistance, float holdHeight) {
    (void)focusPoint;
    isActive_ = true;
    isReturning_ = false;
    elapsed_ = 0.0f;
    returnElapsed_ = 0.0f;
    cameraFrom_ = from;
    isHoldPosition_ = holdPosition;
    holdDistance_ = holdDistance;
    // 地面に沈み込まないよう、わずかに浮かせておく
    holdHeight_ = (std::max)(0.2f, holdHeight);

    // ここから演出用カメラで描く。戻すのは呼び出し側（元のカメラを知っているのはそちら）
    if (pCamera_) {
        pCamera_->SetPosition(cameraFrom_);
        CameraManager::GetInstance()->SetActive(pCamera_);
    }
}

void BossDefeatDirector::Update(float deltaTime, const Vector3 &focusPoint, float facingYaw,
                                const BossSpiderDefeatParams &params) {
    if (!isActive_) {
        return;
    }
    elapsed_ += deltaTime;

    // --- 上下の黒帯を閉じる ---
    const float screenWidth = static_cast<float>(WinApp::GetVirtualWidth());
    const float screenHeight = static_cast<float>(WinApp::GetVirtualHeight());
    const float barProgress = std::clamp(elapsed_ / (std::max)(0.01f, params.barTime), 0.0f, 1.0f);
    const float barHeight = screenHeight * std::clamp(params.barRatio, 0.0f, 0.5f) * SmoothInOut(barProgress);

    topBar_->SetPosition(Vector2{0.0f, 0.0f});
    topBar_->SetSize(Vector2{screenWidth, barHeight});
    bottomBar_->SetPosition(Vector2{0.0f, screenHeight - barHeight});
    bottomBar_->SetSize(Vector2{screenWidth, barHeight});
    // 位置と大きさを入れるだけでよい（頂点の作り直しは Draw が行う）

    if (!pCamera_) {
        return;
    }

    // --- カメラをコアの正面へ回り込ませる ---
    // 敵が向いている向きの先に立つので、寄りきったところで敵と向き合う構図になる。
    // 真正面だと平板に見えるので、focusYawOffset ぶんだけ横へずらせるようにしてある
    const float angle = facingYaw + params.focusYawOffset * (std::numbers::pi_v<float> / 180.0f);
    const Vector3 front{std::cos(angle), 0.0f, std::sin(angle)};

    Vector3 focusTarget = focusPoint + front * params.focusDistance +
                          Vector3{0.0f, params.focusHeight, 0.0f};
    if (isHoldPosition_) {
        // 登場演出では座標を動かさない。地面に据えたまま、起き上がるコアを見上げ続ける。
        // 変形でコアが大きくなるぶん、寄り先も通常より離しておく
        if (holdPosition_.LengthSq() <= 0.0001f) {
            holdPosition_ = Vector3{focusPoint.x + front.x * holdDistance_, holdHeight_,
                                    focusPoint.z + front.z * holdDistance_};
        }
        focusTarget = holdPosition_;
    }
    const float focusProgress = std::clamp(elapsed_ / (std::max)(0.01f, params.focusTime), 0.0f, 1.0f);
    Vector3 position = Lerp(cameraFrom_, focusTarget, SmoothInOut(focusProgress));

    // 手持ちのような、ゆっくりした揺れ。上下と左右で周期をずらして規則的に見せない
    const float sway = params.handheldAmount;
    position.x += std::sin(elapsed_ * params.handheldSpeed) * sway;
    position.y += std::sin(elapsed_ * params.handheldSpeed * 1.7f) * sway * 0.6f;
    position.z += std::cos(elapsed_ * params.handheldSpeed * 0.8f) * sway;

    pCamera_->SetPosition(position);
    lastLookAt_ = focusPoint + Vector3{0.0f, params.lookHeight, 0.0f};
    pCamera_->SetTarget(lastLookAt_);
    pCamera_->Update();
}

void BossDefeatDirector::Draw() {
    if (!isActive_ || !topBar_) {
        return;
    }
    topBar_->Draw();
    bottomBar_->Draw();
}

void BossDefeatDirector::Stop() {
    isActive_ = false;
    isReturning_ = false;
    elapsed_ = 0.0f;
    returnElapsed_ = 0.0f;
    holdPosition_ = Vector3{};
}

void BossDefeatDirector::BeginReturn() {
    if (!isActive_ || isReturning_) {
        return;
    }
    isReturning_ = true;
    returnElapsed_ = 0.0f;
    returnFrom_ = pCamera_ ? pCamera_->GetViewProjection().translation_ : Vector3{};
    returnLookFrom_ = lastLookAt_;
}

bool BossDefeatDirector::UpdateReturn(float deltaTime, const Vector3 &cameraTo, const Vector3 &lookTo,
                                      const BossSpiderDefeatParams &params) {
    if (!isActive_ || !isReturning_) {
        return false;
    }
    returnElapsed_ += deltaTime;
    const float progress = std::clamp(returnElapsed_ / (std::max)(0.01f, params.returnTime), 0.0f, 1.0f);
    const float eased = SmoothInOut(progress);

    // 黒帯は逆に開いていく
    const float screenWidth = static_cast<float>(WinApp::GetVirtualWidth());
    const float screenHeight = static_cast<float>(WinApp::GetVirtualHeight());
    const float barHeight = screenHeight * std::clamp(params.barRatio, 0.0f, 0.5f) * (1.0f - eased);
    topBar_->SetPosition(Vector2{0.0f, 0.0f});
    topBar_->SetSize(Vector2{screenWidth, barHeight});
    bottomBar_->SetPosition(Vector2{0.0f, screenHeight - barHeight});
    bottomBar_->SetSize(Vector2{screenWidth, barHeight});

    // カメラは位置も注視点も滑らかに戻す（両端で速度0なので繋ぎ目が出ない）
    if (pCamera_) {
        pCamera_->SetPosition(Lerp(returnFrom_, cameraTo, eased));
        pCamera_->SetTarget(Lerp(returnLookFrom_, lookTo, eased));
        pCamera_->Update();
    }

    if (progress < 1.0f) {
        return false;
    }
    Stop();
    return true;
}
