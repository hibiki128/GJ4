#include "Boss.h"
#include "src/Boss/Attack/BossAttackSlam.h"
#include "src/Boss/Attack/BossAttackSpin.h"
#include "src/Audio/GameSounds.h"
#include "src/Boss/Effect/BossParticles.h"
#include "src/Boss/State/BossStates.h"
#include "collider/ColliderTagManager.h"
#include "debug/imgui/ImGuiNotification.h"
#include "debug/log/Logger.h"
#include "debug/param/GameParamHub.h"
#include "frame/Frame.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

using namespace Hagine;

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

Boss::~Boss() {
    // GameParamHub の解除は params_ のデストラクタが行う
}

void Boss::Init(const std::string objectName) {
    RegisterGameTags();

    // --- データ読み込み（色マスタ → ボス個別データ → 使用色サブセット）---
    palette_.LoadMaster();
    parameters_.Load(bossId_);
    palette_.SetUsedColors(parameters_.GetUsedColors());

    // --- コア（内側の球）を生成 ---
    BaseObject::Init(objectName);
    CreateModel("boss/boss.obj");

    // パーツを組む前に登録する。保存済みの実行時調整値があればここで反映され、
    // その値でパーツが作られる
    RegisterTuningParameters();

    const BossShellParams &shell = parameters_.Shell();

    // シーンに保存済みの配置があればそれを尊重し、無いときだけ既定値を入れる
    if (!objectData_ || !objectData_->Contains("translation")) {
        transform_->translation_ = Vector3{0.0f, GetBodyRadius() + shell.groundOffset, 0.0f};
    }
    if (!objectData_ || !objectData_->Contains("scale")) {
        const float coreSize = (shell.shellRadius - cluster_.GetSphereRadius()) * shell.coreScale;
        transform_->scale_ = Vector3{coreSize, coreSize, coreSize};
    }
    transform_->UpdateMatrix();
    SetTexture(kBossTexturePath);
    SetColor(Vector4{0.16f, 0.16f, 0.20f, 1.0f});

    // --- 殻の球を生成して自分にぶら下げる ---
    // 殻の見た目はメタボール（同色が融合した1枚のメッシュ）なので、その設定を先に渡す
    cluster_.SetMetaBallParams(parameters_.MetaBall());
    cluster_.Build(this, objectName + "Sphere", shell, palette_, parameters_.Chain(),
                   parameters_.GetColorSeed());

    staggerTimer_ = 0.0f;
    homePosition_ = transform_->translation_;

    SetupStatesAndAttacks();

    Logger::Info("Boss: " + bossId_ + " を生成しました（球 " +
                 std::to_string(cluster_.GetInitialCount()) + "個 / 使用色 " +
                 std::to_string(palette_.GetUsedColors().size()) + "色）");
}

void Boss::Update() {
    BaseObject::Update();

    if (isPaused_) {
        // 止めているあいだも、殻の見た目だけは作り直しておく
        // （ImGui で殻の設定をいじったとき、その場で反映されるように）
        cluster_.Update();
        return;
    }

    const float deltaTime = Frame::DeltaTime();

    // 第2形態が出ているあいだ、この形態は描いていない。
    // 攻撃まで続けると、姿の見えないまま突進や落下でプレイヤーを殴ってしまい、
    // 予告線や着弾の警告表示だけが地面に出る。動く処理はここで止める。
    //
    // ただし球まわりの後片付けは進める。ここを飛ばすと、消えかけの球が
    // 消え切らないまま残ってプールへも戻らない
    if (!formVisible_) {
        if (pCurrentAttack_) {
            // 進行中の攻撃を畳む（予告線・警告表示もここで消える）
            EndCurrentAttack();
            RequestState(BossStateId::Idle);
        }
        cluster_.SetEffectParams(parameters_.Effect());
        cluster_.UpdateMotions(deltaTime);
        cluster_.Update();
        return;
    }

    if (staggerTimer_ > 0.0f) {
        staggerTimer_ = (std::max)(0.0f, staggerTimer_ - deltaTime);
    }

    // 撃破・怯みは進行中の状態に割り込む（遷移は次の更新の先頭で適用される）
    if (IsDead()) {
        if (stateMachine_.GetCurrentId() != BossStateId::Dead) {
            stateMachine_.Request(BossStateId::Dead);
        }
    } else if (IsStaggered() && stateMachine_.GetCurrentId() != BossStateId::Stagger) {
        stateMachine_.Request(BossStateId::Stagger);
    }

    // 実行時に演出パラメータを変えてもすぐ効くよう、毎フレーム渡す
    cluster_.SetEffectParams(parameters_.Effect());
    // 吸着・消滅の演出を進める（消え切った球はここでプールへ戻る）
    cluster_.UpdateMotions(deltaTime);

    // 削れるほど攻撃が早く・激しくなる
    UpdateExposureScaling();

    stateMachine_.Update(*this, deltaTime);
    ClampToArena();

    // 殻の見た目を更新する。球が増減した色だけメッシュを作り直すので、
    // 動いている・回っているだけのフレームでは何も起きない
    cluster_.Update();

    if (drawGraphDebug_) {
        // 線はフレーム単位で積み上げるので、描画フェーズではなく更新中に積む
        cluster_.DebugDraw();
    }

    if (drawTargetDebug_) {
        DrawTargetDebug();
    }
}

void Boss::DispatchShellCompute() {
    // 描いていないなら殻のメッシュを作り直す必要もない
    if (!formVisible_) {
        return;
    }
    cluster_.DispatchCompute(Frame::DeltaTime());
}

void Boss::Draw(const ViewProjection &viewProjection) {
    // 第2形態が出ているあいだは、こちらは丸ごと描かない（コアは1つに見せる）
    if (!formVisible_) {
        return;
    }

    // コア（自分自身）→ パーツの順に描く。
    // 呼び出し元（BaseObjectManager::Draw）のインスタンシング収集範囲内なので、
    // 同じプリミティブを使うパーツはまとめて1ドローになる
    BaseObject::Draw(viewProjection);
    cluster_.Draw(viewProjection);

    // 攻撃中だけ出る表示物（落下攻撃の着弾予告など）
    if (pCurrentAttack_) {
        pCurrentAttack_->Draw(viewProjection);
    }
}

/// ===================================================
/// 状態・攻撃
/// ===================================================

void Boss::SetupStatesAndAttacks() {
    stateMachine_.Register(std::make_unique<BossStateAppear>());
    stateMachine_.Register(std::make_unique<BossStateIdle>());
    stateMachine_.Register(std::make_unique<BossStateAttack>());
    stateMachine_.Register(std::make_unique<BossStateStagger>());
    stateMachine_.Register(std::make_unique<BossStateDead>());

    // 攻撃はパラメータを参照で受け取るので、実行時に値を変えると即反映される
    scheduler_.AddAttack(std::make_unique<BossAttackSpin>(&parameters_.Spin(), &parameters_.WallStagger(),
                                                        &parameters_.Exposure()));
    scheduler_.AddAttack(std::make_unique<BossAttackSlam>(&parameters_.Slam(), &parameters_.Exposure()));
    UpdateExposureScaling();
    scheduler_.Reset();

    stateMachine_.Start(*this, BossStateId::Appear);
}

/// ===================================================
/// 登場演出
/// ===================================================

void Boss::BeginAppear() {
    appearTime_ = 0.0f;
    // 配色シードと変えておく（同じ並びで散らばると規則的に見えるため）
    cluster_.BeginAppear(parameters_.Appear(), parameters_.GetColorSeed() + 1u);
}

bool Boss::UpdateAppear(float deltaTime) {
    const BossAppearParams &appear = parameters_.Appear();
    appearTime_ += deltaTime;

    cluster_.UpdateAppear(appear, appearTime_);

    // 自転は「集束中は高速 → 回転が収まるまで滑らかに減速 → 通常速度」
    const float idleSpin = parameters_.Battle().idleSpinSpeed;
    float spinSpeed = idleSpin;
    if (appearTime_ < appear.gatherTime) {
        spinSpeed = appear.gatherSpinSpeed;
    } else if (appearTime_ < appear.gatherTime + appear.settleTime) {
        const float settle = std::clamp((appearTime_ - appear.gatherTime) /
                                            (std::max)(0.01f, appear.settleTime),
                                        0.0f, 1.0f);
        spinSpeed = ApplyEasing(EasingType::OutCubic, appear.gatherSpinSpeed, idleSpin, settle, 1.0f);
    }
    AddSpin(spinSpeed * deltaTime);

    return appearTime_ < BossSphereCluster::GetAppearDuration(appear);
}

void Boss::EndAppear() {
    // 端数の時間で中途半端な大きさのまま止まらないよう、最終状態へそろえる
    cluster_.FinishAppear();
    // 登場直後にいきなり攻撃しないよう、間隔を取り直す
    scheduler_.Reset();
}

void Boss::AddIdleSpin(float deltaTime) {
    AddSpin(parameters_.Battle().idleSpinSpeed * deltaTime);
}

bool Boss::TickAttackCoolDown(float deltaTime) {
    // 攻撃を止められているあいだは、待ち時間も進めず新しい攻撃も選ばない
    if (!isAttackEnabled_) {
        return false;
    }
    // 調整UIから攻撃を名指しされているときは、間隔を待たずに始める
    if (forcedAttackIndex_ >= 0) {
        return true;
    }
    return scheduler_.TickCoolDown(deltaTime);
}

void Boss::StartScheduledAttack() {
    if (forcedAttackIndex_ >= 0) {
        pCurrentAttack_ = scheduler_.GetAttack(static_cast<size_t>(forcedAttackIndex_));
        forcedAttackIndex_ = -1;
    } else {
        pCurrentAttack_ = scheduler_.PickNext();
    }
    if (pCurrentAttack_) {
        pCurrentAttack_->Start(MakeAttackContext(0.0f));
    }
}

bool Boss::UpdateCurrentAttack(float deltaTime) {
    if (!pCurrentAttack_) {
        return false;
    }
    pCurrentAttack_->Update(MakeAttackContext(deltaTime));
    return !pCurrentAttack_->IsFinished();
}

void Boss::EndCurrentAttack() {
    if (pCurrentAttack_) {
        // 怯みで割り込まれた場合はここが中断処理になる
        const bool completed = pCurrentAttack_->IsFinished();
        if (!completed) {
            pCurrentAttack_->Cancel(MakeAttackContext(0.0f));
        }
        pCurrentAttack_ = nullptr;

        // やり切ったときだけ知らせる。中断（怯み・形態交代）で出してしまうと、
        // ボスを止めるほど回復エリアが増えて的にならなくなる
        if (completed && attackFinishedCallback_) {
            attackFinishedCallback_();
        }
    }
    scheduler_.NotifyAttackFinished();
}

BossAttackContext Boss::MakeAttackContext(float deltaTime) {
    BossAttackContext context{};
    context.boss = this;
    context.target = pTargetLocator_;
    context.deltaTime = deltaTime;
    // 攻撃のスケーリングには「到達できる上限で正規化した露出度」を渡す
    context.exposure = GetNormalizedExposure();
    return context;
}

float Boss::GetNormalizedExposure() const {
    // 弾を付着させて塊を育てられるため、殻はすべて削り切れる。
    // 素の割合をそのまま使えば 1.0 まで到達する
    return cluster_.GetExposure();
}

void Boss::UpdateExposureScaling() {
    const BossExposureParams &exposure = parameters_.Exposure();
    // 攻撃頻度は露出度から毎フレーム算出する（次のクールダウンから反映される）
    scheduler_.SetInterval(Lerp(exposure.attackIntervalAtZero, exposure.attackIntervalAtFull,
                                GetNormalizedExposure()));
}

void Boss::BeginWallStagger() {
    const BossWallStaggerParams &wall = parameters_.WallStagger();
    staggerKind_ = StaggerKind::Wall;
    // 動きが途中で切れないよう、3つの段階の合計をそのまま怯み時間にする
    staggerTimer_ = (std::max)(0.05f, wall.wobbleTime + wall.shakeTime + wall.settleTime);
    staggerShakeTime_ = 0.0f;
    // 連鎖のひるみと同じフレームに重なっても、揺らし方が混ざらないようにそろえる
    SetOffset(Vector3{0.0f, 0.0f, 0.0f});
}

bool Boss::IsBeyondBounds(const Vector3 &position) const {
    Vector3 fromHome{position.x - homePosition_.x, 0.0f, position.z - homePosition_.z};
    if (fromHome.Length() > parameters_.Battle().arenaRadius) {
        return true;
    }
    return pFieldBounds_ && !pFieldBounds_->Contains(position);
}

bool Boss::UpdateStaggerMotion(float deltaTime) {
    if (staggerKind_ == StaggerKind::Wall) {
        return UpdateWallStagger(deltaTime);
    }

    // --- 連鎖破壊のひるみ：小刻みに震えるだけ ---
    staggerShakeTime_ += deltaTime;
    // 描画オフセットだけを揺らす。当たり判定の位置は動かさない
    SetOffset(Vector3{std::sin(staggerShakeTime_ * 46.0f) * 0.25f, 0.0f,
                      std::cos(staggerShakeTime_ * 37.0f) * 0.18f});
    return IsStaggered();
}

bool Boss::UpdateWallStagger(float deltaTime) {
    const BossWallStaggerParams &wall = parameters_.WallStagger();

    // 入った最初のフレームで、ぶつかった地点をふらつきの中心として覚える。
    // 押し戻し（ClampToArena）が済んだあとの位置なので、壁にめり込まない
    if (staggerShakeTime_ <= 0.0f) {
        // 止まるのは「先端が壁に触れた」時点なので、中心は本体半径のぶん内側にある。
        // ふらつきの幅より広く空いているため、ここを中心にしても押し戻しには当たらない
        staggerAnchor_ = transform_->translation_;
    }
    staggerShakeTime_ += deltaTime;

    const float wobbleTime = (std::max)(0.01f, wall.wobbleTime);
    const float shakeTime = (std::max)(0.01f, wall.shakeTime);
    const float settleTime = (std::max)(0.01f, wall.settleTime);
    constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;

    // 首を振る角度。ここでは「振れ幅の係数」だけを段階ごとに決める
    float swing = 0.0f;
    // ふらつきの強さ（1で通常、0で止まっている）
    float wobble = 0.0f;

    if (staggerShakeTime_ < wobbleTime) {
        // 1) ぶつかった地点でふらふら揺れる。狙いを付けられる程度にゆっくり
        wobble = 1.0f;
    } else if (staggerShakeTime_ < wobbleTime + shakeTime) {
        // 2) 立ち直りの合図。首を横に振る（振り幅は減りながら収まる）
        const float progress = (staggerShakeTime_ - wobbleTime) / shakeTime;
        const float cycles = (std::max)(0.5f, wall.shakeCount);
        swing = std::sin(progress * cycles * 2.0f * std::numbers::pi_v<float>) * (1.0f - progress);
        // 振っているあいだにふらつきを引いていく
        wobble = 1.0f - progress;
    } else {
        // 3) ゆっくり元の姿勢へ戻る。
        // 「元の姿勢」＝止まっていた自転が通常の速さまで戻ること。
        // 一気に回し始めると首を振った意味が消えるので、時間をかけて上げていく
        const float progress =
            std::clamp((staggerShakeTime_ - wobbleTime - shakeTime) / settleTime, 0.0f, 1.0f);
        wobble = 0.0f;
        const float idleSpin = parameters_.Battle().idleSpinSpeed;
        AddSpin(ApplyEasing(EasingType::InQuad, 0.0f, idleSpin, progress, 1.0f) * deltaTime);
    }

    // 位置のふらつき。前後と左右で周期をずらすと、決まった円ではなく
    // 定まらない揺れになって「立ちくらみ」に見える。
    // どちらも sin なのは、ぶつかった瞬間に位置が飛ばないようにするため（0から始まる）。
    // 描画オフセットではなく本体の位置を動かすので、球の当たり判定も一緒に動く
    const float phase = staggerShakeTime_ * wall.wobbleSpeed;
    const Vector3 sway{std::sin(phase) * wall.wobbleAmount * wobble, 0.0f,
                       std::sin(phase * 0.73f) * wall.wobbleAmount * wobble};
    transform_->translation_ = staggerAnchor_ + sway;

    // 姿勢。ふらつきに合わせて傾け、立ち直りでは首を横（Y軸まわり）へ振る
    const Quaternion tilt =
        Quaternion::FromAxisAngle(Vector3{0.0f, 0.0f, 1.0f},
                                  std::sin(phase) * wall.wobbleTilt * kDegToRad * wobble);
    const Quaternion shake =
        Quaternion::FromAxisAngle(Vector3{0.0f, 1.0f, 0.0f}, swing * wall.shakeAngle * kDegToRad);
    staggerPosture_ = shake * tilt;
    ApplyRotation();

    // 頭の上を粒が回る。これが「いま殴っていい」の目印になる
    BossParticles::GetInstance()->UpdateStaggerRing(GetHeadCenter(), deltaTime);

    return staggerShakeTime_ < wobbleTime + shakeTime + settleTime;
}

void Boss::ClearStaggerShake() {
    staggerShakeTime_ = 0.0f;
    staggerKind_ = StaggerKind::Chain;
    staggerPosture_ = Quaternion::IdentityQuaternion();
    SetOffset(Vector3{0.0f, 0.0f, 0.0f});
    ApplyRotation();
    BossParticles::GetInstance()->StopStaggerRing();
}

void Boss::AddSpin(float degrees) {
    spinAngle_ += degrees * (std::numbers::pi_v<float> / 180.0f);
    ApplyRotation();
}

void Boss::ApplyRotation() {
    // 真上を軸にすると極のパーツが永久に見えないので、軸を少し傾けて回す
    const Vector3 axis = Vector3{0.25f, 1.0f, 0.15f}.Normalize();
    // 自転の上に姿勢（ひるみの傾き・首振り）を載せる。順番を逆にすると
    // 首振りが自転に巻き込まれて、振っているのか回っているのか分からなくなる
    transform_->quaternionRotation_ = staggerPosture_ * Quaternion::FromAxisAngle(axis, spinAngle_);
}

void Boss::SetBossPosition(const Vector3 &position) {
    transform_->translation_ = position;
}

void Boss::ClampToArena() {
    const float arenaRadius = parameters_.Battle().arenaRadius;

    Vector3 position = transform_->translation_;
    Vector3 offset{position.x - homePosition_.x, 0.0f, position.z - homePosition_.z};
    const float distance = offset.Length();
    if (distance > arenaRadius && distance > 0.0001f) {
        offset = offset / distance * arenaRadius;
        position.x = homePosition_.x + offset.x;
        position.z = homePosition_.z + offset.z;
    }
    // 地面より下へ潜らせない
    position.y = (std::max)(position.y, homePosition_.y);

    // 巣の範囲に収めたうえで、さらにフィールドの外へは出さない。
    // 巣がフィールドの端に寄っていても、外周をはみ出すのはこれで止まる
    if (pFieldBounds_) {
        pFieldBounds_->ClampToField(position);
    }

    transform_->translation_ = position;
}

bool Boss::IsTargetWithin(const Vector3 &center, float radius) const {
    if (!pTargetLocator_ || !pTargetLocator_->IsTargetValid()) {
        return false;
    }
    const Vector3 difference = pTargetLocator_->GetTargetPosition() - center;
    const float reach = radius + pTargetLocator_->GetTargetRadius();
    return difference.LengthSq() <= reach * reach;
}

bool Boss::DealDamageToTarget(float amount, const Vector3 &impactPoint) {
    DamageInfo info{};
    info.amount = amount;
    info.hitPoint = impactPoint;

    if (pTargetDamageSink_) {
        pTargetDamageSink_->ApplyDamage(info);
        return true;
    }

    // プレイヤー側に受け口が実装されるまでは、当たったことだけ分かるようにしておく
    ImGuiNotification::Post("ボスの攻撃がヒット（プレイヤー側の受け口が未接続）",
                            {1.0f, 0.55f, 0.3f, 1.0f});
    return false;
}

void Boss::ApplyDamage(const DamageInfo &info) {
    // このボスはHPを削って倒すのではなく、殻の球をすべて破壊するのが撃破条件。
    // 球を減らすのは同色消去そのものなので、ここでは怯みだけを受け取る
    if (IsDead()) {
        return;
    }

    // 連続ヒットで怯みが短くならないよう、長い方を採用する
    staggerTimer_ = (std::max)(staggerTimer_, info.staggerTime);
}

bool Boss::FindLockOnTarget(const LockOnRequest &request, LockOnResult &out) {
    // 登場演出の最中は球が定位置にいないので狙わせない
    if (IsAppearing()) {
        out = LockOnResult{};
        return false;
    }
    return cluster_.FindLockOnTarget(request, parameters_.LockOn().requireFacing, out);
}

BulletHitResult Boss::RaycastAttach(const Vector3 &worldStart, const Vector3 &worldEnd, Color color) {
    // 登場演出の最中は無敵（球が飛来中で当たり判定の位置が定まらない）
    if (IsAppearing()) {
        return BulletHitResult{};
    }

    const bool wasAlive = !IsDead();

    const BulletHitResult result =
        cluster_.RaycastAttach(worldStart, worldEnd, color, parameters_.Chain(), palette_);

    // 消去が起きたぶんだけ怯みが入る（付着しただけなら何も起きない）
    if (result.destroyed) {
        // そろって消えた合図。短い間に何度も起きるので、鳴らし直しの間隔で重なりを防ぐ
        GameSounds::GetInstance()->Play(GameSounds::Id::Break);
        DamageInfo info{};
        info.hitPoint = result.hitPoint;
        info.chainSize = result.clusterSize;
        info.staggerTime = result.staggerTime;
        ApplyDamage(info);

        // 殻の球をすべて破壊し切ったら撃破
        if (wasAlive && IsDead()) {
            ImGuiNotification::Post("ボス撃破（殻をすべて破壊）", {1.0f, 0.85f, 0.3f, 1.0f});
            Logger::Info("Boss: " + bossId_ + " 撃破（殻の球をすべて破壊）");
        }
    }

    return result;
}

bool Boss::RaycastPoint(const Vector3 &worldStart, const Vector3 &worldEnd, Color color, AimHit &outHit) {
    // 当たり判定を持たない間は照準も素通りさせる（RaycastAttach と同じ条件にそろえる）
    if (IsAppearing()) {
        return false;
    }

    // 殻の球は色に関係なく弾を止めるので、色は見ない
    (void)color;
    ShellCell hitCell{};
    if (!cluster_.RaycastPoint(worldStart, worldEnd, outHit.point, &hitCell)) {
        return false;
    }

    // エイムアシストの吸着先は当たった球の中心。
    // 消える途中などで座標が引けなければ、表面の点をそのまま中心として返す（＝寄らない）
    if (!cluster_.TryGetCellWorldPosition(hitCell, outHit.center)) {
        outHit.center = outHit.point;
    }
    return true;
}

bool Boss::TryGetTargetPosition(const ShellCell &cell, Vector3 &out) {
    return cluster_.TryGetCellWorldPosition(cell, out);
}

void Boss::ResetBoss() {
    cluster_.ResetAll(parameters_.Shell(), palette_, parameters_.Chain(), parameters_.GetColorSeed());
    staggerTimer_ = 0.0f;

    pCurrentAttack_ = nullptr;
    UpdateExposureScaling();
    scheduler_.Reset();
    ClearStaggerShake();
    SetBossPosition(homePosition_);
    stateMachine_.Start(*this, BossStateId::Appear);
}

void Boss::ApplyShellChanges() {
    // 球の大きさだけの変更なら、位置と見た目を引き直すだけで済む
    ApplyCoreLayout();
    cluster_.ApplyRadius(parameters_.Shell());
}

void Boss::RebuildShell() {
    ApplyCoreLayout();
    cluster_.Build(this, objectName_ + "Sphere", parameters_.Shell(), palette_, parameters_.Chain(),
                   parameters_.GetColorSeed());

    UpdateExposureScaling();
}

float Boss::ApplyMasterScale(float scale) {
    const float next = std::clamp(scale, 0.1f, 5.0f);
    const float previous = (std::max)(0.01f, parameters_.GetMasterScale());
    const float ratio = next / previous;
    if (std::abs(ratio - 1.0f) < 0.0001f) {
        return 1.0f;
    }

    // 長さにあたる値をまとめて掛ける（殻の半径・攻撃の届く範囲・接地の余白など）。
    // コアの大きさは (殻の半径 - 球の半径) × coreScale で出しているので一緒に付いてくる
    parameters_.ScaleLengths(ratio);
    parameters_.SetMasterScale(next);

    // 球は作り直さず、格子の間隔と半径を引き直すだけ。
    // 接地高さ（homePosition_.y）もここで新しい外周半径へ合わせ直される
    ApplyShellChanges();
    return ratio;
}

void Boss::ApplyCoreLayout() {
    const BossShellParams &shell = parameters_.Shell();

    // コアの大きさと接地高さを殻へ追従させる
    const float coreSize = (shell.shellRadius - cluster_.GetSphereRadius()) * shell.coreScale;
    transform_->scale_ = Vector3{coreSize, coreSize, coreSize};
    homePosition_.y = GetBodyRadius() + shell.groundOffset;

    // 攻撃で浮いている最中に高さを合わせると落下が破綻するので、そのときは触らない
    if (stateMachine_.GetCurrentId() != BossStateId::Attack) {
        Vector3 position = transform_->translation_;
        position.y = homePosition_.y;
        transform_->translation_ = position;
    }
    transform_->UpdateMatrix();
}

void Boss::RegisterGameTags() {
    // エンジンはゲームのタグ名を知らないので、ゲーム側から登録する。
    // 登録前に SetTag / AddCollisionMask を呼んでも無視されるため必ず先に行う
    ColliderTagManager::GetInstance()->RegisterGameTags({kBossTag, kBossPartTag, kPlayerBulletTag});
}

void Boss::RegisterTuningParameters() {
    // ボスIDが決まったこの時点でオーナーを確定させる。以降は名前と変数だけで登録できる
    params_.SetOwner("Boss/" + bossId_);
    BossChainParams &chain = parameters_.Chain();
    BossLockOnParams &lockOn = parameters_.LockOn();
    BossShellParams &shell = parameters_.Shell();

    // --- 殻の形 ---
    //
    // 大きさそのもの（基本殻の半径・球の半径・接地高さの調整）はここへ登録しない。
    // GameParamHub は起動時に自分の保存値を書き戻すので、登録すると
    // Boss01.json 側の「全体の倍率を掛けたあとの大きさ」が毎回上書きされ、
    // 倍率の値だけ残って見た目が元に戻ってしまう。
    // これらはボスのパネル（殻の形）から触れて、Boss01.json に保存される
    GameParamHub::Options layerOptions{};
    layerOptions.speed = 1.0f;
    layerOptions.min = 0.0f;
    layerOptions.max = 5.0f;
    layerOptions.onChange = [this] { RebuildShell(); };
    params_.Register("殻:分割数(0=12/1=42/2=162)", &shell.subdivision, layerOptions);
    params_.Register("殻:外側へ付着できる層数", &shell.outerLayers, layerOptions);
    params_.Register("殻:内側へ付着できる層数", &shell.innerLayers, layerOptions);

    GameParamHub::Options coreOptions{};
    coreOptions.speed = 0.01f;
    coreOptions.min = 0.1f;
    coreOptions.max = 1.2f;
    coreOptions.onChange = [this] { ApplyShellChanges(); };
    params_.Register("殻:コアの大きさ", &shell.coreScale, coreOptions);

    // --- 殻の見た目（メタボール）。変えた色のメッシュだけ作り直される ---
    BossMetaBallParams &metaBall = parameters_.MetaBall();
    GameParamHub::Options metaBallOptions{};
    metaBallOptions.speed = 0.01f;
    metaBallOptions.min = 0.05f;
    metaBallOptions.max = 4.0f;
    metaBallOptions.onChange = [this] { cluster_.SetMetaBallParams(parameters_.MetaBall()); };
    params_.Register("メタボール:影響半径の倍率", &metaBall.influenceScale, metaBallOptions);

    GameParamHub::Options voxelOptions = metaBallOptions;
    voxelOptions.min = 0.1f;
    voxelOptions.max = 1.0f;
    params_.Register("メタボール:セルの細かさ", &metaBall.voxelRatio, voxelOptions);

    GameParamHub::Options thresholdOptions = metaBallOptions;
    thresholdOptions.min = 0.05f;
    thresholdOptions.max = 2.0f;
    params_.Register("メタボール:しきい値", &metaBall.threshold, thresholdOptions);

    GameParamHub::Options highlightOptions = metaBallOptions;
    highlightOptions.min = 1.0f;
    highlightOptions.max = 2.0f;
    params_.Register("メタボール:強調球の倍率", &metaBall.highlightScale, highlightOptions);

    // 脈動（GPU生成のときだけ効く）
    GameParamHub::Options wobbleOptions = metaBallOptions;
    wobbleOptions.speed = 0.005f;
    wobbleOptions.min = 0.0f;
    wobbleOptions.max = 0.5f;
    params_.Register("メタボール:脈動の振幅", &metaBall.wobbleAmplitude, wobbleOptions);

    GameParamHub::Options wobbleSpeedOptions = metaBallOptions;
    wobbleSpeedOptions.speed = 0.05f;
    wobbleSpeedOptions.min = 0.0f;
    wobbleSpeedOptions.max = 20.0f;
    params_.Register("メタボール:脈動の速さ", &metaBall.wobbleSpeed, wobbleSpeedOptions);
    params_.Register("メタボール:脈動のばらけ", &metaBall.wobbleFrequency, wobbleSpeedOptions);

    // 上限を変えると頂点バッファを作り直すので、GPU の完了待ちが1回入る
    GameParamHub::Options budgetOptions{};
    budgetOptions.speed = 500.0f;
    budgetOptions.min = 1000.0f;
    budgetOptions.max = 200000.0f;
    budgetOptions.onChange = metaBallOptions.onChange;
    params_.Register("メタボール:1色の三角形上限", &metaBall.maxTrianglesPerColor, budgetOptions);

    params_.Register("連鎖:最低連結数", &chain.minMatch, {1.0f, 2.0f, 8.0f});
    params_.Register("連鎖:基礎怯み時間", &chain.staggerBase, {0.01f, 0.0f, 5.0f});
    params_.Register("連鎖:1つあたり怯み加算", &chain.staggerPerPart, {0.01f, 0.0f, 2.0f});
    params_.Register("連鎖:初期塊の上限", &chain.maxInitialCluster, {1.0f, 0.0f, 60.0f});
    params_.Register("ロックオン:許容角度", &lockOn.maxAngleDegrees, {0.5f, 0.0f, 90.0f});
    params_.Register("ロックオン:有効距離", &lockOn.maxDistance, {0.5f, 0.0f, 300.0f});
    params_.Register("ロックオン:表面のみ狙う", &lockOn.requireFacing);

    // --- 戦闘・攻撃 ---
    BossBattleParams &battle = parameters_.Battle();
    BossSpinAttackParams &spin = parameters_.Spin();
    BossSlamAttackParams &slam = parameters_.Slam();

    params_.Register("戦闘:行動範囲", &battle.arenaRadius, {0.5f, 1.0f, 200.0f});
    params_.Register("戦闘:待機時の自転速度", &battle.idleSpinSpeed, {0.5f, 0.0f, 360.0f});

    params_.Register("突進:予兆時間", &spin.telegraphTime, {0.01f, 0.05f, 5.0f});
    params_.Register("突進:予兆の自転速度", &spin.telegraphSpinSpeed, {5.0f, 0.0f, 2000.0f});
    params_.Register("突進:速度", &spin.dashSpeed, {0.2f, 0.0f, 100.0f});
    params_.Register("突進:時間", &spin.dashTime, {0.01f, 0.05f, 5.0f});
    params_.Register("突進:硬直", &spin.recoverTime, {0.01f, 0.0f, 5.0f});
    params_.Register("突進:ダメージ", &spin.damage, {0.5f, 0.0f, 200.0f});

    BossWallStaggerParams &wallStagger = parameters_.WallStagger();
    params_.Register("壁ひるみ:ふらつく時間", &wallStagger.wobbleTime, {0.05f, 0.05f, 15.0f});
    params_.Register("壁ひるみ:ふらつきの速さ", &wallStagger.wobbleSpeed, {0.05f, 0.0f, 20.0f});
    params_.Register("壁ひるみ:傾く角度", &wallStagger.wobbleTilt, {0.5f, 0.0f, 90.0f});
    params_.Register("壁ひるみ:首を振る時間", &wallStagger.shakeTime, {0.05f, 0.05f, 5.0f});
    params_.Register("壁ひるみ:首を振る角度", &wallStagger.shakeAngle, {0.5f, 0.0f, 120.0f});
    params_.Register("壁ひるみ:首を振る回数", &wallStagger.shakeCount, {0.1f, 0.5f, 8.0f});
    params_.Register("壁ひるみ:元へ戻る時間", &wallStagger.settleTime, {0.05f, 0.05f, 6.0f});
    params_.Register("壁ひるみ:必要な突進距離", &wallStagger.minTravel, {0.1f, 0.0f, 60.0f});

    // 届く範囲（当たりの甘さ・落下の高さと有効半径）はここへ登録しない。
    // 全体の倍率が掛かる値なので、登録すると起動のたびに GameParamHub の保存値へ
    // 戻されて、倍率が効いていないように見える。触るのはボスのパネルの「攻撃」から
    params_.Register("落下:飛び上がり時間", &slam.riseTime, {0.01f, 0.05f, 5.0f});
    params_.Register("落下:狙いの時間", &slam.aimTime, {0.01f, 0.0f, 5.0f});
    params_.Register("落下:落下時間", &slam.fallTime, {0.01f, 0.05f, 5.0f});
    params_.Register("落下:着弾後の静止", &slam.impactTime, {0.01f, 0.0f, 5.0f});
    params_.Register("落下:ダメージ", &slam.damage, {0.5f, 0.0f, 200.0f});
    params_.Register("落下:硬直", &slam.recoverTime, {0.01f, 0.0f, 5.0f});

    // --- 吸着・消滅の演出 ---
    BossEffectParams &effect = parameters_.Effect();
    params_.Register("演出:吸着の時間", &effect.attachTime, {0.005f, 0.01f, 2.0f});
    params_.Register("演出:吸着開始の大きさ", &effect.attachStartScale, {0.01f, 0.01f, 1.0f});
    params_.Register("演出:消えるまでの時間", &effect.vanishTime, {0.005f, 0.02f, 2.0f});
    params_.Register("演出:消える順番の時間差", &effect.vanishSpread, {0.005f, 0.0f, 0.5f});

    // --- 登場演出 ---
    BossAppearParams &appear = parameters_.Appear();
    params_.Register("登場:集束の時間", &appear.gatherTime, {0.05f, 0.1f, 10.0f});
    params_.Register("登場:回転が収まる時間", &appear.settleTime, {0.01f, 0.0f, 5.0f});
    params_.Register("登場:膨らむ時間", &appear.expandTime, {0.01f, 0.05f, 5.0f});
    params_.Register("登場:集束中の自転速度", &appear.gatherSpinSpeed, {5.0f, 0.0f, 3000.0f});
    params_.Register("登場:飛来中の大きさ", &appear.startScale, {0.01f, 0.01f, 1.0f});
    params_.Register("登場:到着時の大きさ", &appear.arriveScale, {0.01f, 0.01f, 1.0f});
    params_.Register("登場:到着のばらつき", &appear.spawnSpread, {0.01f, 0.0f, 3.0f});

    // --- 露出度スケーリング（難易度カーブ）---
    BossExposureParams &exposure = parameters_.Exposure();
    params_.Register("露出度:攻撃間隔(露出0)", &exposure.attackIntervalAtZero, {0.05f, 0.2f, 30.0f});
    params_.Register("露出度:攻撃間隔(露出1)", &exposure.attackIntervalAtFull, {0.05f, 0.2f, 30.0f});
    params_.Register("露出度:突進速度の倍率(露出1)", &exposure.spinDashSpeedScaleAtFull, {0.01f, 0.1f, 5.0f});
    params_.Register("露出度:突進予兆の倍率(露出1)", &exposure.spinTelegraphScaleAtFull, {0.01f, 0.1f, 2.0f});
    params_.Register("露出度:落下回数(露出0)", &exposure.slamCountAtZero, {1.0f, 1.0f, 12.0f});
    params_.Register("露出度:落下回数(露出1)", &exposure.slamCountAtFull, {1.0f, 1.0f, 12.0f});
    params_.Register("露出度:狙い時間の倍率(露出1)", &exposure.slamAimScaleAtFull, {0.01f, 0.1f, 2.0f});
}

void Boss::DrawImGui() {
    // 「トランスフォームマネージャ」ウィンドウで選択したときに出る、オブジェクトの全項目
    BaseObject::DrawImGui();
    DrawGameplayImGui();
}

void Boss::DrawGameplayImGui() {
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader("ボス", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    // 撃破条件は「中心のコアを除く色付きの球をすべて破壊すること」。
    // 残りの球数がそのまま撃破までの進捗になる
    const float initialCount = (std::max)(1.0f, GetMaxHp());
    if (!formVisible_) {
        ImGui::TextColored(ImVec4{1.0f, 0.8f, 0.3f, 1.0f}, "第2形態が出ているので、この形態は描いていません");
    }
    ImGui::Text("残りの球: %.0f / %.0f %s", GetHp(), initialCount, IsDead() ? "（撃破）" : "");
    ImGui::ProgressBar(1.0f - GetHp() / initialCount, ImVec2(-1.0f, 0.0f), "破壊した割合");

    ImGui::Text("露出度: %.1f%%  (球 %d / 初期 %d ・プール上限 %d)", cluster_.GetExposure() * 100.0f,
                cluster_.GetOccupiedCount(), cluster_.GetInitialCount(), cluster_.GetCapacity());
    ImGui::Text("現在の攻撃間隔: %.2f 秒", scheduler_.GetInterval());
    ImGui::Text("怯み残り: %.2f 秒", staggerTimer_);

    ImGui::SeparatorText("状態");
    ImGui::Text("現在: %s（%.2f 秒経過）", GetStateName(), stateMachine_.GetElapsed());
    if (pCurrentAttack_) {
        ImGui::Text("攻撃中: %s ／ %s", pCurrentAttack_->GetName(), pCurrentAttack_->GetPhaseName());
    } else {
        ImGui::Text("次の攻撃まで: %.2f 秒", scheduler_.GetRemainingCoolDown());
    }
    if (ImGui::Button("今すぐ攻撃")) {
        scheduler_.ForceReady();
    }
    ImGui::SameLine();
    if (ImGui::Button("登場演出を再生")) {
        RequestState(BossStateId::Appear);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("攻撃の当て先: %s", pTargetDamageSink_ ? "接続済み" : "未接続（通知のみ）");


    // 攻撃が「相手を狙えているか」を数字で出す。
    // 位置が動かない・向きが変わらない場合、まずここを見れば切り分けられる
    const Vector3 bossPosition = transform_->translation_;
    ImGui::Text("ボスの位置: (%.2f, %.2f, %.2f)", bossPosition.x, bossPosition.y, bossPosition.z);
    if (pTargetLocator_ && pTargetLocator_->IsTargetValid()) {
        const Vector3 targetPosition = pTargetLocator_->GetTargetPosition();
        Vector3 toTarget = targetPosition - bossPosition;
        toTarget.y = 0.0f;
        ImGui::Text("相手の位置: (%.2f, %.2f, %.2f)", targetPosition.x, targetPosition.y, targetPosition.z);
        ImGui::Text("相手までの距離: %.2f ／ 向き (%.2f, %.2f)", toTarget.Length(),
                    toTarget.LengthSq() > 0.0001f ? toTarget.Normalize().x : 0.0f,
                    toTarget.LengthSq() > 0.0001f ? toTarget.Normalize().z : 0.0f);
    } else {
        ImGui::TextColored(ImVec4{1.0f, 0.4f, 0.3f, 1.0f}, "相手が未接続（攻撃が狙いを付けられません）");
    }
    ImGui::Text("行動範囲: 中心 (%.2f, %.2f) 半径 %.2f", homePosition_.x, homePosition_.z,
                parameters_.Battle().arenaRadius);
    {
        Vector3 fromHome{bossPosition.x - homePosition_.x, 0.0f, bossPosition.z - homePosition_.z};
        const float distance = fromHome.Length();
        ImGui::SameLine();
        if (distance >= parameters_.Battle().arenaRadius - 0.01f) {
            ImGui::TextColored(ImVec4{1.0f, 0.8f, 0.3f, 1.0f}, "（範囲の端で止まっています）");
        } else {
            ImGui::TextDisabled("（中心から %.2f）", distance);
        }
    }

    ImGui::SeparatorText("攻撃");
    BossSpinAttackParams &spin = parameters_.Spin();
    BossSlamAttackParams &slam = parameters_.Slam();
    for (size_t index = 0; index < scheduler_.GetAttackCount(); ++index) {
        IBossAttack *attack = scheduler_.GetAttack(index);
        if (!attack) {
            continue;
        }
        if (index > 0) {
            ImGui::SameLine();
        }
        if (ImGui::Button(attack->GetName())) {
            // 次に始める攻撃をこれに決めて、いったん待機へ抜ける。
            // 進行中のものは待機へ移るときに畳まれる（中断処理は EndCurrentAttack が持っている）
            forcedAttackIndex_ = static_cast<int>(index);
            RequestState(BossStateId::Idle);
        }
    }

    if (ImGui::TreeNode("1. 回転突進")) {
        ImGui::DragFloat("予兆の時間", &spin.telegraphTime, 0.01f, 0.05f, 5.0f);
        HelpMarker("その場で自転を上げて溜める時間です。終盤で突進方向が固定されるので、\n"
                   "長いほど横へ抜ける余裕が生まれます");
        ImGui::DragFloat("予兆の自転速度", &spin.telegraphSpinSpeed, 5.0f, 0.0f, 2000.0f);
        ImGui::DragFloat("突進の速さ", &spin.dashSpeed, 0.2f, 0.0f, 100.0f);
        ImGui::DragFloat("突進の時間", &spin.dashTime, 0.01f, 0.05f, 5.0f);
        ImGui::DragFloat("突進後の硬直", &spin.recoverTime, 0.01f, 0.0f, 5.0f);
        ImGui::DragFloat("接触ダメージ", &spin.damage, 0.5f, 0.0f, 200.0f);
        ImGui::DragFloat("当たりの甘さ", &spin.contactMargin, 0.05f, 0.0f, 10.0f);
        HelpMarker("本体の半径への上乗せです。大きいほどかすっても当たります");
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("2. 飛び上がって落下")) {
        ImGui::DragFloat("飛び上がる時間", &slam.riseTime, 0.01f, 0.05f, 5.0f);
        ImGui::DragFloat("飛び上がる高さ", &slam.riseHeight, 0.2f, 1.0f, 80.0f);
        ImGui::DragFloat("狙いを定める時間", &slam.aimTime, 0.01f, 0.0f, 5.0f);
        HelpMarker("落下地点は動き出しの時点で決まっているので、ここは逃げるための猶予です");
        ImGui::DragFloat("落下の時間", &slam.fallTime, 0.01f, 0.05f, 5.0f);
        ImGui::DragFloat("着弾後の静止", &slam.impactTime, 0.01f, 0.0f, 5.0f);
        ImGui::DragFloat("着弾の有効半径", &slam.impactRadius, 0.1f, 0.5f, 40.0f);
        ImGui::DragFloat("着弾ダメージ", &slam.damage, 0.5f, 0.0f, 200.0f);
        ImGui::DragFloat("最後の硬直", &slam.recoverTime, 0.01f, 0.0f, 5.0f);
        ImGui::TreePop();
    }

    ImGui::SeparatorText("壁に激突したときのひるみ");
    BossWallStaggerParams &wall = parameters_.WallStagger();
    if (IsStaggered() || staggerShakeTime_ > 0.0f) {
        ImGui::TextColored(ImVec4{1.0f, 0.85f, 0.3f, 1.0f}, "ひるみ中: %.2f 秒経過（狙い撃ちのチャンス）",
                           staggerShakeTime_);
    } else if (ImGui::Button("壁ひるみを再生")) {
        BeginWallStagger();
    }
    HelpMarker("突進がフィールドの壁で止まると自動で起きます。\n"
               "自転が止まるので、狙った色の球を撃ち抜けます");
    ImGui::DragFloat("ふらつく時間", &wall.wobbleTime, 0.05f, 0.05f, 15.0f);
    ImGui::DragFloat("ふらつきの大きさ", &wall.wobbleAmount, 0.01f, 0.0f, 5.0f);
    HelpMarker("ぶつかった地点のまわりで、前後と左右の周期をずらして揺れます。\n"
               "大きくすると狙いづらくなるので、ほんの少しで十分です（斜めには約1.4倍ずれます）");
    ImGui::DragFloat("ふらつきの速さ", &wall.wobbleSpeed, 0.05f, 0.0f, 20.0f);
    ImGui::DragFloat("ふらつきで傾く角度", &wall.wobbleTilt, 0.5f, 0.0f, 90.0f);
    ImGui::DragFloat("首を振る時間", &wall.shakeTime, 0.05f, 0.05f, 5.0f);
    HelpMarker("立ち直りの合図です。ここが終わるとチャンスも終わります");
    ImGui::DragFloat("首を振る角度", &wall.shakeAngle, 0.5f, 0.0f, 120.0f);
    ImGui::DragFloat("首を振る往復の回数", &wall.shakeCount, 0.1f, 0.5f, 8.0f);
    ImGui::DragFloat("元の姿勢へ戻る時間", &wall.settleTime, 0.05f, 0.05f, 6.0f);
    HelpMarker("止まっていた自転が通常の速さへ戻るまでの時間です");
    ImGui::DragFloat("ひるむのに必要な突進距離", &wall.minTravel, 0.1f, 0.0f, 60.0f);
    HelpMarker("これだけ進んでからぶつかった時だけひるみます。\n"
               "壁際で突進を始めたときに、いきなりひるむのを防ぎます");
    ImGui::TextDisabled("ひるみの合計: %.2f 秒", wall.wobbleTime + wall.shakeTime + wall.settleTime);

    ImGui::SeparatorText("色残量");
    for (Color color : palette_.GetUsedColors()) {
        const Vector4 rgba = palette_.GetRgba(color);
        ImGui::TextColored(ImVec4(rgba.x, rgba.y, rgba.z, rgba.w), "%-7s : %d",
                           BossColorPalette::GetIdText(color), cluster_.CountAlive(color));
    }

    ImGui::SeparatorText("殻の形");
    BossShellParams &shell = parameters_.Shell();

    // 全体の倍率は「値へ掛けたうえで、何倍ぶん掛けたかを覚える」作りなので、
    // 倍率が1以外のときにここを手で書き換えると、その値まで倍率ぶん割り戻されてしまう。
    // 気づかずに小さくなりすぎるのを防ぐため、そのときだけ注意を出す
    if (std::abs(parameters_.GetMasterScale() - 1.0f) > 0.001f) {
        ImGui::TextColored(ImVec4{1.0f, 0.8f, 0.3f, 1.0f},
                           "全体の倍率が %.2f 倍です。ここの値は 1.00 倍のときに触ってください",
                           parameters_.GetMasterScale());
        HelpMarker("倍率が掛かった状態で書き換えると、その数値が「倍率込みの値」として\n"
                   "扱われます。等倍に戻したときに小さくなりすぎたら、\n"
                   "「ボス全体の大きさ」の「いまの大きさを1倍にする」で基準を取り直せます");
    }
    bool radiusChanged = false;
    bool bandChanged = false;
    radiusChanged |= ImGui::DragFloat("球の半径(0で自動)", &shell.sphereRadius, 0.01f, 0.0f, 3.0f);
    // 実際に使われている半径を見せる。0以外を入れると自動算出（隣と接する大きさ）から
    // 外れてハニカムが崩れるため、ここで食い違いに気づけるようにしておく
    ImGui::SameLine();
    ImGui::TextDisabled("実際: %.3f", cluster_.GetSphereRadius());
    radiusChanged |= ImGui::DragFloat("コアの大きさ", &shell.coreScale, 0.01f, 0.1f, 1.2f);
    bandChanged |= ImGui::DragFloat("基本殻の半径", &shell.shellRadius, 0.05f, 0.1f, 30.0f);
    bandChanged |= ImGui::SliderInt("分割数(0=12/1=42/2=162)", &shell.subdivision, 0, 2);
    bandChanged |= ImGui::SliderInt("外側へ付着できる層数", &shell.outerLayers, 0, 5);
    bandChanged |= ImGui::SliderInt("内側へ付着できる層数", &shell.innerLayers, 0, 5);

    // 見た目の外周は球の中心より外へ膨らむので、外周半径ちょうどだと下の球が床へ潜る。
    // 大きさを変えるほど潜り方も増えるため、持ち上げ量を手で足せるようにしておく
    if (ImGui::DragFloat("接地高さの調整", &shell.groundOffset, 0.01f, -5.0f, 20.0f)) {
        ApplyCoreLayout();
    }
    HelpMarker("中心の高さは「外周半径 + この値」になります。\n"
               "融合メッシュのふくらみや、外側へ積み上がった弾のぶんだけ\n"
               "下の球が床に潜って見えるときに持ち上げてください。\n"
               "全体の大きさを変えると、この値も一緒に掛かります");
    // 基本殻の球は外周半径ちょうどで床に接するが、そこへ弾が外側の層まで積み上がると
    // そのぶん下へはみ出す。融合メッシュが球より太る設定なら、その差も足りない。
    // どちらも計算で出せるので、目分量で探さずに済むよう「必要な値」を出して入れられるようにする
    const float sphereRadius = cluster_.GetSphereRadius();
    const float stackDepth = sphereRadius * 2.0f * static_cast<float>(shell.outerLayers);
    const float metaBallBulge =
        sphereRadius * (std::max)(0.0f, parameters_.MetaBall().influenceScale * 0.5f - 1.0f);
    const float suggested = stackDepth + metaBallBulge;
    ImGui::TextDisabled("中心の高さ %.3f ／ 外周半径 %.3f ／ 弾が積もるぶん %.3f",
                        transform_->translation_.y, GetBodyRadius(), suggested);
    if (ImGui::Button("弾が積もるぶんまで持ち上げる")) {
        shell.groundOffset = suggested;
        ApplyCoreLayout();
    }
    HelpMarker("外側の層いっぱいまで弾が付いても床から出ない高さにします。\n"
               "弾が少ないうちは浮いて見えるので、間を取りたいときは\n"
               "上のスライダーで半分くらいに減らしてください");

    const bool rebuildPressed = ImGui::Button("殻を作り直す");
    if (bandChanged || rebuildPressed) {
        // 帯を変えると球の数が変わるので作り直す
        RebuildShell();
    } else if (radiusChanged) {
        // 球の大きさだけなら位置と見た目の更新で済む
        ApplyShellChanges();
    }
    ImGui::TextDisabled("基本殻はハニカム状に均等配置。弾はその外側・内側の層へ付着します");

    ImGui::SeparatorText("殻の見た目（メタボール）");
    BossMetaBallParams &metaBall = parameters_.MetaBall();
    bool metaBallChanged = false;
    metaBallChanged |= ImGui::DragFloat("影響半径の倍率", &metaBall.influenceScale, 0.01f, 0.05f, 4.0f);
    metaBallChanged |= ImGui::DragFloat("セルの細かさ", &metaBall.voxelRatio, 0.01f, 0.1f, 1.0f);
    metaBallChanged |= ImGui::DragFloat("しきい値", &metaBall.threshold, 0.01f, 0.05f, 2.0f);
    metaBallChanged |= ImGui::DragFloat("強調球の倍率", &metaBall.highlightScale, 0.01f, 1.0f, 2.0f);
    metaBallChanged |= ImGui::Checkbox("GPUで生成する", &metaBall.useGpu);
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("GPU: 毎フレーム作り直すので脈動が使えます\n"
                          "CPU: 球が増減した色だけ作り直します（動かせませんが確実に動きます）");
    }

    ImGui::BeginDisabled(!metaBall.useGpu);
    metaBallChanged |= ImGui::DragFloat("脈動の振幅", &metaBall.wobbleAmplitude, 0.005f, 0.0f, 0.5f);
    metaBallChanged |= ImGui::DragFloat("脈動の速さ", &metaBall.wobbleSpeed, 0.05f, 0.0f, 20.0f);
    metaBallChanged |= ImGui::DragFloat("脈動のばらけ", &metaBall.wobbleFrequency, 0.05f, 0.0f, 10.0f);
    ImGui::EndDisabled();

    if (metaBallChanged) {
        cluster_.SetMetaBallParams(metaBall);
    }

    const BossShellMetaBall &shellMetaBall = cluster_.GetMetaBall();
    if (shellMetaBall.IsGpuMode()) {
        const Hagine::MetaBallGpuStats &gpuStats = shellMetaBall.GetGpuStats();
        ImGui::TextDisabled("GPU生成: 格子 %u x %u x %u（セル %u 個 / 1辺 %.3f）",
                            gpuStats.gridX, gpuStats.gridY, gpuStats.gridZ,
                            gpuStats.cellCount, gpuStats.cellSize);
        ImGui::TextDisabled("1色あたり三角形 %d 個まで（あふれると欠けます）",
                            metaBall.maxTrianglesPerColor);
    } else {
        ImGui::TextDisabled("CPU生成: 三角形 %d 個 ／ 直近の作り直し %.2f ms",
                            shellMetaBall.GetTotalTriangleCount(),
                            shellMetaBall.GetLastBuildMilliseconds());
    }
    ImGui::TextDisabled("セルを細かくするほど滑らかになります（CPU生成では作り直しも重くなります）");

    ImGui::SeparatorText("デバッグ");
    ImGui::Checkbox("隣接グラフを描画", &drawGraphDebug_);
    ImGui::Checkbox("狙いを描画", &drawTargetDebug_);
    ImGui::TextDisabled("  青=ボスの位置 / 赤=ボスが思っている相手の位置 / 橙=その真下 / 緑の円=行動範囲");
    if (ImGui::Button("ボスをリセット")) {
        ResetBoss();
        ImGuiNotification::Post("ボスをリセットしました", {0.4f, 0.8f, 1.0f, 1.0f});
    }
    ImGui::SameLine();
    if (ImGui::Button("パラメータを保存")) {
        parameters_.Save();
        ImGuiNotification::Post("ボスデータを保存しました", {0.2f, 0.8f, 0.2f, 1.0f});
    }
#endif // USE_IMGUI
}

void Boss::DrawTargetDebug() {
    LineRenderer *lineRenderer = LineRenderer::GetInstance();

    // ボスが立っている場所（攻撃の計算で使っている位置そのもの）
    const Vector3 bossPosition = GetBossPosition();
    lineRenderer->AddSphere(bossPosition, 1.0f, Vector4{0.3f, 0.8f, 1.0f, 1.0f});

    if (!pTargetLocator_ || !pTargetLocator_->IsTargetValid()) {
        return;
    }

    // ボスが「相手はここにいる」と思っている場所。
    // 実際のプレイヤーとここがずれていれば、位置の受け取り方が原因になる
    const Vector3 targetPosition = pTargetLocator_->GetTargetPosition();
    lineRenderer->AddSphere(targetPosition, 1.0f, Vector4{1.0f, 0.3f, 0.3f, 1.0f});
    lineRenderer->AddSphere(Vector3{targetPosition.x, 0.0f, targetPosition.z}, 0.5f,
                            Vector4{1.0f, 0.6f, 0.2f, 1.0f});
    lineRenderer->AddLine(bossPosition, targetPosition, Vector4{1.0f, 0.8f, 0.3f, 1.0f});

    // 行動範囲（この円の外へは出られない）
    lineRenderer->AddCircle(Vector3{homePosition_.x, 0.05f, homePosition_.z},
                            Vector3{parameters_.Battle().arenaRadius, 0.0f, 0.0f},
                            Vector3{0.0f, 0.0f, parameters_.Battle().arenaRadius},
                            Vector4{0.4f, 1.0f, 0.4f, 1.0f}, 48);
}
