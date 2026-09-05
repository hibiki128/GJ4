#include "BossSpider.h"
#include "Easing.h"
#include "MyMath.h"
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
    CreatePrimitiveModel(PrimitiveType::Sphere);
    SetTexture(kBossTexturePath);
    SetShouldSave(false);

    palette_.LoadMaster();

    // 保存済みの値があればそれで組む（無ければ既定値のまま）
    LoadSpiderParams(bossId_, parameters_);

    legNamePrefix_ = objectName + "Leg";
    RebuildLegs();

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
    transform_->scale_ = Vector3{startRadius_, startRadius_, startRadius_};
    transform_->UpdateMatrix();

    // 足の着地点は先に決めておく。脚が生えきったあと、ここへ向けて関節が曲がる
    for (int index = 0; index < activeLegCount_; ++index) {
        legs_[static_cast<size_t>(index)]->SetHidden(false);
        legs_[static_cast<size_t>(index)]->ResetFoot(bodyPosition_, bodyYaw_, parameters_);
    }

    // 立ったときの高さは、置いた足の高さから決める（足の球が地面に乗る）
    standHeight_ = CalcFootAverageHeight() + parameters_.bodyHeight;

    phase_ = Phase::Transform;
    transformTime_ = 0.0f;
    SetIsAlive(true);
    SetIsModelDraw(true);
    // まだ1本も生えていない状態から始める
    PlaceLegs(bodyPosition_, 0.0f, 0.0f);
}

void BossSpider::Hide() {
    phase_ = Phase::Hidden;
    transformTime_ = 0.0f;
    for (int index = 0; index < activeLegCount_; ++index) {
        legs_[static_cast<size_t>(index)]->SetHidden(true);
    }
    SetIsAlive(false);
    SetIsModelDraw(false);
}

const char *BossSpider::GetPhaseName() const {
    switch (phase_) {
    case Phase::Transform:
        return "変形中";
    case Phase::Active:
        return "戦闘中";
    default:
        return "未出現";
    }
}

void BossSpider::SkipTransform() {
    phase_ = Phase::Active;
    transformTime_ = 0.0f;
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
    const float radius = Lerp(startRadius_, parameters_.bodyRadius, rise);

    transform_->translation_ = bodyPosition_;
    transform_->quaternionRotation_ = Quaternion::FromAxisAngle(kWorldUp, bodyYaw_);
    transform_->scale_ = Vector3{radius, radius, radius};
    transform_->UpdateMatrix();

    const float bend = SmoothInOut(bendProgress);
    PlaceLegs(bodyPosition_, growProgress, bend);

    if (transformTime_ >= CalcTransformDuration()) {
        phase_ = Phase::Active;
        transformTime_ = 0.0f;
    }
}

void BossSpider::Update() {
    BaseObject::Update();

    if (phase_ == Phase::Hidden) {
        return;
    }

    const float deltaTime = Frame::DeltaTime();

    // 変形が終わるまでは歩かない
    if (phase_ != Phase::Active) {
        UpdateTransform(deltaTime);
        return;
    }

    const Vector3 moveDirection = UpdateBodyMove(deltaTime);
    UpdateLegs(moveDirection, deltaTime);

    // 胴の高さと揺れを決めてから脚を並べる。逆にすると脚の付け根が1フレーム前の胴を
    // 参照することになり、揺れるたびに胴と脚がずれて見える
    const Vector3 renderPosition = UpdateBodyPosture(deltaTime, moveDirection.LengthSq() > 0.0001f);
    PlaceLegs(renderPosition);

    // 並べ直したあとに演出を進める。逆にすると、くっついた球の吸い寄せも
    // 消滅の押し出しも、並べ直しで毎フレーム打ち消されてしまう
    for (int index = 0; index < activeLegCount_; ++index) {
        legs_[static_cast<size_t>(index)]->UpdateMotions(deltaTime, parameters_);
    }
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

Vector3 BossSpider::UpdateBodyPosture(float deltaTime, bool isMoving) {
    (void)deltaTime;

    // 足の平均の高さに胴を乗せる（脚が持ち上がると胴もわずかに上がる）
    const float averageFootHeight = CalcFootAverageHeight();

    // 歩調に合わせた上下と左右の揺れ。止まっているときは揺らさない
    const float bob = isMoving ? std::sin(walkPhase_ * 3.0f) * parameters_.bodyBob : 0.0f;
    const float sway = isMoving ? std::sin(walkPhase_ * 1.5f) * parameters_.bodySway : 0.0f;

    const Vector3 right{std::cos(bodyYaw_ + std::numbers::pi_v<float> * 0.5f), 0.0f,
                        std::sin(bodyYaw_ + std::numbers::pi_v<float> * 0.5f)};

    bodyPosition_.y = averageFootHeight + parameters_.bodyHeight;

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


BulletHitResult BossSpider::RaycastAttach(const Vector3 &worldStart, const Vector3 &worldEnd, Color color) {
    BulletHitResult result{};
    // 変形が終わるまでは当たり判定を持たない（脚が生えている最中に撃たれても困る）
    if (phase_ != Phase::Active) {
        return result;
    }

    // いちばん手前で当たった脚を選ぶ
    int hitLeg = -1;
    float nearest = 0.0f;
    Vector3 hitPoint{};
    for (int index = 0; index < activeLegCount_; ++index) {
        float distance = 0.0f;
        Vector3 point{};
        if (!legs_[static_cast<size_t>(index)]->Raycast(worldStart, worldEnd, parameters_, distance, point)) {
            continue;
        }
        if (hitLeg < 0 || distance < nearest) {
            hitLeg = index;
            nearest = distance;
            hitPoint = point;
        }
    }
    if (hitLeg < 0) {
        return result;
    }

    result.hit = true;
    result.hitPoint = hitPoint;

    BossSpiderLeg *leg = legs_[static_cast<size_t>(hitLeg)].get();
    result.attached = leg->Attach(color, hitPoint, palette_, parameters_, effect_);
    if (!result.attached) {
        return result; // これ以上伸ばせない（弾は当たったので消える）
    }

    // 先端に同じ色がそろっていたら、そのぶんだけ脚が縮む
    const int destroyed = leg->TryEliminate(chain_.minMatch, effect_);
    if (destroyed > 0) {
        result.destroyed = true;
        result.clusterSize = destroyed;
        result.staggerTime = chain_.staggerBase +
                             chain_.staggerPerPart * static_cast<float>(destroyed - chain_.minMatch);
    }
    return result;
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
    if (ImGui::Button("変形を再生")) {
        // 球体形態のコアと同じ大きさ・同じ接地高さから始めて、変形だけを確かめる
        Awaken(Vector3{0.0f, parameters_.bodyRadius, 0.0f}, parameters_.bodyRadius);
    }
    ImGui::SameLine();
    if (ImGui::Button("変形を飛ばす")) {
        Awaken(Vector3{0.0f, parameters_.bodyRadius, 0.0f}, parameters_.bodyRadius);
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
