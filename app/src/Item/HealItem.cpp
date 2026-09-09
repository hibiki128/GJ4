#include "HealItem.h"
#include "3d/Object/Base/BaseObjectManager.h"
#include "Frame/Frame.h"
#include <algorithm>
#include <cmath>
#include <numbers>

using namespace Hagine;

namespace {
// 中身のハート。mtl が空のモデルなので、見た目は下の白テクスチャと SetColor で決める
constexpr const char *kHeartModelPath = "heart/heart.obj";

// ハートも膜も、色をそのまま出したいので白テクスチャを貼る。
// プリミティブ生成時の既定は debug/uvChecker.png なので、生成後に必ず上書きする
constexpr const char *kWhiteTexturePath = "debug/white1x1.png";

// あと1発で割れるときの膜の濃さ（満タンのときに対する割合）。
// 薄くなっていくことで、あと何発かを撃ちながら読めるようにする
constexpr float kSealThinnestAlphaRate = 0.50f;

// ロックオンで強調しているときに、白へ寄せる割合
constexpr float kHighlightWhiteRate = 0.35f;

// 膜に当たった瞬間に白へ寄せる割合（そこから hitFlashTime かけて戻る）
constexpr float kHitFlashWhiteRate = 0.70f;

// 膜が弾けるときに、どれだけ膨らみながら消えるか
constexpr float kSealBurstScale = 0.35f;

// 膜が割れた瞬間に、ハートがどれだけ大きく跳ねるか
constexpr float kHeartPopScale = 0.35f;

/// <summary>色を白へ寄せる（明るさだけを上げたいので、アルファは触らない）</summary>
Vector4 ToWhite(const Vector4 &rgba, float rate) {
    return Vector4{rgba.x + (1.0f - rgba.x) * rate, rgba.y + (1.0f - rgba.y) * rate,
                   rgba.z + (1.0f - rgba.z) * rate, rgba.w};
}
} // namespace

void HealItem::InitItem(const std::string &objectName, const HealItemParams *params,
                        const HealItemHooks *hooks) {
    params_ = params;
    hooks_ = hooks;

    // --- 中身のハート（このオブジェクト自身） ---
    BaseObject::Init(objectName);
    CreateModel(kHeartModelPath);

    // mtl が空でテクスチャが割り当たらないモデルなので、白を貼って色を SetColor 側へ寄せる
    // （BossWarningMarker が同じ作りのモデルでやっているのと同じ手当て）
    SetTexture(kWhiteTexturePath);

    // 実行中に増減するので、シーンのJSONには残さない / ギズモの選択対象にもしない
    SetShouldSave(false);
    SetGizmoSelectable(false);

    // --- 包む膜（別オブジェクト） ---
    // 膜だけ加算合成にしたいので、ハートとは別のオブジェクトに分けている。
    // ハートの子にはせず、位置は毎フレーム自分で置く（膜は動かず、ハートだけが中で揺れるため）
    seal_ = std::make_unique<BaseObject>();
    seal_->Init(objectName + "_Seal");
    seal_->CreatePrimitiveModel(PrimitiveType::Sphere);
    seal_->SetTexture(kWhiteTexturePath);
    seal_->SetShouldSave(false);
    seal_->SetGizmoSelectable(false);

    // 向こう側が透ける光る殻に見せたいので、加算合成にして陰影を切る。
    // 通常ブレンドのままだとディファードの G-Buffer 側（不透明専用）へ回ってしまい、
    // 色のアルファが効かず、ただの黄色い玉になってしまう
    seal_->SetBlendMode(BlendMode::Add);
    seal_->GetLighting() = false;

    // 膜の実体はこのアイテムが持ったまま、更新と描画だけをエンジンへ任せる。
    // 登録解除は BaseObject のデストラクタが自動でやってくれる
    BaseObjectManager::GetInstance()->RegisterExternal(seal_.get());

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
    spinYaw_ = 0.0f;
    isHighlighted_ = false;

    transform_->translation_ = basePosition_;
    transform_->UpdateMatrix();

    SetIsAlive(true);
    SetIsModelDraw(true);

    ApplyVisual();
}

void HealItem::Deactivate() {
    state_ = State::None;
    isHighlighted_ = false;

    // 破棄はせず、描画だけ止めて次に出すまで待つ
    SetIsAlive(false);
    SetIsModelDraw(false);

    if (seal_) {
        seal_->SetIsAlive(false);
        seal_->SetIsModelDraw(false);
    }
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

    // 膜の中でハートを浮かせる。置いただけだと拾えるものだと気づきにくい
    bobPhase_ += params_->bobSpeed * deltaTime;
    spinYaw_ += params_->spinSpeed * (std::numbers::pi_v<float> / 180.0f) * deltaTime;

    transform_->translation_ =
        basePosition_ + Vector3{0.0f, std::sin(bobPhase_) * params_->bobHeight, 0.0f};
    transform_->SetRotationEuler(Vector3{0.0f, spinYaw_, 0.0f});

    ApplyVisual();

    // 拾えるのは膜が割れてから。実際に回復したときだけ消える
    if (state_ == State::Free && TryPickup()) {
        Deactivate();
        return;
    }

    BaseObject::Update();
}

bool HealItem::RaycastSeal(const Vector3 &from, const Vector3 &to, float bulletRadius,
                           float &outDistance, Vector3 &outPoint) const {
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
    // 膜は定位置に留まるので、判定の中心も出した位置そのままでよい。
    // 弾の太さは膜側へ足して解く（見た目どおりの太さで当たる）
    const float radius = params_->sealRadius + (std::max)(0.0f, bulletRadius);
    const Vector3 toStart = from - basePosition_;
    const float b = toStart.Dot(direction);
    const float c = toStart.LengthSq() - radius * radius;
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

    // 拾う相手はハートそのものなので、揺れている今の位置で見る
    const float reach = params_->coreRadius + params_->pickupRadius;
    if ((playerPosition - transform_->translation_).LengthSq() > reach * reach) {
        return false;
    }

    // 満タンで効かなかったときは false が返る。その場合はアイテムを消さずに残す
    // （効かない回復で消してしまうと、取り逃したように見えてしまうため）
    return hooks_->pickupHandler();
}

float HealItem::BurstProgress() const {
    if (state_ == State::Sealed) {
        return 0.0f; // まだ割れていない
    }
    if (params_->breakTime <= 0.0f) {
        return 1.0f;
    }
    return 1.0f - (breakTimer_ / params_->breakTime); // 割れた瞬間 0 から、消え切って 1
}

void HealItem::ApplyVisual() {
    const float burst = BurstProgress();

    // --- 中身のハート ---
    // 膜が割れた瞬間だけ大きく跳ねて、弾けるあいだに元の大きさへ収まる
    const float pop = (state_ == State::Free) ? kHeartPopScale * (1.0f - burst) : 0.0f;
    const float heartScale = params_->coreRadius * (1.0f + pop);
    transform_->scale_ = Vector3{heartScale, heartScale, heartScale};
    SetColor(params_->coreRgba);

    if (!seal_) {
        return;
    }

    // --- 包む膜 ---
    const bool sealVisible = (state_ == State::Sealed) || (burst < 1.0f);
    seal_->SetIsAlive(sealVisible);
    seal_->SetIsModelDraw(sealVisible);
    if (!sealVisible) {
        return;
    }

    // 膜は揺れず、出した位置に留まる。狙う的が動かないので照準が安定する
    float sealScale = params_->sealRadius * (1.0f + kSealBurstScale * burst);
    if (state_ == State::Sealed && isHighlighted_) {
        // ロックオン中は少し大きく見せる。狙えていることが画面で分かるようにするため
        sealScale *= (std::max)(params_->highlightScale, 1.0f);
    }

    WorldTransform *sealTransform = seal_->GetWorldTransform();
    sealTransform->translation_ = basePosition_;
    sealTransform->scale_ = Vector3{sealScale, sealScale, sealScale};
    sealTransform->UpdateMatrix();

    Vector4 rgba = params_->sealRgba;

    // 残りが減るほど薄くする。撃ちながら「あと何発か」を読めるようにするため。
    // 満タンで濃さそのまま、あと1発で kSealThinnestAlphaRate 倍
    if (params_->sealHitPoints > 1) {
        const float rate =
            static_cast<float>(sealHp_ - 1) / static_cast<float>(params_->sealHitPoints - 1);
        rgba.w *= kSealThinnestAlphaRate + (1.0f - kSealThinnestAlphaRate) * rate;
    }

    // 弾けているあいだは薄れて消える
    rgba.w *= (1.0f - burst);

    if (isHighlighted_) {
        rgba = ToWhite(rgba, kHighlightWhiteRate);
    }

    // 当たった瞬間だけ強く光らせて、そこから元へ戻す。
    // 薄くなるだけだと、当たったのか外れたのかが分かりにくい
    if (hitFlashTimer_ > 0.0f && params_->hitFlashTime > 0.0f) {
        rgba = ToWhite(rgba, kHitFlashWhiteRate * (hitFlashTimer_ / params_->hitFlashTime));
    }

    seal_->SetColor(rgba);
}
