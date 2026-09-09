#include "HealItem.h"
#include "Frame/Frame.h"
#include <algorithm>
#include <cmath>

using namespace Hagine;

namespace {
// 色をそのまま出したいので白テクスチャを貼る（色は SetColor 側で決める）。
// プリミティブ生成時の既定は debug/uvChecker.png なので、生成後に必ず上書きする
constexpr const char *kHealItemTexturePath = "debug/white1x1.png";

// あと1発で割れるときの膜の濃さ（満タンのときに対する割合）。
// 薄くなっていくことで、あと何発かを撃ちながら読めるようにする
constexpr float kSealThinnestAlphaRate = 0.50f;

// ロックオンで強調しているときに、白へ寄せる割合
constexpr float kHighlightWhiteRate = 0.35f;

// 膜に当たった瞬間に白へ寄せる割合（そこから hitFlashTime かけて戻る）
constexpr float kHitFlashWhiteRate = 0.70f;

/// <summary>色を白へ寄せる（明るさだけを上げたいので、アルファは触らない）</summary>
Hagine::Vector4 ToWhite(const Hagine::Vector4 &rgba, float rate) {
    return Hagine::Vector4{rgba.x + (1.0f - rgba.x) * rate, rgba.y + (1.0f - rgba.y) * rate,
                           rgba.z + (1.0f - rgba.z) * rate, rgba.w};
}
} // namespace

void HealItem::InitItem(const std::string &objectName, const HealItemParams *params,
                        const HealItemHooks *hooks) {
    params_ = params;
    hooks_ = hooks;

    BaseObject::Init(objectName);

    // CreatePrimitiveModel は内部で JSON を読み直してトランスフォームを上書きするため、
    // 大きさや色の設定は必ずこの後に行う（BossSphere::InitSphere と同じ理由）
    CreatePrimitiveModel(PrimitiveType::Sphere);
    SetTexture(kHealItemTexturePath);

    // 実行中に増減するので、シーンのJSONには残さない / ギズモの選択対象にもしない
    SetShouldSave(false);
    SetGizmoSelectable(false);

    // 生成直後は待機状態にしておく
    Deactivate();
}

void HealItem::Spawn(const Vector3 &position) {
    basePosition_ = position;
    state_ = State::Sealed;
    // 出したあとに調整値を変えても、いま出ている膜の固さは変わらないようにする
    sealHp_ = (std::max)(params_->sealHitPoints, 1);
    lifeTimer_ = params_->lifeTime;
    breakTimer_ = 0.0f;
    hitFlashTimer_ = 0.0f;
    bobPhase_ = 0.0f;
    isHighlighted_ = false;

    // 膜は「向こう側が透ける光る殻」に見せたいので、加算合成にして陰影を切る。
    // 通常ブレンドのままだとディファードの G-Buffer 側（不透明専用）へ回ってしまい、
    // 色のアルファが効かず、ただの黄色い玉になってしまう
    SetBlendMode(BlendMode::Add);
    GetLighting() = false;

    transform_->translation_ = basePosition_;
    transform_->UpdateMatrix();

    SetIsAlive(true);
    SetIsModelDraw(true);

    ApplyVisual();
}

void HealItem::Deactivate() {
    state_ = State::None;
    isHighlighted_ = false;
    SetIsAlive(false);

    // 破棄はせず、描画だけ止めて次に出すまで待つ
    SetIsModelDraw(false);
}

void HealItem::Update() {
    // 待機中のアイテムは動かさない（マネージャーには登録されたままなので毎フレーム呼ばれる）
    if (state_ == State::None) {
        return;
    }

    // ポーズ中は時間を進めない。揺れも寿命も止まったままにする
    if (isPaused_) {
        return;
    }

    const float deltaTime = Frame::DeltaTime();

    // 寿命。0以下なら消えない（拾うまで置いておきたいときの設定）
    if (params_->lifeTime > 0.0f) {
        lifeTimer_ -= deltaTime;
        if (lifeTimer_ <= 0.0f) {
            Deactivate();
            return;
        }
    }

    if (breakTimer_ > 0.0f) {
        breakTimer_ = (std::max)(breakTimer_ - deltaTime, 0.0f);
    }
    if (hitFlashTimer_ > 0.0f) {
        hitFlashTimer_ = (std::max)(hitFlashTimer_ - deltaTime, 0.0f);
    }

    // その場で上下に揺らす。地面に置いただけだと拾えるものだと気づきにくい
    bobPhase_ += params_->bobSpeed * deltaTime;
    transform_->translation_ =
        basePosition_ + Vector3{0.0f, std::sin(bobPhase_) * params_->bobHeight, 0.0f};

    ApplyVisual();

    // 拾えるのは膜が割れてから。実際に回復したときだけ消える
    if (state_ == State::Free && TryPickup()) {
        Deactivate();
        return;
    }

    BaseObject::Update();
}

bool HealItem::RaycastSeal(const Vector3 &from, const Vector3 &to, float &outDistance,
                           Vector3 &outPoint) const {
    if (state_ != State::Sealed) {
        return false;
    }

    const Vector3 segment = to - from;
    const float length = segment.Length();
    if (length <= 0.0001f) {
        return false;
    }
    const Vector3 direction = segment / length;

    // 線分と球の交差（BossSphereCluster::RaycastLocal と同じ解き方）。
    // 判定に使うのは描いている位置そのものなので、揺れているぶんもそのまま当たりに出る
    const Vector3 toStart = from - transform_->translation_;
    const float b = toStart.Dot(direction);
    const float c = toStart.LengthSq() - params_->sealRadius * params_->sealRadius;
    const float discriminant = b * b - c;
    if (discriminant < 0.0f) {
        return false;
    }

    const float root = std::sqrt(discriminant);
    float t = -b - root;
    if (t < 0.0f) {
        t = -b + root; // 始点が膜の内側にある場合
    }
    if (t < 0.0f || t > length) {
        return false;
    }

    outDistance = t;
    outPoint = from + direction * t;
    return true;
}

void HealItem::ApplySealHit() {
    if (state_ != State::Sealed) {
        return;
    }

    hitFlashTimer_ = params_->hitFlashTime;

    sealHp_ = (std::max)(sealHp_ - 1, 0);
    if (sealHp_ <= 0) {
        BreakSeal();
        return;
    }

    // まだ割れていない。薄くなった見た目をその場で反映する
    ApplyVisual();
}

void HealItem::BreakSeal() {
    state_ = State::Free;
    sealHp_ = 0;
    breakTimer_ = params_->breakTime;
    hitFlashTimer_ = 0.0f;
    isHighlighted_ = false;

    // 膜のあいだは光らせていたので、中身は普通の見た目へ戻す
    SetBlendMode(BlendMode::Normal);
    GetLighting() = true;

    ApplyVisual();
}

bool HealItem::TryPickup() {
    // 未配線なら拾えないだけ。アイテムはその場に出たままになる
    if (!hooks_->playerPositionGetter || !hooks_->pickupHandler) {
        return false;
    }

    Vector3 playerPosition{};
    if (!hooks_->playerPositionGetter(playerPosition)) {
        return false; // 拾い手がいない（倒れている等）
    }

    const float reach = params_->coreRadius + params_->pickupRadius;
    if ((playerPosition - transform_->translation_).LengthSq() > reach * reach) {
        return false;
    }

    // 満タンで効かなかったときは false が返る。その場合はアイテムを消さずに残す
    // （効かない回復で消してしまうと、取り逃したように見えてしまうため）
    return hooks_->pickupHandler();
}

float HealItem::CurrentRadius() const {
    if (state_ == State::Sealed) {
        return params_->sealRadius;
    }

    if (breakTimer_ <= 0.0f || params_->breakTime <= 0.0f) {
        return params_->coreRadius;
    }

    // 割れた直後は膜の大きさから中身の大きさへ縮む（1 → 0 で進む）
    const float rate = breakTimer_ / params_->breakTime;
    return params_->coreRadius + (params_->sealRadius - params_->coreRadius) * rate;
}

void HealItem::ApplyVisual() {
    float radius = CurrentRadius();

    // ロックオン中は少し大きく見せる。狙えていることが画面で分かるようにするため
    if (state_ == State::Sealed && isHighlighted_) {
        radius *= (std::max)(params_->highlightScale, 1.0f);
    }
    transform_->scale_ = Vector3{radius, radius, radius};

    if (state_ != State::Sealed) {
        SetColor(params_->coreRgba);
        return;
    }

    Vector4 rgba = params_->sealRgba;

    // 残りが減るほど薄くする。撃ちながら「あと何発か」を読めるようにするため。
    // 満タンで濃さそのまま、あと1発で kSealThinnestAlphaRate 倍
    if (params_->sealHitPoints > 1) {
        const float rate = static_cast<float>(sealHp_ - 1) /
                           static_cast<float>(params_->sealHitPoints - 1);
        rgba.w *= kSealThinnestAlphaRate + (1.0f - kSealThinnestAlphaRate) * rate;
    }

    if (isHighlighted_) {
        rgba = ToWhite(rgba, kHighlightWhiteRate);
    }

    // 当たった瞬間だけ強く光らせて、そこから元へ戻す。
    // 薄くなるだけだと、当たったのか外れたのかが分かりにくい
    if (hitFlashTimer_ > 0.0f && params_->hitFlashTime > 0.0f) {
        rgba = ToWhite(rgba, kHitFlashWhiteRate * (hitFlashTimer_ / params_->hitFlashTime));
    }

    SetColor(rgba);
}
