#include "BossSpider.h"
#include "Easing.h"
#include "MyMath.h"
#include "Random.h"
#include "src/Boss/Effect/BossParticles.h"
#include "src/Boss/Spider/Attack/BossSpiderAttackLeap.h"
#include "src/Boss/Spider/Attack/BossSpiderAttackShoot.h"
#include "src/Boss/Spider/Attack/BossSpiderAttackWhirl.h"
#include "debug/imgui/ImGuiNotification.h"
#include "frame/Frame.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

using namespace Hagine;

namespace {

/// <summary>浮き上がりが何割進んだところで脚が生え始めるか</summary>
constexpr float kGrowStartRatio = 0.55f;

/// <summary>脚が何割生えたところで関節が折れ始めるか</summary>
constexpr float kBendStartRatio = 0.70f;

/// <summary>撃破の最後、ふくらみに使う割合（残りはしぼんで消えるのに使う）</summary>
constexpr float kBurstSwellRatio = 0.3f;

} // namespace

#ifdef USE_IMGUI
namespace {

/// <summary>直前の項目の右に「(?)」を出し、マウスを乗せたときだけ説明を見せる</summary>
void HelpMarker(const char *description) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) && ImGui::BeginTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
        ImGui::TextUnformatted(description);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

} // namespace
#endif // USE_IMGUI

void BossSpider::Init(const std::string objectName) {
    BaseObject::Init(objectName);

    // 胴は球体形態の中心と同じ、暗い球
    CreateModel("boss/boss.obj");
    SetTexture(kBossTexturePath);
    SetShouldSave(false);

    palette_.LoadMaster();

    // 保存済みの値があればそれで組む（無ければ既定値のまま）
    LoadSpiderParams(bossId_, parameters_);

    legNamePrefix_ = objectName + "Leg";
    RebuildLegs();

    // 使える攻撃を登録する。並び順は kAttackLeap などの定数と合わせること
    attacks_.push_back(std::make_unique<BossSpiderAttackLeap>(&parameters_.attack.leap));
    attacks_.push_back(std::make_unique<BossSpiderAttackShoot>(&parameters_.attack.shoot));
    attacks_.push_back(std::make_unique<BossSpiderAttackWhirl>(&parameters_.attack.whirl));

    Hide();
}

void BossSpider::RebuildLegs() {
    const int legCount = std::clamp(parameters_.legCount, 1, kMaxLegCount);

    // 脚は破棄せず、足りなければ足すだけにする。
    // 実行中に D3D12 リソースを解放すると、前フレームのGPUコマンドがまだそれを
    // 参照していて「使用中リソースの解放」で落ちる（エンジンはシーン遷移時にだけ
    // GPU 完了を待ってから破棄している）
    while (static_cast<int>(legs_.size()) < legCount) {
        legs_.push_back(std::make_unique<BossSpiderLeg>());
    }

    for (int index = 0; index < static_cast<int>(legs_.size()); ++index) {
        BossSpiderLeg *leg = legs_[static_cast<size_t>(index)].get();
        if (index < legCount) {
            leg->Configure(legNamePrefix_, index, legCount, parameters_, palette_);
            leg->ResetFoot(bodyPosition_, bodyYaw_, parameters_);
        } else {
            leg->SetHidden(true); // 本数を減らしたぶんは隠すだけ
        }
    }
    activeLegCount_ = legCount;

    // 胴の大きさもパラメータへ合わせる
    transform_->scale_ = Vector3{parameters_.bodyRadius, parameters_.bodyRadius, parameters_.bodyRadius};
    transform_->UpdateMatrix();
    SetColor(Vector4{0.16f, 0.16f, 0.20f, 1.0f});

    // Configure は組み直した脚を必ず出す状態にするので、
    // まだ出ていない（変形前の）ときは隠し直す。
    // これをしないと、調整UIで大きさを触っただけで脚が胴のところに現れる
    if (phase_ == Phase::Hidden) {
        for (int index = 0; index < activeLegCount_; ++index) {
            legs_[static_cast<size_t>(index)]->SetHidden(true);
        }
    }
}

void BossSpider::LoadParameters() {
    LoadSpiderParams(bossId_, parameters_);
    RebuildLegs();
}

void BossSpider::SaveParameters() const {
    SaveSpiderParams(bossId_, parameters_);
}

void BossSpider::Awaken(const Vector3 &corePosition, float coreRadius) {
    // 球体形態のコアの位置・大きさをそのまま引き継ぐ。
    // 引き渡した側が同じフレームでコアを消すので、見た目は同じ球が変形し続けたように見える
    bodyPosition_ = corePosition;
    startHeight_ = corePosition.y;
    startRadius_ = (coreRadius > 0.0f) ? coreRadius : parameters_.bodyRadius;
    walkPhase_ = 0.0f;

    // 相手がいるならそちらを向いて起き上がる
    if (pTargetLocator_ && pTargetLocator_->IsTargetValid()) {
        const Vector3 toTarget = pTargetLocator_->GetTargetPosition() - bodyPosition_;
        if (toTarget.x * toTarget.x + toTarget.z * toTarget.z > 0.0001f) {
            bodyYaw_ = std::atan2(toTarget.z, toTarget.x);
        }
    }

    transform_->translation_ = bodyPosition_;
    transform_->quaternionRotation_ = Quaternion::FromAxisAngle(kWorldUp, bodyYaw_);
    // 引き継いだ大きさから始める（変形のあいだに蜘蛛の胴の大きさへ寄せる）
    transform_->scale_ = Vector3{startRadius_, startRadius_, startRadius_};
    transform_->UpdateMatrix();

    // 足の着地点は先に決めておく。脚が生えきったあと、ここへ向けて関節が曲がる
    for (int index = 0; index < activeLegCount_; ++index) {
        legs_[static_cast<size_t>(index)]->SetHidden(false);
        legs_[static_cast<size_t>(index)]->ResetFoot(bodyPosition_, bodyYaw_, parameters_);
    }

    // 立ったときの高さは、置いた足の高さから決める（足の球が地面に乗る）
    standHeight_ = CalcFootAverageHeight() + parameters_.bodyHeight;

    phase_ = Phase::Collapse;
    collapseTime_ = 0.0f;
    staggerTimer_ = 0.0f;
    transformTime_ = 0.0f;
    SetIsAlive(true);
    SetIsModelDraw(true);
    // まだ1本も生えていない状態から始める
    PlaceLegs(bodyPosition_, 0.0f, 0.0f);
}

void BossSpider::Hide() {
    // 進行中の攻撃と飛んでいる弾を片付ける
    if (pCurrentAttack_) {
        BossAttackContext context{};
        context.spider = this;
        context.target = pTargetLocator_;
        pCurrentAttack_->Cancel(context);
        pCurrentAttack_ = nullptr;
    }
    for (SpiderBullet &bullet : bullets_) {
        bullet.active = false;
        bullet.sphere->Deactivate();
    }
    SetLegTuck(0.0f, 0.01f);
    SetLegBend(1.0f, 0.01f);
    legTuck_ = 0.0f;
    legBend_ = 1.0f;

    phase_ = Phase::Hidden;
    transformTime_ = 0.0f;
    staggerTimer_ = 0.0f;
    for (int index = 0; index < activeLegCount_; ++index) {
        legs_[static_cast<size_t>(index)]->SetHidden(true);
    }
    SetIsAlive(false);
    SetIsModelDraw(false);
}

const char *BossSpider::GetPhaseName() const {
    switch (phase_) {
    case Phase::Collapse:
        return "崩れ落ちている";
    case Phase::Transform:
        return "変形中";
    case Phase::Active:
        return "戦闘中";
    case Phase::Defeated:
        return isDefeatFinished_ ? "撃破（演出おわり）" : "撃破演出中";
    default:
        return "未出現";
    }
}

void BossSpider::SkipTransform() {
    phase_ = Phase::Active;
    transformTime_ = 0.0f;
    attackCoolDown_ = (std::max)(0.0f, parameters_.attack.firstDelay);
    bodyPosition_.y = standHeight_;
    transform_->translation_ = bodyPosition_;
    transform_->scale_ = Vector3{parameters_.bodyRadius, parameters_.bodyRadius, parameters_.bodyRadius};
    transform_->UpdateMatrix();
    PlaceLegs(bodyPosition_);
}

float BossSpider::CalcGrowStartTime() const {
    // 浮き上がりきる前に脚が生え始める
    return parameters_.riseTime * kGrowStartRatio;
}

float BossSpider::CalcBendStartTime() const {
    // 生えきる前に関節が折れ始める
    return CalcGrowStartTime() + parameters_.growTime * kBendStartRatio;
}

float BossSpider::CalcTransformDuration() const {
    return CalcBendStartTime() + parameters_.landTime;
}


void BossSpider::UpdateCollapse(float deltaTime) {
    collapseTime_ += deltaTime;

    // 殻を失ったコアが、支えを無くしてふらつきながら地面へ落ちる。
    // ここで一度「終わった」ように見せてから起き上がるので、変形が引き立つ
    const float duration = (std::max)(0.01f, parameters_.collapseTime);
    const float progress = std::clamp(collapseTime_ / duration, 0.0f, 1.0f);
    const float groundHeight = parameters_.bodyRadius;
    bodyPosition_.y = ApplyEasing(EasingType::InQuad, startHeight_, groundHeight, progress, 1.0f);

    // ふらつきは落ちるにつれて収まる
    const float decay = 1.0f - progress;
    const float sway = std::sin(collapseTime_ * parameters_.collapseSwaySpeed) * parameters_.collapseSway * decay;
    const Vector3 right{std::cos(bodyYaw_ + std::numbers::pi_v<float> * 0.5f), 0.0f,
                        std::sin(bodyYaw_ + std::numbers::pi_v<float> * 0.5f)};

    transform_->translation_ = bodyPosition_ + right * sway;
    transform_->quaternionRotation_ =
        Quaternion::FromAxisAngle(kWorldUp, bodyYaw_) *
        Quaternion::FromAxisAngle(Vector3{0.0f, 0.0f, 1.0f}, sway * 0.35f);
    transform_->scale_ = Vector3{startRadius_, startRadius_, startRadius_};
    transform_->UpdateMatrix();

    // 脚はまだ胴の中。1本も出さない
    PlaceLegs(bodyPosition_, 0.0f, 0.0f);

    if (collapseTime_ < duration + parameters_.collapseRest) {
        return;
    }

    // 落ちた場所から起き上がる。ここから先はいつもの変形
    startHeight_ = bodyPosition_.y;
    phase_ = Phase::Transform;
    transformTime_ = 0.0f;
}

void BossSpider::UpdateTransform(float deltaTime) {
    transformTime_ += deltaTime;

    // 3つの動きを別々の窓で少しずつ重ねて進める。
    // 「浮き上がる → 止まる → 生える → 止まる → 折れる」と切り分けると、
    // ひとつ終わるたびに動きが止まって、次が前の動きの巻き戻しに見えてしまう
    const float riseProgress =
        std::clamp(transformTime_ / (std::max)(0.01f, parameters_.riseTime), 0.0f, 1.0f);
    const float growProgress = std::clamp((transformTime_ - CalcGrowStartTime()) /
                                              (std::max)(0.01f, parameters_.growTime),
                                          0.0f, 1.0f);
    const float bendProgress = std::clamp((transformTime_ - CalcBendStartTime()) /
                                              (std::max)(0.01f, parameters_.landTime),
                                          0.0f, 1.0f);

    // 胴は立つ高さまで一度上がるだけで、あとは下りない（下ろすと巻き戻しに見える）。
    // 足を地面へ着けるのは脚の関節だけが受け持つ
    const float rise = SmoothInOut(riseProgress);
    bodyPosition_.y = Lerp(startHeight_, standHeight_, rise);

    // 引き継いだコアの大きさから、蜘蛛の胴の大きさへ寄せる。
    // 同じ球がそのまま変形したように見せるため、位置と一緒に大きさも繋ぐ
    const float radius = Lerp(startRadius_, parameters_.bodyRadius, rise);
    transform_->scale_ = Vector3{radius, radius, radius};

    transform_->translation_ = bodyPosition_;
    transform_->quaternionRotation_ = Quaternion::FromAxisAngle(kWorldUp, bodyYaw_);

    transform_->UpdateMatrix();

    const float bend = SmoothInOut(bendProgress);
    PlaceLegs(bodyPosition_, growProgress, bend);

    if (transformTime_ >= CalcTransformDuration()) {
        phase_ = Phase::Active;
        transformTime_ = 0.0f;
        // 変形の余韻を邪魔しないよう、最初の攻撃までは間を置く
        attackCoolDown_ = (std::max)(0.0f, parameters_.attack.firstDelay);
    }
}


int BossSpider::GetAliveLegCount() const {
    int alive = 0;
    for (int index = 0; index < activeLegCount_; ++index) {
        // 球が1つも残っていない脚は無くなったものとして数えない
        if (legs_[static_cast<size_t>(index)]->GetSphereCount() > 0) {
            ++alive;
        }
    }
    return alive;
}

void BossSpider::BeginDefeat() {
    if (phase_ == Phase::Defeated) {
        return;
    }
    // 進行中の攻撃は打ち切る（撃破後に攻撃が続くと締まらない）
    if (pCurrentAttack_) {
        BossAttackContext context{};
        context.spider = this;
        context.target = pTargetLocator_;
        pCurrentAttack_->Cancel(context);
        pCurrentAttack_ = nullptr;
    }
    for (SpiderBullet &bullet : bullets_) {
        bullet.active = false;
        bullet.sphere->Deactivate();
    }

    // 脚が残っていれば、ここで丸ごと切り落とす。
    // ImGui から始めたときも、実際に脚を壊し切ったときと同じ絵になる
    for (int index = 0; index < activeLegCount_; ++index) {
        legs_[static_cast<size_t>(index)]->SeverAll(parameters_);
    }

    phase_ = Phase::Defeated;
    defeatTime_ = 0.0f;
    defeatStartHeight_ = bodyPosition_.y;
    isDefeatFinished_ = false;

    // 前回の演出で消えたままになっていることがあるので、必ず出し直す
    transform_->scale_ = Vector3{parameters_.bodyRadius, parameters_.bodyRadius, parameters_.bodyRadius};
    transform_->UpdateMatrix();
    SetIsModelDraw(true);
}

void BossSpider::UpdateDefeat(float deltaTime) {
    const BossSpiderDefeatParams &defeat = parameters_.defeat;
    defeatTime_ += deltaTime;

    // 支えを失ってふらつきながら落ちる。落ち始めはゆっくりで、
    // 最後は重さに負けて一気に地面へ落ちる
    const float fallProgress = std::clamp(defeatTime_ / (std::max)(0.01f, defeat.fallTime), 0.0f, 1.0f);
    const float groundHeight = parameters_.bodyRadius;
    const float height = ApplyEasing(EasingType::InQuad, defeatStartHeight_, groundHeight, fallProgress, 1.0f);

    // ふらつきは落ちるにつれて収まる（地面に着いたら止まる）
    const float swayDecay = 1.0f - fallProgress;
    const float sway = std::sin(defeatTime_ * defeat.swaySpeed) * defeat.swayAmount * swayDecay;
    const float swaySide = std::cos(defeatTime_ * defeat.swaySpeed * 0.7f) * defeat.swayAmount * swayDecay;

    const Vector3 right{std::cos(bodyYaw_ + std::numbers::pi_v<float> * 0.5f), 0.0f,
                        std::sin(bodyYaw_ + std::numbers::pi_v<float> * 0.5f)};
    const Vector3 forward{std::cos(bodyYaw_), 0.0f, std::sin(bodyYaw_)};

    bodyPosition_.y = height;
    const Vector3 renderPosition = bodyPosition_ + right * sway + forward * swaySide;

    transform_->translation_ = renderPosition;
    // ふらつきに合わせて傾く。倒れ込んでいくように見せる
    const float tilt = defeat.tiltAngle * (std::numbers::pi_v<float> / 180.0f);
    transform_->quaternionRotation_ =
        Quaternion::FromAxisAngle(kWorldUp, bodyYaw_) *
        Quaternion::FromAxisAngle(Vector3{0.0f, 0.0f, 1.0f}, sway * tilt) *
        Quaternion::FromAxisAngle(Vector3{1.0f, 0.0f, 0.0f}, swaySide * tilt);
    transform_->UpdateMatrix();

    // 残っている脚も一緒に落ちる（胴について回るだけ）
    PlaceLegs(renderPosition, 1.0f, legBend_);
    for (int index = 0; index < activeLegCount_; ++index) {
        legs_[static_cast<size_t>(index)]->UpdateMotions(deltaTime, parameters_);
    }

    // --- 落ちて間を置いたあと、ひとふくらみしてから消える ---
    const float burstStart = defeat.fallTime + defeat.restTime;
    if (defeatTime_ < burstStart) {
        return;
    }

    const float burstDuration = (std::max)(0.01f, defeat.burstTime);
    const float burstProgress = std::clamp((defeatTime_ - burstStart) / burstDuration, 0.0f, 1.0f);

    // 前半で素早くふくらみ（OutQuad）、後半でためてから一気にしぼむ（InCubic）。
    // 完全な0スケールは行列が潰れて法線が壊れるので、ごく小さい値で止めてから消す
    float scale = 0.0f;
    if (burstProgress < kBurstSwellRatio) {
        scale = ApplyEasing(EasingType::OutQuad, 1.0f, defeat.burstScale,
                            burstProgress / kBurstSwellRatio, 1.0f);
    } else {
        scale = ApplyEasing(EasingType::InCubic, defeat.burstScale, 0.0f,
                            (burstProgress - kBurstSwellRatio) / (1.0f - kBurstSwellRatio), 1.0f);
    }
    const float radius = (std::max)(parameters_.bodyRadius * 0.001f, parameters_.bodyRadius * scale);
    transform_->scale_ = Vector3{radius, radius, radius};
    transform_->UpdateMatrix();

    if (burstProgress < 1.0f) {
        return;
    }

    if (isDefeatFinished_) {
        return;
    }

    // 消えきった。ここがシーン遷移の起点になる
    SetIsModelDraw(false);
    isDefeatFinished_ = true;
    // しぼみ切るのに合わせて破片を散らす。消える瞬間がはじけたように見える
    BossParticles::GetInstance()->Burst(BossParticles::Id::DefeatBurst, renderPosition);
}

void BossSpider::Update() {
    BaseObject::Update();

    if (phase_ == Phase::Hidden || isPaused_) {
        return;
    }

    const float deltaTime = Frame::DeltaTime();

    if (phase_ == Phase::Defeated) {
        UpdateDefeat(deltaTime);
        return;
    }

    if (phase_ == Phase::Collapse) {
        UpdateCollapse(deltaTime);
        return;
    }

    // 変形が終わるまでは歩かない
    if (phase_ != Phase::Active) {
        UpdateTransform(deltaTime);
        return;
    }

    // 脚をすべて壊されたら撃破
    if (GetAliveLegCount() <= 0) {
        BeginDefeat();
        return;
    }

    // 攻撃中は攻撃が胴を動かす。攻撃していなければ相手へ歩いて寄る
    // 脚の姿勢（畳み・折り）を先に進める。足の位置がこれで決まる
    UpdateLegPosture(deltaTime);

    // 攻撃中は攻撃が胴を動かす。攻撃していなければ相手へ歩いて寄る
    const Vector3 moveDirection = UpdateAttack(deltaTime);

    // 歩きでも突進でも、胴はフィールドの外へ出さない。脚を並べるより前に収めるので、
    // 足の置き場所も押し戻したあとの胴から決まる（脚だけ外へ残らない）
    if (pFieldBounds_) {
        pFieldBounds_->ClampToField(bodyPosition_);
    }

    UpdateLegs(moveDirection, deltaTime);

    // 胴の高さと揺れを決めてから脚を並べる。逆にすると脚の付け根が1フレーム前の胴を
    // 参照することになり、揺れるたびに胴と脚がずれて見える
    const bool isAttacking = (pCurrentAttack_ != nullptr);
    const Vector3 renderPosition =
        UpdateBodyPosture(deltaTime, !isAttacking && moveDirection.LengthSq() > 0.0001f, !isAttacking);
    PlaceLegs(renderPosition, 1.0f, legBend_);

    // 並べ直したあとに演出を進める。逆にすると、くっついた球の吸い寄せも
    // 消滅の押し出しも、並べ直しで毎フレーム打ち消されてしまう
    for (int index = 0; index < activeLegCount_; ++index) {
        legs_[static_cast<size_t>(index)]->UpdateMotions(deltaTime, parameters_);
    }

    // 撃った弾を進める（攻撃が終わっても飛び続ける）
    UpdateBullets(deltaTime);
}

Vector3 BossSpider::CalcDesiredDirection() const {
    if (!pTargetLocator_ || !pTargetLocator_->IsTargetValid()) {
        return Vector3{0.0f, 0.0f, 0.0f};
    }

    Vector3 toTarget = pTargetLocator_->GetTargetPosition() - bodyPosition_;
    toTarget.y = 0.0f;

    // 近づきすぎたら止まる（相手を押しのけないように）
    const float distance = toTarget.Length();
    if (distance < parameters_.stopDistance) {
        return Vector3{0.0f, 0.0f, 0.0f};
    }
    return toTarget / distance;
}

Vector3 BossSpider::UpdateBodyMove(float deltaTime) {
    const Vector3 desired = CalcDesiredDirection();
    if (desired.LengthSq() <= 0.0001f) {
        return Vector3{0.0f, 0.0f, 0.0f};
    }

    // 向きは滑らかに寄せる（急に振り向かせない）。
    // 角度差を -π〜π に収めてから、1フレームぶんだけ回す
    const float targetYaw = std::atan2(desired.z, desired.x);
    const float turnStep = parameters_.turnSpeed * (std::numbers::pi_v<float> / 180.0f) * deltaTime;
    float difference = targetYaw - bodyYaw_;
    while (difference > std::numbers::pi_v<float>) {
        difference -= 2.0f * std::numbers::pi_v<float>;
    }
    while (difference < -std::numbers::pi_v<float>) {
        difference += 2.0f * std::numbers::pi_v<float>;
    }
    bodyYaw_ += std::clamp(difference, -turnStep, turnStep);

    // 向いている方向へ進む
    const Vector3 forward{std::cos(bodyYaw_), 0.0f, std::sin(bodyYaw_)};
    bodyPosition_ += forward * (parameters_.moveSpeed * deltaTime);

    walkPhase_ += deltaTime * parameters_.moveSpeed;
    return forward;
}

void BossSpider::UpdateLegs(const Vector3 &moveDirection, float deltaTime) {
    const int legCount = activeLegCount_;
    if (legCount <= 0) {
        return;
    }

    for (int index = 0; index < legCount; ++index) {
        // 隣り合う脚が浮いていたら、この脚は接地させておく。
        // これだけで「対角の脚が交互に出る」蜘蛛らしい歩容になる
        const int previous = (index - 1 + legCount) % legCount;
        const int next = (index + 1) % legCount;
        const bool neighborStepping = legs_[static_cast<size_t>(previous)]->IsStepping() ||
                                      legs_[static_cast<size_t>(next)]->IsStepping();

        legs_[static_cast<size_t>(index)]->Update(bodyPosition_, bodyYaw_, moveDirection,
                                                  parameters_, !neighborStepping, deltaTime);
    }
}

float BossSpider::CalcFootAverageHeight() const {
    if (activeLegCount_ <= 0) {
        return parameters_.legSphereRadius;
    }
    float sum = 0.0f;
    for (int index = 0; index < activeLegCount_; ++index) {
        sum += legs_[static_cast<size_t>(index)]->GetFootPosition().y;
    }
    return sum / static_cast<float>(activeLegCount_);
}

Vector3 BossSpider::UpdateBodyPosture(float deltaTime, bool isMoving, bool controlHeight) {
    (void)deltaTime;

    // 足の平均の高さに胴を乗せる（脚が持ち上がると胴もわずかに上がる）
    const float averageFootHeight = CalcFootAverageHeight();

    // 歩調に合わせた上下と左右の揺れ。止まっているときは揺らさない
    const float bob = isMoving ? std::sin(walkPhase_ * 3.0f) * parameters_.bodyBob : 0.0f;
    const float sway = isMoving ? std::sin(walkPhase_ * 1.5f) * parameters_.bodySway : 0.0f;

    const Vector3 right{std::cos(bodyYaw_ + std::numbers::pi_v<float> * 0.5f), 0.0f,
                        std::sin(bodyYaw_ + std::numbers::pi_v<float> * 0.5f)};

    // 攻撃中は胴の高さを攻撃が決めているので、ここでは触らない
    if (controlHeight) {
        bodyPosition_.y = averageFootHeight + parameters_.bodyHeight;
    }

    const Vector3 renderPosition = bodyPosition_ + right * sway + Vector3{0.0f, bob, 0.0f};

    transform_->translation_ = renderPosition;
    // 揺れに合わせて胴をわずかに傾ける（歩くたびに body が軋むように見える）
    transform_->quaternionRotation_ =
        Quaternion::FromAxisAngle(kWorldUp, bodyYaw_) *
        Quaternion::FromAxisAngle(Vector3{0.0f, 0.0f, 1.0f}, sway * 0.35f);
    transform_->UpdateMatrix();

    return renderPosition;
}

void BossSpider::PlaceLegs(const Vector3 &bodyPosition, float growth, float bend) {
    // 一斉に生えると作り物っぽいので、隣り合う脚で生え始めをずらす
    const float stagger = std::clamp(parameters_.growStagger, 0.0f, 0.9f);
    const float span = (std::max)(0.01f, 1.0f - stagger);

    for (int index = 0; index < activeLegCount_; ++index) {
        const float delay = stagger * static_cast<float>(index % 2);
        const float legGrowth =
            (growth >= 1.0f) ? 1.0f : std::clamp((growth - delay) / span, 0.0f, 1.0f);
        legs_[static_cast<size_t>(index)]->PlacePose(bodyPosition, bodyYaw_, parameters_, legGrowth, bend);
    }
}



void BossSpider::ReportHit(const Vector3 &center, float radius, float damage) {
    if (hitCallback_) {
        hitCallback_(center, radius, damage);
    }
}

void BossSpider::FaceTowards(const Vector3 &worldPoint) {
    Vector3 toPoint = worldPoint - bodyPosition_;
    toPoint.y = 0.0f;
    if (toPoint.LengthSq() > 0.0001f) {
        bodyYaw_ = std::atan2(toPoint.z, toPoint.x);
    }
}

void BossSpider::SetLegTuck(float tuck, float duration) {
    legTuckFrom_ = legTuck_;
    legTuckTarget_ = std::clamp(tuck, 0.0f, 1.0f);
    legTuckTimer_ = 0.0f;
    legTuckDuration_ = (std::max)(0.01f, duration);
}

void BossSpider::SetLegBend(float bend, float duration) {
    legBendFrom_ = legBend_;
    legBendTarget_ = std::clamp(bend, 0.0f, 1.0f);
    legBendTimer_ = 0.0f;
    legBendDuration_ = (std::max)(0.01f, duration);
}

void BossSpider::UpdateLegPosture(float deltaTime) {
    // 畳み具合。足の位置がこれで決まるので、一気に変えると足がワープする
    if (legTuckTimer_ < legTuckDuration_) {
        legTuckTimer_ += deltaTime;
        const float progress = std::clamp(legTuckTimer_ / legTuckDuration_, 0.0f, 1.0f);
        legTuck_ = Lerp(legTuckFrom_, legTuckTarget_, SmoothInOut(progress));
    } else {
        legTuck_ = legTuckTarget_;
    }

    // 折り具合（1で膝を曲げた通常姿勢・0で真横に伸び切る）
    if (legBendTimer_ < legBendDuration_) {
        legBendTimer_ += deltaTime;
        const float progress = std::clamp(legBendTimer_ / legBendDuration_, 0.0f, 1.0f);
        legBend_ = Lerp(legBendFrom_, legBendTarget_, SmoothInOut(progress));
    } else {
        legBend_ = legBendTarget_;
    }

    for (int index = 0; index < activeLegCount_; ++index) {
        legs_[static_cast<size_t>(index)]->SetPosture(1.0f, legTuck_);
    }
}

float BossSpider::GetFootReach() const {
    if (activeLegCount_ <= 0) {
        return parameters_.footRadius;
    }
    // 真横に伸び切っているときは、折れ線を伸ばし切った長さがそのまま届く範囲になる。
    // 膝を曲げているときは足を置いている半径。あいだは混ぜる
    const float straight = parameters_.bodyRadius * 0.85f + BossSpiderLeg::ResolvePathLength(parameters_);
    const float bent = legs_[0]->CalcFootReach(parameters_);
    return Lerp(straight, bent, std::clamp(legBend_, 0.0f, 1.0f));
}

void BossSpider::ScaleSizesBy(float ratio) {
    if (ratio <= 0.0f || std::abs(ratio - 1.0f) < 0.0001f) {
        return;
    }

    // どの値が「長さ」なのかは BossParameters 側にまとめてある
    ScaleSpiderLengths(parameters_, ratio);

    // 脚の球の数は「長さ ÷ 球の直径」で決まり、比率で掛けている限り変わらない。
    // なので組み直さず、半径を配り直すだけで足りる。
    // ここで RebuildLegs を呼ぶと Configure が連なりを作り直してしまい、
    // くっついた球も切り落とし中の球も消えるうえ、隠していた脚まで出てきてしまう
    for (int index = 0; index < activeLegCount_; ++index) {
        legs_[static_cast<size_t>(index)]->ApplySphereRadius(parameters_.legSphereRadius);
    }
    transform_->scale_ = Vector3{parameters_.bodyRadius, parameters_.bodyRadius, parameters_.bodyRadius};
    transform_->UpdateMatrix();

    // 足の置き場所は半径ぶん外へずれるので、立っているときだけ置き直して高さを取り直す。
    // 変形中や跳躍中に置き直すと、動きの途中で足がワープする
    if (phase_ == Phase::Active) {
        ReplantFeet();
        standHeight_ = CalcFootAverageHeight() + parameters_.bodyHeight;
        bodyPosition_.y = standHeight_;
    } else {
        standHeight_ = parameters_.legSphereRadius + parameters_.bodyHeight;
    }
}

void BossSpider::ReplantFeet() {
    for (int index = 0; index < activeLegCount_; ++index) {
        legs_[static_cast<size_t>(index)]->ResetFoot(bodyPosition_, bodyYaw_, parameters_);
    }
}

float BossSpider::CalcTargetDistance() const {
    if (!pTargetLocator_ || !pTargetLocator_->IsTargetValid()) {
        return -1.0f;
    }
    Vector3 toTarget = pTargetLocator_->GetTargetPosition() - bodyPosition_;
    toTarget.y = 0.0f;
    return toTarget.Length();
}

IBossAttack *BossSpider::PickAttack() {
    if (attacks_.empty()) {
        return nullptr;
    }
    const BossSpiderAttackParams &attack = parameters_.attack;

    // 稀に回転接近。距離は問わない
    if (Random::Range(0.0f, 1.0f) < std::clamp(attack.whirlChance, 0.0f, 1.0f)) {
        return attacks_[kAttackWhirl].get();
    }
    // 遠ければ弾を撃ち、そうでなければ跳ねまわる（跳躍はどの距離でも出る）
    const float distance = CalcTargetDistance();
    if (distance > attack.shootRange) {
        return attacks_[kAttackShoot].get();
    }
    return attacks_[kAttackLeap].get();
}

Vector3 BossSpider::UpdateAttack(float deltaTime) {
    BossAttackContext context{};
    context.spider = this;
    context.target = pTargetLocator_;
    context.deltaTime = deltaTime;

    // 回転攻撃の回り終わりは目が回っていて動けない。頭上の輪がその目印になる。
    // クールダウンもここでは進めないので、隙が終わってから次の攻撃までの間が始まる
    if (staggerTimer_ > 0.0f) {
        staggerTimer_ = (std::max)(0.0f, staggerTimer_ - deltaTime);
        BossParticles::GetInstance()->UpdateStaggerRing(GetHeadCenter(), deltaTime);
        if (staggerTimer_ <= 0.0f) {
            BossParticles::GetInstance()->StopStaggerRing();
        }
        // 隙を作っているのが攻撃自身（回転攻撃の回り終わり）のこともあるので、
        // 進行中の攻撃は止めない。止めると姿勢を持っている側が進まなくなる
        if (!pCurrentAttack_) {
            return Vector3{0.0f, 0.0f, 0.0f};
        }
    }

    if (!pCurrentAttack_) {
        // 攻撃と攻撃のあいだは歩いて間合いを取る
        attackCoolDown_ = (std::max)(0.0f, attackCoolDown_ - deltaTime);
        const Vector3 moveDirection = UpdateBodyMove(deltaTime);
        if (attackCoolDown_ <= 0.0f) {
            pCurrentAttack_ = PickAttack();
            if (pCurrentAttack_) {
                pCurrentAttack_->Start(context);
            }
        }
        return moveDirection;
    }

    // 攻撃中は胴の動きを攻撃が受け持つ。進んだぶんを進行方向として脚へ渡す
    const Vector3 before = bodyPosition_;
    pCurrentAttack_->Update(context);

    Vector3 moved = bodyPosition_ - before;
    moved.y = 0.0f;
    const Vector3 moveDirection = (moved.LengthSq() > 0.0001f) ? moved.Normalize() : Vector3{0.0f, 0.0f, 0.0f};

    if (pCurrentAttack_->IsFinished()) {
        pCurrentAttack_ = nullptr;
        attackCoolDown_ = (std::max)(0.0f, parameters_.attack.interval);
    }
    return moveDirection;
}

void BossSpider::FireBullet(const Vector3 &direction, const BossSpiderShootParams &params) {
    Vector3 forward = direction;
    forward.y = 0.0f;
    if (forward.LengthSq() <= 0.0001f) {
        forward = Vector3{std::cos(bodyYaw_), 0.0f, std::sin(bodyYaw_)};
    }
    forward = forward.Normalize();

    // 空きを探す。無ければ増やすだけ（実行中に破棄するとGPUが参照中で落ちる）
    int slot = -1;
    for (int index = 0; index < static_cast<int>(bullets_.size()); ++index) {
        if (!bullets_[static_cast<size_t>(index)].active) {
            slot = index;
            break;
        }
    }
    if (slot < 0) {
        auto sphere = std::make_unique<BossSphere>();
        sphere->InitSphere(objectName_ + "Bullet" + std::to_string(bulletPool_.size()), params.radius);
        SpiderBullet bullet{};
        bullet.sphere = sphere.get();
        bulletPool_.push_back(std::move(sphere));
        bullets_.push_back(bullet);
        slot = static_cast<int>(bullets_.size()) - 1;
    }

    // 色は蜘蛛が使っている色から選ぶ。プレイヤーは同じ色を当てて消せる
    const std::vector<Color> &usedColors = palette_.GetUsedColors();
    const Color color = usedColors.empty()
                            ? Color::RED
                            : usedColors[static_cast<size_t>(Random::Range(0, static_cast<int>(usedColors.size()) - 1))];

    SpiderBullet &bullet = bullets_[static_cast<size_t>(slot)];
    bullet.color = color;
    bullet.radius = (std::max)(0.05f, params.radius);
    bullet.life = (std::max)(0.1f, params.life);
    bullet.damage = params.damage;
    bullet.speed = (std::max)(0.1f, params.speed);
    bullet.homingRate = (std::max)(0.0f, params.homingRate);
    bullet.homingLeft = (std::max)(0.0f, params.homingTime);
    bullet.velocity = forward * bullet.speed;
    bullet.position = bodyPosition_ + forward * (parameters_.bodyRadius + bullet.radius);
    bullet.active = true;

    bullet.sphere->Place(ShellCell{-1, slot}, bullet.position, color, palette_.GetRgba(color));
    bullet.sphere->SetSphereRadius(bullet.radius);
    bullet.sphere->SetIsAlive(true);
    bullet.sphere->SetIsModelDraw(true);
}

void BossSpider::UpdateBullets(float deltaTime) {
    const bool hasTarget = (pTargetLocator_ && pTargetLocator_->IsTargetValid());
    const Vector3 targetPosition = hasTarget ? pTargetLocator_->GetTargetPosition() : Vector3{};

    for (SpiderBullet &bullet : bullets_) {
        if (!bullet.active) {
            continue;
        }
        // 追尾: 相手のほうへ向きだけを寄せる。速さは変えないので避けられる余地が残る
        if (hasTarget && bullet.homingLeft > 0.0f && bullet.homingRate > 0.0f) {
            bullet.homingLeft -= deltaTime;
            Vector3 toTarget = targetPosition - bullet.position;
            if (toTarget.LengthSq() > 0.0001f && bullet.velocity.LengthSq() > 0.0001f) {
                const Vector3 desired = toTarget.Normalize();
                const Vector3 current = bullet.velocity.Normalize();
                const float blend = std::clamp(bullet.homingRate * deltaTime, 0.0f, 1.0f);
                Vector3 mixed = Lerp(current, desired, blend);
                if (mixed.LengthSq() > 0.0001f) {
                    bullet.velocity = mixed.Normalize() * bullet.speed;
                }
            }
        }

        bullet.position += bullet.velocity * deltaTime;
        bullet.life -= deltaTime;
        bullet.sphere->SetLocalPosition(bullet.position);

        // 相手に届いたら当たりを知らせて消える
        if (hasTarget && (targetPosition - bullet.position).Length() <= bullet.radius) {
            ReportHit(bullet.position, bullet.radius, bullet.damage);
            bullet.active = false;
            bullet.sphere->Deactivate();
            continue;
        }
        if (bullet.life <= 0.0f) {
            bullet.active = false;
            bullet.sphere->Deactivate();
        }
    }
}

BulletHitResult BossSpider::RaycastAttach(const Vector3 &worldStart, const Vector3 &worldEnd, Color color) {
    BulletHitResult result{};
    // 変形が終わるまでは当たり判定を持たない（脚が生えている最中に撃たれても困る）
    if (phase_ != Phase::Active) {
        return result;
    }

    // まず飛んでいる弾を見る。同じ色を当てられた弾は消える
    int bulletIndex = -1;
    Vector3 bulletPoint{};
    if (FindBulletHit(worldStart, worldEnd, color, bulletIndex, bulletPoint)) {
        SpiderBullet &bullet = bullets_[static_cast<size_t>(bulletIndex)];
        bullet.active = false;
        bullet.sphere->Deactivate();
        result.hit = true;
        result.destroyed = true;
        result.clusterSize = 1;
        result.hitPoint = bullet.position;
        return result;
    }

    // いちばん手前で当たった脚を選ぶ
    int hitLeg = -1;
    int hitIndex = -1;
    Vector3 hitPoint{};
    if (!FindLegHit(worldStart, worldEnd, hitLeg, hitIndex, hitPoint)) {
        return result;
    }

    result.hit = true;
    result.hitPoint = hitPoint;

    BossSpiderLeg *leg = legs_[static_cast<size_t>(hitLeg)].get();
    result.attached = leg->Attach(color, hitPoint, hitIndex, palette_, parameters_, effect_);
    if (!result.attached) {
        return result; // これ以上伸ばせない（弾は当たったので消える）
    }

    // 同じ色がそろっていたらそこが消え、その先に残っていた球は切り落とされる
    int severed = 0;
    const int destroyed = leg->TryEliminate(chain_.minMatch, effect_, parameters_, severed);
    if (destroyed > 0) {
        result.destroyed = true;
        result.clusterSize = destroyed + severed;
        result.staggerTime = chain_.staggerBase +
                             chain_.staggerPerPart * static_cast<float>(destroyed - chain_.minMatch);
    }
    return result;
}

bool BossSpider::RaycastPoint(const Vector3 &worldStart, const Vector3 &worldEnd, Color color,
                              AimHit &outHit) {
    if (phase_ != Phase::Active) {
        return false;
    }

    // 当たる順番も RaycastAttach とそろえる（飛翔弾が手前を塞いでいれば照準もそこで止まる）
    int bulletIndex = -1;
    if (FindBulletHit(worldStart, worldEnd, color, bulletIndex, outHit.point)) {
        // 飛翔弾は FindBulletHit が中心をそのまま返すので、寄せ先も同じ点でよい
        outHit.center = outHit.point;
        return true;
    }

    int hitLeg = -1;
    int hitIndex = -1;
    if (!FindLegHit(worldStart, worldEnd, hitLeg, hitIndex, outHit.point)) {
        return false;
    }

    // エイムアシストの吸着先は当たった脚の球の中心。
    // 引けなければ表面の点をそのまま中心として返す（＝寄らない）
    if (hitLeg < 0 || hitLeg >= static_cast<int>(legs_.size()) ||
        !legs_[static_cast<size_t>(hitLeg)]->TryGetSpherePosition(hitIndex, outHit.center)) {
        outHit.center = outHit.point;
    }
    return true;
}

bool BossSpider::FindBulletHit(const Vector3 &worldStart, const Vector3 &worldEnd, Color color,
                               int &outBulletIndex, Vector3 &outPoint) const {
    const Vector3 segment = worldEnd - worldStart;
    const float segmentLength = segment.Length();
    if (segmentLength <= 0.0001f) {
        return false;
    }
    const Vector3 direction = segment / segmentLength;

    for (size_t index = 0; index < bullets_.size(); ++index) {
        const SpiderBullet &bullet = bullets_[index];
        if (!bullet.active || bullet.color != color) {
            continue; // 色が違う弾はすり抜ける（当てても消えない）
        }
        const Vector3 toCenter = bullet.position - worldStart;
        const float along = toCenter.Dot(direction);
        if (along < -bullet.radius || along > segmentLength + bullet.radius) {
            continue;
        }
        if (toCenter.LengthSq() - along * along > bullet.radius * bullet.radius) {
            continue;
        }

        outBulletIndex = static_cast<int>(index);
        outPoint = bullet.position;
        return true;
    }
    return false;
}

bool BossSpider::FindLegHit(const Vector3 &worldStart, const Vector3 &worldEnd, int &outLegIndex,
                            int &outSphereIndex, Vector3 &outPoint) const {
    int hitLeg = -1;
    int hitIndex = -1;
    float nearest = 0.0f;
    Vector3 hitPoint{};

    for (int index = 0; index < activeLegCount_; ++index) {
        float distance = 0.0f;
        Vector3 point{};
        int sphereIndex = -1;
        if (!legs_[static_cast<size_t>(index)]->Raycast(worldStart, worldEnd, parameters_, distance, point,
                                                        sphereIndex)) {
            continue;
        }
        if (hitLeg < 0 || distance < nearest) {
            hitLeg = index;
            hitIndex = sphereIndex;
            nearest = distance;
            hitPoint = point;
        }
    }
    if (hitLeg < 0) {
        return false;
    }

    outLegIndex = hitLeg;
    outSphereIndex = hitIndex;
    outPoint = hitPoint;
    return true;
}

bool BossSpider::FindLockOnTarget(const LockOnRequest &request, LockOnResult &out) {
    out = LockOnResult{};
    if (phase_ != Phase::Active) {
        return false;
    }

    const Vector3 aim = (request.aimDirection.LengthSq() > 0.0001f)
                            ? request.aimDirection.Normalize()
                            : Vector3{0.0f, 0.0f, 1.0f};
    const float cosLimit = std::cos(std::clamp(request.maxAngleDegrees, 0.0f, 180.0f) *
                                    (std::numbers::pi_v<float> / 180.0f));

    // 照準にいちばん近い脚の先端を狙う（脚は先へ伸ばすので、狙う先も先端になる）
    float bestCos = cosLimit;
    for (int index = 0; index < activeLegCount_; ++index) {
        Vector3 tip{};
        if (!legs_[static_cast<size_t>(index)]->TryGetTipPosition(tip)) {
            continue;
        }
        Vector3 toTip = tip - request.origin;
        const float distance = toTip.Length();
        if (distance <= 0.0001f || distance > request.maxDistance) {
            continue;
        }
        const float cosAngle = (toTip / distance).Dot(aim);
        if (cosAngle < bestCos) {
            continue;
        }
        bestCos = cosAngle;
        out.found = true;
        out.worldPosition = tip;
        out.distance = distance;
        out.angleDegrees = std::acos(std::clamp(cosAngle, -1.0f, 1.0f)) *
                           (180.0f / std::numbers::pi_v<float>);
        // 先端の並び順はくっつくたびに変わるので、脚の番号だけ覚えておく
        out.cell = ShellCell{index, -1};
    }
    return out.found;
}

bool BossSpider::TryGetTargetPosition(const ShellCell &cell, Vector3 &out) {
    if (phase_ != Phase::Active || cell.vertex < 0 || cell.vertex >= activeLegCount_) {
        return false;
    }
    return legs_[static_cast<size_t>(cell.vertex)]->TryGetTipPosition(out);
}

void BossSpider::Draw(const ViewProjection &viewProjection) {
    if (phase_ == Phase::Hidden) {
        return;
    }

    BaseObject::Draw(viewProjection);
    for (int index = 0; index < activeLegCount_ && index < static_cast<int>(legs_.size()); ++index) {
        legs_[static_cast<size_t>(index)]->Draw(viewProjection);
    }

    // 攻撃中だけ出る表示物（着地予告など）
    if (pCurrentAttack_) {
        pCurrentAttack_->Draw(viewProjection);
    }

    // 撃った弾
    for (SpiderBullet &bullet : bullets_) {
        if (bullet.active) {
            bullet.sphere->Draw(viewProjection);
        }
    }
}

void BossSpider::DrawImGui() {
    // 「トランスフォームマネージャ」ウィンドウで選択したときに出る、オブジェクトの全項目
    BaseObject::DrawImGui();
    DrawGameplayImGui();
}

void BossSpider::DrawGameplayImGui() {
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader("蜘蛛形態", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Text("状態: %s", GetPhaseName());
    ImGui::TextDisabled("出ているあいだ、球体形態は描かれません（コアは1つ）");
    if (ImGui::Button("変形を再生")) {
        // 本番と同じく、球体形態のコアの位置・大きさから始める
        Awaken(handoffPosition_, handoffRadius_);
    }
    ImGui::SameLine();
    if (ImGui::Button("変形を飛ばす")) {
        Awaken(handoffPosition_, handoffRadius_);
        SkipTransform();
    }
    ImGui::SameLine();
    if (ImGui::Button("引っ込める")) {
        Hide();
    }

    int steppingCount = 0;
    for (int index = 0; index < activeLegCount_; ++index) {
        steppingCount += legs_[static_cast<size_t>(index)]->IsStepping() ? 1 : 0;
    }
    ImGui::Text("浮いている脚: %d / %d", steppingCount, activeLegCount_);

    ImGui::SeparatorText("脚の並び（左右への密集はここ）");
    bool rebuild = false;
    rebuild |= ImGui::SliderInt("脚の本数", &parameters_.legCount, 2, 12);
    rebuild |= ImGui::SliderFloat("左右への密集", &parameters_.legSpread, 10.0f, 180.0f, "%.0f 度");
    HelpMarker("片側（右半分・左半分）に脚が広がる角度です。\n"
               "180 度: 胴のまわりに等間隔（既定）\n"
               "小さくするほど真横へ寄って密集し、蜘蛛らしくなります\n"
               "※ 足を置く半径 5.0 のままだと 40 度あたりが下限で、\n"
               "   それより狭めると隣の脚と足が重なります");
    rebuild |= ImGui::SliderFloat("脚全体の前後寄せ", &parameters_.legSpreadOffset, -90.0f, 90.0f, "%.0f 度");
    HelpMarker("脚の扇全体を前後へ回します。正で前寄り、負で後ろ寄り");
    if (ImGui::Button("等間隔に戻す")) {
        parameters_.legSpread = 180.0f;
        parameters_.legSpreadOffset = 0.0f;
        rebuild = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("左右に寄せる（蜘蛛らしく）")) {
        parameters_.legSpread = 60.0f;
        parameters_.legSpreadOffset = 0.0f;
        rebuild = true;
    }

    ImGui::SeparatorText("脚の球");
    rebuild |= ImGui::SliderInt("付け根〜膝の球数(0で自動)", &parameters_.upperSphereCount, 0, 24);
    ImGui::SameLine();
    ImGui::TextDisabled("実際: %d", legs_.empty() ? 0 : legs_.front()->GetUpperSphereCount());
    rebuild |= ImGui::SliderInt("膝〜足先の球数(0で自動)", &parameters_.lowerSphereCount, 0, 24);
    ImGui::SameLine();
    ImGui::TextDisabled("実際: %d", legs_.empty() ? 0 : legs_.front()->GetLowerSphereCount());

    // 膝の位置は上腿・下腿の球数の比で決まるので、間隔は必ず両側でそろう。
    // あとは全体の密度（間隔と直径の関係）だけを見せれば足りる
    const float spacing = legs_.empty() ? 0.0f : legs_.front()->GetSphereSpacing();
    const float diameter = parameters_.legSphereRadius * 2.0f;
    ImGui::Text("合計 %d 個 / 間隔 %.2f (直径 %.2f)",
                legs_.empty() ? 0 : legs_.front()->GetSphereCount(), spacing, diameter);
    ImGui::SameLine();
    if (spacing > diameter * 1.02f) {
        ImGui::TextColored(ImVec4{1.0f, 0.6f, 0.2f, 1.0f}, "隙間あり");
    } else if (spacing < diameter * 0.7f) {
        ImGui::TextColored(ImVec4{0.6f, 0.8f, 1.0f, 1.0f}, "重なり多め");
    } else {
        ImGui::TextColored(ImVec4{0.4f, 0.9f, 0.4f, 1.0f}, "ちょうど");
    }
    HelpMarker("膝の位置は上腿と下腿の球数の比で自動的に決まるので、\n"
               "どちらの数を変えても球の間隔は両側でそろいます。\n"
               "間隔が直径より広いと隙間が空くので、球数を増やすか\n"
               "「球数を自動に戻す」を押してください");
    if (ImGui::Button("球数を自動に戻す")) {
        parameters_.upperSphereCount = 0;
        parameters_.lowerSphereCount = 0;
        rebuild = true;
    }

    ImGui::SeparatorText("大きさ");
    rebuild |= ImGui::DragFloat("胴の半径", &parameters_.bodyRadius, 0.05f, 0.2f, 10.0f);
    rebuild |= ImGui::DragFloat("脚の球の半径", &parameters_.legSphereRadius, 0.01f, 0.05f, 3.0f);
    // この2つは折れ線の長さを変える＝自動の球数も変わるので、組み直しに含める
    rebuild |= ImGui::DragFloat("脚の長さ", &parameters_.legLength, 0.05f, 0.5f, 30.0f);
    rebuild |= ImGui::DragFloat("膝の持ち上げ", &parameters_.kneeLift, 0.05f, 0.0f, 10.0f);
    if (rebuild) {
        RebuildLegs();
    }
    ImGui::DragFloat("足を置く半径", &parameters_.footRadius, 0.05f, 0.5f, 30.0f);
    ImGui::DragFloat("胴の高さ", &parameters_.bodyHeight, 0.05f, 0.1f, 20.0f);

    ImGui::SeparatorText("変形（球体形態のコアから生える）");
    ImGui::TextDisabled("合計 %.2f 秒（3つの動きが重なるので単純な和より短い）", CalcTransformDuration());
    ImGui::DragFloat("崩れ落ちる時間", &parameters_.collapseTime, 0.05f, 0.05f, 10.0f);
    HelpMarker("変形の前ぶりです。殻を失ったコアがふらつきながら地面へ落ちます");
    ImGui::DragFloat("崩れるときのふらつき", &parameters_.collapseSway, 0.05f, 0.0f, 5.0f);
    ImGui::DragFloat("ふらつきの速さ", &parameters_.collapseSwaySpeed, 0.05f, 0.0f, 20.0f);
    ImGui::DragFloat("落ちてから起き上がるまで", &parameters_.collapseRest, 0.05f, 0.0f, 5.0f);
    ImGui::DragFloat("登場カメラの距離", &parameters_.introCameraDistance, 0.1f, 1.0f, 60.0f);
    HelpMarker("変形でコアが大きくなるので、通常の寄りより離しておきます");
    ImGui::DragFloat("登場カメラの高さ", &parameters_.introCameraHeight, 0.05f, 0.2f, 20.0f);
    HelpMarker("地面からの高さです。低いほど見上げる画になります（座標は演出中ずっと動きません）");
    ImGui::DragFloat("浮き上がる時間", &parameters_.riseTime, 0.05f, 0.05f, 10.0f);
    HelpMarker("コアが「立つ高さ」まで上がるまでの時間です。上がりきったら下がりません。"
               "脚が生えているあいだに下ろすと、浮き上がりの巻き戻しに見えてしまうためです");
    ImGui::DragFloat("脚が生えきる時間", &parameters_.growTime, 0.05f, 0.05f, 15.0f);
    ImGui::SliderFloat("脚ごとの生え始めのずれ", &parameters_.growStagger, 0.0f, 0.8f);
    HelpMarker("0だと8本が一斉に生えます。大きくすると隣り合う脚が交互に生えます");
    ImGui::DragFloat("着地までの時間", &parameters_.landTime, 0.05f, 0.05f, 10.0f);
    HelpMarker("真横へ伸びた脚の関節が折れて、足が地面へ下りるまでの時間です");

    ImGui::SeparatorText("歩行");
    ImGui::DragFloat("歩く速さ", &parameters_.moveSpeed, 0.05f, 0.0f, 30.0f);
    ImGui::DragFloat("向き直りの速さ", &parameters_.turnSpeed, 1.0f, 0.0f, 720.0f);
    ImGui::DragFloat("踏み替えの時間", &parameters_.stepTime, 0.01f, 0.03f, 2.0f);
    ImGui::DragFloat("足を上げる高さ", &parameters_.stepHeight, 0.05f, 0.0f, 10.0f);
    ImGui::DragFloat("踏み替える距離", &parameters_.stepTrigger, 0.05f, 0.1f, 10.0f);
    ImGui::DragFloat("踏み越す量", &parameters_.stepLead, 0.05f, 0.0f, 10.0f);
    ImGui::DragFloat("胴の上下の揺れ", &parameters_.bodyBob, 0.01f, 0.0f, 2.0f);
    ImGui::DragFloat("胴の左右の揺れ", &parameters_.bodySway, 0.01f, 0.0f, 2.0f);
    ImGui::DragFloat("止まる距離", &parameters_.stopDistance, 0.1f, 0.0f, 30.0f);



    ImGui::SeparatorText("脚を切り落とす");
    ImGui::TextDisabled("同じ色がそろった場所より先に球が残っていれば、そこから先は切り落とされます");
    int severedTotal = 0;
    for (int index = 0; index < activeLegCount_; ++index) {
        severedTotal += legs_[static_cast<size_t>(index)]->GetSeveredCount();
    }
    ImGui::Text("飛び散っている球: %d 個", severedTotal);
    ImGui::DragFloat("外へ飛ぶ速さ", &parameters_.sever.speed, 0.1f, 0.0f, 40.0f);
    ImGui::DragFloat("上へ飛ぶ速さ", &parameters_.sever.lift, 0.1f, 0.0f, 40.0f);
    ImGui::DragFloat("散らばり", &parameters_.sever.scatter, 0.05f, 0.0f, 15.0f);
    HelpMarker("1個ごとに速度をばらつかせます。0にすると全部そろって飛びます");
    ImGui::DragFloat("落下の強さ", &parameters_.sever.gravity, 0.5f, 0.0f, 120.0f);
    ImGui::SliderFloat("地面で跳ねる強さ", &parameters_.sever.bounce, 0.0f, 1.0f);
    ImGui::DragFloat("消えるまでの時間", &parameters_.sever.life, 0.05f, 0.1f, 10.0f);
    HelpMarker("最後の3割の時間で縮んで消えます");

    ImGui::SeparatorText("攻撃");
    BossSpiderAttackParams &attack = parameters_.attack;
    ImGui::Text("いまの攻撃: %s / %s",
                pCurrentAttack_ ? pCurrentAttack_->GetName() : "なし",
                pCurrentAttack_ ? pCurrentAttack_->GetPhaseName() : "-");
    ImGui::Text("次の攻撃まで: %.2f 秒", attackCoolDown_);
    for (size_t index = 0; index < attacks_.size(); ++index) {
        // SameLine の第1引数は「開始位置からのオフセット」なので、
        // 間隔を詰めるつもりで負の値を渡すとボタンが左端に重なって隠れる。
        // 2つ目以降を素直に横並びにするだけでよい
        if (index > 0) {
            ImGui::SameLine();
        }
        if (ImGui::Button(attacks_[index]->GetName())) {
            if (pCurrentAttack_) {
                BossAttackContext cancel{};
                cancel.spider = this;
                cancel.target = pTargetLocator_;
                pCurrentAttack_->Cancel(cancel);
            }
            pCurrentAttack_ = attacks_[index].get();
            BossAttackContext start{};
            start.spider = this;
            start.target = pTargetLocator_;
            pCurrentAttack_->Start(start);
        }
    }
    ImGui::DragFloat("攻撃の間隔", &attack.interval, 0.05f, 0.1f, 20.0f);
    ImGui::DragFloat("最初の攻撃までの間", &attack.firstDelay, 0.1f, 0.0f, 30.0f);
    HelpMarker("変形しきってから1回目の攻撃までの間です。登場演出の余韻を邪魔しないよう長めに取ります");
    ImGui::DragFloat("弾を撃つ距離", &attack.shootRange, 0.5f, 1.0f, 100.0f);
    HelpMarker("相手がこれより遠ければ弾を撃ちます。近ければ跳ねまわります");
    ImGui::SliderFloat("回転接近の確率", &attack.whirlChance, 0.0f, 1.0f);
    HelpMarker("距離を問わず、この確率で回転接近を選びます（稀に出すので小さめに）");

    if (ImGui::TreeNode("1. 跳ねまわる")) {
        ImGui::SliderInt("跳ぶ回数", &attack.leap.hopCount, 1, 8);
        ImGui::DragFloat("跳ぶ前の屈伸の時間", &attack.leap.crouchTime, 0.01f, 0.05f, 3.0f);
        ImGui::DragFloat("跳ぶ前に沈む深さ", &attack.leap.crouchDepth, 0.05f, 0.0f, 5.0f);
        HelpMarker("飛ぶ前に胴だけ沈めます。足は地面に着いたままなので脚が畳まれ、"
                   "そこから一気に伸び上がるので立ち幅跳びの屈伸のように見えます");
        ImGui::DragFloat("飛び上がる時間", &attack.leap.riseTime, 0.01f, 0.05f, 3.0f);
        ImGui::DragFloat("頂点の高さ", &attack.leap.apexHeight, 0.1f, 0.5f, 40.0f);
        ImGui::DragFloat("落下の時間", &attack.leap.fallTime, 0.01f, 0.05f, 3.0f);
        ImGui::DragFloat("着地の屈伸の時間", &attack.leap.impactTime, 0.01f, 0.02f, 3.0f);
        ImGui::DragFloat("着地で沈む深さ", &attack.leap.landAbsorbDepth, 0.05f, 0.0f, 5.0f);
        HelpMarker("着地の衝撃を殺すように沈んでから立ち上がります。0にすると硬い着地になります");
        ImGui::DragFloat("着地の有効半径", &attack.leap.impactRadius, 0.1f, 0.5f, 30.0f);
        ImGui::DragFloat("着地のダメージ", &attack.leap.damage, 0.5f, 0.0f, 200.0f);
        ImGui::DragFloat("着地点のばらつき", &attack.leap.landSpread, 0.1f, 0.0f, 20.0f);
        HelpMarker("相手のぴったり真上ではなく、この半径のどこかへ落ちます（0で真上）");
        ImGui::DragFloat("1回で跳べる距離", &attack.leap.maxLeapRange, 0.5f, 1.0f, 80.0f);
        ImGui::DragFloat("最後の硬直", &attack.leap.recoverTime, 0.05f, 0.0f, 5.0f);
        ImGui::SliderFloat("空中で脚を畳む量", &attack.leap.legTuck, 0.0f, 1.0f);
        ImGui::DragFloat("脚を畳む時間", &attack.leap.legFoldTime, 0.01f, 0.02f, 2.0f);
        HelpMarker("飛び上がるときに脚を畳むのにかける時間です。"
                   "伸ばし戻すほうは落下の時間に合わせて自動で行い、接地する前に着地姿勢を作り終えます");
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("2. 弾を撃つ")) {
        ImGui::DragFloat("溜めの時間", &attack.shoot.telegraphTime, 0.05f, 0.0f, 5.0f);
        ImGui::SliderInt("発射数", &attack.shoot.shotCount, 1, 20);
        ImGui::DragFloat("発射の間隔", &attack.shoot.shotInterval, 0.01f, 0.02f, 3.0f);
        ImGui::DragFloat("弾速", &attack.shoot.speed, 0.1f, 0.5f, 60.0f);
        ImGui::DragFloat("弾の半径", &attack.shoot.radius, 0.05f, 0.1f, 6.0f);
        ImGui::DragFloat("弾が消えるまで", &attack.shoot.life, 0.1f, 0.5f, 30.0f);
        ImGui::DragFloat("左右のばらつき", &attack.shoot.spreadDegrees, 0.5f, 0.0f, 60.0f);
        ImGui::DragFloat("追尾の強さ", &attack.shoot.homingRate, 0.05f, 0.0f, 12.0f);
        HelpMarker("相手のほうへ向きを寄せる強さです。0で真っ直ぐ飛びます。"
                   "速さは変わらないので、大きくしても回り込めば避けられます");
        ImGui::DragFloat("追尾する時間", &attack.shoot.homingTime, 0.1f, 0.0f, 20.0f);
        HelpMarker("この時間を過ぎたら追うのをやめて真っ直ぐ飛びます");
        ImGui::DragFloat("命中ダメージ", &attack.shoot.damage, 0.5f, 0.0f, 200.0f);
        ImGui::DragFloat("撃ち終わりの硬直", &attack.shoot.recoverTime, 0.05f, 0.0f, 5.0f);

        ImGui::SeparatorText("撃つときの動き");
        ImGui::DragFloat("撃つ前に伸び上がる高さ", &attack.shoot.telegraphRise, 0.05f, 0.0f, 6.0f);
        HelpMarker("溜めのあいだに胴を持ち上げます。脚は接地したままなので、伸び上がる形になります");
        ImGui::DragFloat("1発ごとに沈む深さ", &attack.shoot.recoilDepth, 0.05f, 0.0f, 4.0f);
        ImGui::DragFloat("反動が収まる時間", &attack.shoot.recoilTime, 0.01f, 0.02f, 2.0f);
        HelpMarker("撃った瞬間にすとんと沈み、この時間をかけて戻ります");
        ImGui::DragFloat("震えの大きさ", &attack.shoot.shakeAmount, 0.01f, 0.0f, 1.5f);
        ImGui::DragFloat("震えの速さ", &attack.shoot.shakeSpeed, 1.0f, 1.0f, 200.0f);
        HelpMarker("反動が収まるにつれて震えも小さくなります。0にすると沈むだけになります");
        int flying = 0;
        for (const SpiderBullet &bullet : bullets_) {
            flying += bullet.active ? 1 : 0;
        }
        ImGui::TextDisabled("飛んでいる弾: %d 発（同じ色を当てると消える）", flying);
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("3. 真横に広げて回転")) {
        ImGui::DragFloat("脚を広げる時間", &attack.whirl.telegraphTime, 0.05f, 0.05f, 8.0f);
        HelpMarker("予備動作です。遅いほど避ける余地が生まれます");
        ImGui::DragFloat("その場で回る時間", &attack.whirl.spinTime, 0.05f, 0.1f, 15.0f);
        ImGui::DragFloat("回り始めの速さ", &attack.whirl.spinSpeed, 5.0f, 0.0f, 1440.0f);
        ImGui::SliderFloat("回り終わりの速さの割合", &attack.whirl.spinEndSpeedRatio, 0.0f, 1.0f);
        HelpMarker("回り始めの速さに対する割合です。1で等速、小さいほど回りながら減速します。\n"
                   "遅くなりきったところが「そろそろ隙ができる」の合図になります");
        ImGui::TextDisabled("回り終わりの隙は下の「ひるみ」で調整します");
        ImGui::DragFloat("回るときの脚の高さ", &attack.whirl.spinHeight, 0.05f, 0.0f, 15.0f);
        HelpMarker("地面からの高さです。低いほど当たりやすくなります（胴が地面へ潜らない範囲で止まります）");
        ImGui::TextDisabled("いまの届く範囲: %.2f（真横に伸び切ると %.2f）", GetFootReach(),
                            parameters_.bodyRadius * 0.85f + BossSpiderLeg::ResolvePathLength(parameters_));
        HelpMarker("真横に伸び切った脚の先が届く範囲が、そのまま攻撃範囲になります。歩かずその場で回ります");
        ImGui::DragFloat("接触ダメージ", &attack.whirl.damage, 0.5f, 0.0f, 200.0f);
        ImGui::DragFloat("回転後の硬直", &attack.whirl.recoverTime, 0.05f, 0.0f, 5.0f);
        ImGui::TreePop();
    }


    ImGui::SeparatorText("ひるみ（回転攻撃の回り終わり）");
    ImGui::TextDisabled("回転が止まったあと、脚を真横に広げ切ったまま低い姿勢で静止します。");
    ImGui::TextDisabled("立ち上がって脚を戻すのはこのあと。当たり判定も出ていないので狙い撃てます");
    if (IsStaggered()) {
        ImGui::TextColored(ImVec4{1.0f, 0.85f, 0.3f, 1.0f}, "ひるみ中: 残り %.2f 秒（狙い撃ちのチャンス）",
                           staggerTimer_);
    } else {
        ImGui::Text("いまはひるんでいません");
        if (ImGui::Button("ひるませる")) {
            BeginStagger(parameters_.attack.whirl.staggerTime);
        }
        HelpMarker("回転攻撃を出さずに、その場でひるみだけを再生します。\n"
                   "脚は広がっていないので、姿勢まで含めて見たいときは\n"
                   "上の「回転して接近」から出してください");
        ImGui::SameLine();
        if (ImGui::Button("回転攻撃から出す") && attacks_.size() > kAttackWhirl) {
            if (pCurrentAttack_) {
                BossAttackContext cancel{};
                cancel.spider = this;
                cancel.target = pTargetLocator_;
                pCurrentAttack_->Cancel(cancel);
            }
            pCurrentAttack_ = attacks_[kAttackWhirl].get();
            BossAttackContext start{};
            start.spider = this;
            start.target = pTargetLocator_;
            pCurrentAttack_->Start(start);
        }
    }
    if (ImGui::Button("ひるみを止める")) {
        ClearStagger();
    }

    ImGui::DragFloat("動けない時間", &parameters_.attack.whirl.staggerTime, 0.05f, 0.0f, 12.0f);
    HelpMarker("脚を広げ切ったまま静止している時間です。0にすると隙が無くなります");
    ImGui::SliderFloat("回り終わりの速さの割合##stagger", &parameters_.attack.whirl.spinEndSpeedRatio,
                       0.0f, 1.0f);
    HelpMarker("回り始めの速さに対する割合です。小さいほど回りながら減速し、\n"
               "遅くなりきったところが「そろそろ隙ができる」の合図になります（1で等速）");
    ImGui::TextDisabled("頭上を回る粒の見た目・置き方は「パーティクル設定」→「ボスの土煙」で調整します");

    ImGui::SeparatorText("撃破演出");
    BossSpiderDefeatParams &defeat = parameters_.defeat;
    ImGui::Text("残っている脚: %d / %d", GetAliveLegCount(), activeLegCount_);
    if (ImGui::Button("撃破演出を始める")) {
        BeginDefeat();
    }
    HelpMarker("脚をすべて壊すのと同じ扱いで、撃破演出をその場で始めます");
    ImGui::SameLine();
    if (ImGui::Button("戦闘中に戻す")) {
        // 撃破で脚を切り落としているので、戻すときは組み直す
        phase_ = Phase::Active;
        isDefeatFinished_ = false;
        bodyPosition_.y = standHeight_;
        transform_->scale_ = Vector3{parameters_.bodyRadius, parameters_.bodyRadius, parameters_.bodyRadius};
        transform_->UpdateMatrix();
        SetIsModelDraw(true);
        RebuildLegs();
        ReplantFeet();
    }

    ImGui::DragFloat("落ちきるまでの時間", &defeat.fallTime, 0.05f, 0.1f, 20.0f);
    ImGui::DragFloat("ふらつきの大きさ", &defeat.swayAmount, 0.05f, 0.0f, 6.0f);
    ImGui::DragFloat("ふらつきの速さ", &defeat.swaySpeed, 0.05f, 0.0f, 20.0f);
    ImGui::DragFloat("ふらつきで傾く角度", &defeat.tiltAngle, 0.5f, 0.0f, 90.0f);
    HelpMarker("落ちるにつれてふらつきは収まり、地面に着くと止まります");
    ImGui::DragFloat("落ちてからはじけるまでの間", &defeat.restTime, 0.05f, 0.0f, 10.0f);
    ImGui::DragFloat("ふくらむ大きさ", &defeat.burstScale, 0.01f, 1.0f, 4.0f);
    ImGui::DragFloat("はじけて消える時間", &defeat.burstTime, 0.01f, 0.05f, 5.0f);
    HelpMarker("素早くふくらんでから、ためて一気にしぼみます。消えきったところで撃破フラグが立ちます");
    ImGui::Text("撃破フラグ（シーン遷移の起点）: %s", isDefeatFinished_ ? "立っている" : "まだ");

    ImGui::SeparatorText("撃破演出（画面）");
    ImGui::DragFloat("黒帯が閉じる時間", &defeat.barTime, 0.05f, 0.05f, 10.0f);
    ImGui::SliderFloat("黒帯の高さ", &defeat.barRatio, 0.0f, 0.5f);
    HelpMarker("画面の高さに対する割合です。上下それぞれこの高さぶん出ます");
    ImGui::DragFloat("カメラが寄る時間", &defeat.focusTime, 0.05f, 0.05f, 15.0f);
    ImGui::DragFloat("寄ったときの距離", &defeat.focusDistance, 0.1f, 1.0f, 60.0f);
    ImGui::DragFloat("正面からのずらし角", &defeat.focusYawOffset, 1.0f, -180.0f, 180.0f);
    HelpMarker("敵の正面に回り込んで向き合います。0で真正面、ずらすと斜めからの画になります");
    ImGui::DragFloat("寄ったときの高さ", &defeat.focusHeight, 0.1f, -20.0f, 40.0f);
    ImGui::DragFloat("注視点の高さ", &defeat.lookHeight, 0.1f, -10.0f, 20.0f);
    ImGui::DragFloat("手持ち揺れの大きさ", &defeat.handheldAmount, 0.01f, 0.0f, 3.0f);
    ImGui::DragFloat("手持ち揺れの速さ", &defeat.handheldSpeed, 0.05f, 0.0f, 10.0f);
    ImGui::DragFloat("カメラが戻る時間", &defeat.returnTime, 0.05f, 0.05f, 10.0f);
    HelpMarker("登場演出のあと、黒帯が開いてカメラがプレイヤーへ戻るまでの時間です");

    ImGui::SeparatorText("保存");
    ImGui::TextDisabled("保存先: Assets/jsons/Boss/%s.json の \"spider\"", bossId_.c_str());
    if (ImGui::Button("この値を保存")) {
        SaveParameters();
        ImGuiNotification::Post("蜘蛛のパラメータを保存しました", {0.2f, 0.8f, 0.2f, 1.0f});
    }
    ImGui::SameLine();
    if (ImGui::Button("保存した値に戻す")) {
        LoadParameters();
        ImGuiNotification::Post("保存した値を読み込みました", {0.4f, 0.8f, 1.0f, 1.0f});
    }
    ImGui::SameLine();
    if (ImGui::Button("コードの既定値に戻す")) {
        parameters_ = BossSpiderParams{};
        RebuildLegs();
        ImGuiNotification::Post("既定値に戻しました（保存はしていません）", {1.0f, 0.7f, 0.2f, 1.0f});
    }
    HelpMarker("既定値に戻しただけではファイルは変わりません。\n"
               "その状態で残したいときは「この値を保存」を押してください");
#endif // USE_IMGUI
}
