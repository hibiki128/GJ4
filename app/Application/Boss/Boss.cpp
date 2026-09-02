#include "Boss.h"
#include "Application/Boss/Attack/BossAttackSlam.h"
#include "Application/Boss/Attack/BossAttackSpin.h"
#include "Application/Boss/State/BossStates.h"
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

Boss::~Boss() {
    // 登録したポインタが宙に浮かないよう、破棄前に必ず解除する
    if (!paramOwnerLabel_.empty()) {
        GameParamHub::GetInstance()->Unregister(paramOwnerLabel_);
    }
}

void Boss::Init(const std::string objectName) {
    RegisterGameTags();

    // --- データ読み込み（色マスタ → ボス個別データ → 使用色サブセット）---
    palette_.LoadMaster();
    parameters_.Load(bossId_);
    palette_.SetUsedColors(parameters_.GetUsedColors());

    // --- コア（内側の球）を生成 ---
    BaseObject::Init(objectName);
    CreatePrimitiveModel(PrimitiveType::Sphere);

    // パーツを組む前に登録する。保存済みの実行時調整値があればここで反映され、
    // その値でパーツが作られる
    RegisterTuningParameters();

    const BossLayoutParams &layout = parameters_.Layout();

    // シーンに保存済みの配置があればそれを尊重し、無いときだけ既定値を入れる
    if (!objectData_ || !objectData_->Contains("translation")) {
        transform_->translation_ = Vector3{0.0f, layout.radius, 0.0f};
    }
    if (!objectData_ || !objectData_->Contains("scale")) {
        const float coreSize = layout.radius * layout.coreScale;
        transform_->scale_ = Vector3{coreSize, coreSize, coreSize};
    }
    transform_->UpdateMatrix();
    SetTexture(kBossTexturePath);
    SetColor(Vector4{0.16f, 0.16f, 0.20f, 1.0f});

    // --- パーツ群を生成して自分にぶら下げる ---
    graph_.Build(this, objectName + "Part", layout, palette_, parameters_.Chain(),
                 parameters_.GetColorSeed(), kBossPartTag, kPlayerBulletTag);
    builtSubdivision_ = layout.subdivision;

    hp_ = parameters_.GetMaxHp();
    staggerTimer_ = 0.0f;
    homePosition_ = transform_->translation_;

    SetupStatesAndAttacks();

    Logger::Info("Boss: " + bossId_ + " を生成しました（パーツ " +
                 std::to_string(graph_.GetTotalCount()) + "個 / 使用色 " +
                 std::to_string(palette_.GetUsedColors().size()) + "色）");
}

void Boss::Update() {
    BaseObject::Update();

    const float deltaTime = Frame::DeltaTime();

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

    // 削れるほど攻撃が早く・激しくなる
    UpdateExposureScaling();

    stateMachine_.Update(*this, deltaTime);
    ClampToArena();

    if (drawGraphDebug_) {
        // 線はフレーム単位で積み上げるので、描画フェーズではなく更新中に積む
        graph_.DebugDrawGraph();
    }
}

void Boss::Draw(const ViewProjection &viewProjection) {
    // コア（自分自身）→ パーツの順に描く。
    // 呼び出し元（BaseObjectManager::Draw）のインスタンシング収集範囲内なので、
    // 同じプリミティブを使うパーツはまとめて1ドローになる
    BaseObject::Draw(viewProjection);
    graph_.Draw(viewProjection);

    // 攻撃中だけ出る表示物（落下攻撃の着弾予告など）
    if (pCurrentAttack_) {
        pCurrentAttack_->Draw(viewProjection);
    }
}

/// ===================================================
/// 状態・攻撃
/// ===================================================

void Boss::SetupStatesAndAttacks() {
    stateMachine_.Register(std::make_unique<BossStateIdle>());
    stateMachine_.Register(std::make_unique<BossStateAttack>());
    stateMachine_.Register(std::make_unique<BossStateStagger>());
    stateMachine_.Register(std::make_unique<BossStateDead>());

    // 攻撃はパラメータを参照で受け取るので、実行時に値を変えると即反映される
    scheduler_.AddAttack(std::make_unique<BossAttackSpin>(&parameters_.Spin(), &parameters_.Exposure()));
    scheduler_.AddAttack(std::make_unique<BossAttackSlam>(&parameters_.Slam(), &parameters_.Exposure()));
    UpdateExposureScaling();
    scheduler_.Reset();

    stateMachine_.Start(*this, BossStateId::Idle);
}

void Boss::AddIdleSpin(float deltaTime) {
    AddSpin(parameters_.Battle().idleSpinSpeed * deltaTime);
}

bool Boss::TickAttackCoolDown(float deltaTime) {
    return scheduler_.TickCoolDown(deltaTime);
}

void Boss::StartScheduledAttack() {
    pCurrentAttack_ = scheduler_.PickNext();
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
        if (!pCurrentAttack_->IsFinished()) {
            pCurrentAttack_->Cancel(MakeAttackContext(0.0f));
        }
        pCurrentAttack_ = nullptr;
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
    const int total = graph_.GetTotalCount();
    if (total <= 0) {
        return 0.0f;
    }
    const int destroyed = total - graph_.GetAliveCount();

    // 色は変化しないので、最後まで壊せないパーツが必ず残る。
    // 素の割合のままだと露出度が 1.0 に届かず、後半の激しさが出ないため、
    // 既定では「壊せるパーツの総数」を分母にして正規化する
    const int denominator = parameters_.Exposure().normalizeByDestroyable
                                ? graph_.GetDestroyablePartCount()
                                : total;
    if (denominator <= 0) {
        return 0.0f;
    }
    return std::clamp(static_cast<float>(destroyed) / static_cast<float>(denominator), 0.0f, 1.0f);
}

void Boss::UpdateExposureScaling() {
    const BossExposureParams &exposure = parameters_.Exposure();
    // 攻撃頻度は露出度から毎フレーム算出する（次のクールダウンから反映される）
    scheduler_.SetInterval(Lerp(exposure.attackIntervalAtZero, exposure.attackIntervalAtFull,
                                GetNormalizedExposure()));
}

void Boss::UpdateStaggerShake(float deltaTime) {
    staggerShakeTime_ += deltaTime;
    // 描画オフセットだけを揺らす。当たり判定の位置は動かさない
    SetOffset(Vector3{std::sin(staggerShakeTime_ * 46.0f) * 0.25f, 0.0f,
                      std::cos(staggerShakeTime_ * 37.0f) * 0.18f});
}

void Boss::ClearStaggerShake() {
    staggerShakeTime_ = 0.0f;
    SetOffset(Vector3{0.0f, 0.0f, 0.0f});
}

void Boss::AddSpin(float degrees) {
    spinAngle_ += degrees * (std::numbers::pi_v<float> / 180.0f);

    // 真上を軸にすると極のパーツが永久に見えないので、軸を少し傾けて回す
    const Vector3 axis = Vector3{0.25f, 1.0f, 0.15f}.Normalize();
    transform_->quaternionRotation_ = Quaternion::FromAxisAngle(axis, spinAngle_);
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
    if (IsDead()) {
        return;
    }

    hp_ = (std::max)(0.0f, hp_ - info.amount);
    // 連続ヒットで怯みが短くならないよう、長い方を採用する
    staggerTimer_ = (std::max)(staggerTimer_, info.staggerTime);

    if (IsDead()) {
        ImGuiNotification::Post("ボス撃破", {1.0f, 0.85f, 0.3f, 1.0f});
    }
}

bool Boss::FindLockOnTarget(const LockOnRequest &request, LockOnResult &out) {
    return graph_.FindLockOnTarget(request, parameters_.LockOn().requireFacing, out);
}

ChainHitResult Boss::ReportHit(int partIndex, Color shotColor) {
    ChainHitResult result{};

    BossPart *part = graph_.GetPart(partIndex);
    if (!part || !part->IsPartAlive()) {
        return result;
    }
    if (part->GetPartColor() != shotColor) {
        return result; // 色が違うパーツは弾かれる（ダメージなし）
    }

    result.accepted = true;

    const std::vector<int> cluster = graph_.CollectSameColorCluster(partIndex);
    result.chainSize = static_cast<int>(cluster.size());

    const BossChainParams &chain = parameters_.Chain();
    if (result.chainSize < chain.minMatch) {
        return result; // 連鎖不成立。色を切り替えて連結を育てる
    }

    const Vector3 hitPoint = part->GetWorldPosition();
    const int broken = graph_.BreakParts(cluster);
    if (broken <= 0) {
        return result;
    }

    // 連鎖規模が大きいほどダメージ・怯みが伸びる
    const int overMatch = broken - chain.minMatch;
    result.destroyed = true;
    result.damage = chain.damagePerPart * static_cast<float>(broken) *
                    (1.0f + chain.chainBonus * static_cast<float>(overMatch));
    result.staggerTime = chain.staggerBase + chain.staggerPerPart * static_cast<float>(overMatch);

    DamageInfo info{};
    info.amount = result.damage;
    info.hitPoint = hitPoint;
    info.chainSize = broken;
    info.staggerTime = result.staggerTime;
    ApplyDamage(info);

    return result;
}

ChainHitResult Boss::ReportHitByCollider(const ColliderBase *hitCollider, Color shotColor) {
    return ReportHit(FindPartIndex(hitCollider), shotColor);
}

int Boss::FindPartIndex(const ColliderBase *collider) const {
    return graph_.FindPartIndex(collider);
}

void Boss::ResetBoss() {
    graph_.ResetAll(palette_, parameters_.Chain(), parameters_.GetColorSeed());
    hp_ = parameters_.GetMaxHp();
    staggerTimer_ = 0.0f;

    pCurrentAttack_ = nullptr;
    UpdateExposureScaling();
    scheduler_.Reset();
    ClearStaggerShake();
    SetBossPosition(homePosition_);
    stateMachine_.Start(*this, BossStateId::Idle);
}

void Boss::ApplyLayoutChanges() {
    const BossLayoutParams &layout = parameters_.Layout();

    // 分割数が変わったときだけ作り直しが要る（パーツの数そのものが変わるため）
    if (layout.subdivision != builtSubdivision_) {
        RebuildParts();
        return;
    }

    ApplyCoreLayout();
    graph_.ApplyLayout(layout);
}

void Boss::RebuildParts() {
    const BossLayoutParams &layout = parameters_.Layout();

    ApplyCoreLayout();
    graph_.Build(this, objectName_ + "Part", layout, palette_, parameters_.Chain(),
                 parameters_.GetColorSeed(), kBossPartTag, kPlayerBulletTag);
    builtSubdivision_ = layout.subdivision;

    UpdateExposureScaling();
}

void Boss::ApplyCoreLayout() {
    const BossLayoutParams &layout = parameters_.Layout();

    // コアの大きさと接地高さを半径へ追従させる
    const float coreSize = layout.radius * layout.coreScale;
    transform_->scale_ = Vector3{coreSize, coreSize, coreSize};
    homePosition_.y = layout.radius;

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
    paramOwnerLabel_ = "Boss/" + bossId_;

    GameParamHub *hub = GameParamHub::GetInstance();
    BossChainParams &chain = parameters_.Chain();
    BossLockOnParams &lockOn = parameters_.LockOn();
    BossLayoutParams &layout = parameters_.Layout();

    // --- 見た目（変更したらパーツを作り直す）---
    GameParamHub::Options rebuildOptions{};
    rebuildOptions.speed = 0.05f;
    rebuildOptions.min = 0.1f;
    rebuildOptions.max = 30.0f;
    rebuildOptions.onChange = [this] { ApplyLayoutChanges(); };

    hub->Register(paramOwnerLabel_, "見た目:全体の半径", &layout.radius, rebuildOptions);

    rebuildOptions.min = 0.0f;
    rebuildOptions.max = 10.0f;
    hub->Register(paramOwnerLabel_, "見た目:パーツの大きさ(0で自動)", &layout.partScale, rebuildOptions);

    rebuildOptions.speed = 0.01f;
    rebuildOptions.min = 0.05f;
    rebuildOptions.max = 1.5f;
    hub->Register(paramOwnerLabel_, "見た目:パーツの厚み", &layout.partThickness, rebuildOptions);

    rebuildOptions.min = 0.1f;
    rebuildOptions.max = 1.2f;
    hub->Register(paramOwnerLabel_, "見た目:コアの大きさ", &layout.coreScale, rebuildOptions);

    GameParamHub::Options subdivisionOptions{};
    subdivisionOptions.speed = 1.0f;
    subdivisionOptions.min = 0.0f;
    subdivisionOptions.max = 2.0f;
    subdivisionOptions.onChange = [this] { ApplyLayoutChanges(); };
    hub->Register(paramOwnerLabel_, "見た目:分割数(0=12/1=42/2=162)", &layout.subdivision, subdivisionOptions);

    hub->Register(paramOwnerLabel_, "連鎖:最低連結数", &chain.minMatch, {1.0f, 2.0f, 8.0f});
    hub->Register(paramOwnerLabel_, "連鎖:パーツ単価ダメージ", &chain.damagePerPart, {0.5f, 0.0f, 500.0f});
    hub->Register(paramOwnerLabel_, "連鎖:規模ボーナス", &chain.chainBonus, {0.01f, 0.0f, 3.0f});
    hub->Register(paramOwnerLabel_, "連鎖:基礎怯み時間", &chain.staggerBase, {0.01f, 0.0f, 5.0f});
    hub->Register(paramOwnerLabel_, "連鎖:1つあたり怯み加算", &chain.staggerPerPart, {0.01f, 0.0f, 2.0f});
    hub->Register(paramOwnerLabel_, "連鎖:初期塊の上限", &chain.maxInitialCluster, {1.0f, 0.0f, 60.0f});
    hub->Register(paramOwnerLabel_, "ロックオン:許容角度", &lockOn.maxAngleDegrees, {0.5f, 0.0f, 90.0f});
    hub->Register(paramOwnerLabel_, "ロックオン:有効距離", &lockOn.maxDistance, {0.5f, 0.0f, 300.0f});
    hub->Register(paramOwnerLabel_, "ロックオン:表面のみ狙う", &lockOn.requireFacing);

    // --- 戦闘・攻撃 ---
    BossBattleParams &battle = parameters_.Battle();
    BossSpinAttackParams &spin = parameters_.Spin();
    BossSlamAttackParams &slam = parameters_.Slam();

    hub->Register(paramOwnerLabel_, "戦闘:行動範囲", &battle.arenaRadius, {0.5f, 1.0f, 200.0f});
    hub->Register(paramOwnerLabel_, "戦闘:待機時の自転速度", &battle.idleSpinSpeed, {0.5f, 0.0f, 360.0f});

    hub->Register(paramOwnerLabel_, "突進:予兆時間", &spin.telegraphTime, {0.01f, 0.05f, 5.0f});
    hub->Register(paramOwnerLabel_, "突進:予兆の自転速度", &spin.telegraphSpinSpeed, {5.0f, 0.0f, 2000.0f});
    hub->Register(paramOwnerLabel_, "突進:速度", &spin.dashSpeed, {0.2f, 0.0f, 100.0f});
    hub->Register(paramOwnerLabel_, "突進:時間", &spin.dashTime, {0.01f, 0.05f, 5.0f});
    hub->Register(paramOwnerLabel_, "突進:硬直", &spin.recoverTime, {0.01f, 0.0f, 5.0f});
    hub->Register(paramOwnerLabel_, "突進:ダメージ", &spin.damage, {0.5f, 0.0f, 200.0f});
    hub->Register(paramOwnerLabel_, "突進:当たりの甘さ", &spin.contactMargin, {0.05f, 0.0f, 10.0f});

    hub->Register(paramOwnerLabel_, "落下:飛び上がり時間", &slam.riseTime, {0.01f, 0.05f, 5.0f});
    hub->Register(paramOwnerLabel_, "落下:高さ", &slam.riseHeight, {0.2f, 1.0f, 80.0f});
    hub->Register(paramOwnerLabel_, "落下:狙いの時間", &slam.aimTime, {0.01f, 0.0f, 5.0f});
    hub->Register(paramOwnerLabel_, "落下:落下時間", &slam.fallTime, {0.01f, 0.05f, 5.0f});
    hub->Register(paramOwnerLabel_, "落下:着弾後の静止", &slam.impactTime, {0.01f, 0.0f, 5.0f});
    hub->Register(paramOwnerLabel_, "落下:有効半径", &slam.impactRadius, {0.1f, 0.5f, 40.0f});
    hub->Register(paramOwnerLabel_, "落下:ダメージ", &slam.damage, {0.5f, 0.0f, 200.0f});
    hub->Register(paramOwnerLabel_, "落下:硬直", &slam.recoverTime, {0.01f, 0.0f, 5.0f});

    // --- 露出度スケーリング（難易度カーブ）---
    BossExposureParams &exposure = parameters_.Exposure();
    hub->Register(paramOwnerLabel_, "露出度:壊せる分で正規化", &exposure.normalizeByDestroyable);
    hub->Register(paramOwnerLabel_, "露出度:攻撃間隔(露出0)", &exposure.attackIntervalAtZero, {0.05f, 0.2f, 30.0f});
    hub->Register(paramOwnerLabel_, "露出度:攻撃間隔(露出1)", &exposure.attackIntervalAtFull, {0.05f, 0.2f, 30.0f});
    hub->Register(paramOwnerLabel_, "露出度:突進速度の倍率(露出1)", &exposure.spinDashSpeedScaleAtFull, {0.01f, 0.1f, 5.0f});
    hub->Register(paramOwnerLabel_, "露出度:突進予兆の倍率(露出1)", &exposure.spinTelegraphScaleAtFull, {0.01f, 0.1f, 2.0f});
    hub->Register(paramOwnerLabel_, "露出度:落下回数(露出0)", &exposure.slamCountAtZero, {1.0f, 1.0f, 12.0f});
    hub->Register(paramOwnerLabel_, "露出度:落下回数(露出1)", &exposure.slamCountAtFull, {1.0f, 1.0f, 12.0f});
    hub->Register(paramOwnerLabel_, "露出度:狙い時間の倍率(露出1)", &exposure.slamAimScaleAtFull, {0.01f, 0.1f, 2.0f});
}

void Boss::DrawImGui() {
    BaseObject::DrawImGui();

#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader("ボス", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    const float maxHp = (std::max)(1.0f, parameters_.GetMaxHp());
    ImGui::Text("HP: %.0f / %.0f", hp_, maxHp);
    ImGui::ProgressBar(hp_ / maxHp, ImVec2(-1.0f, 0.0f));

    ImGui::Text("露出度: %.1f%%  (残り %d / %d)", graph_.GetExposure() * 100.0f,
                graph_.GetAliveCount(), graph_.GetTotalCount());
    ImGui::Text("スケーリング用の露出度: %.1f%%（壊せる %d 枚を上限として正規化）",
                GetNormalizedExposure() * 100.0f, graph_.GetDestroyablePartCount());
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
    ImGui::TextDisabled("攻撃の当て先: %s", pTargetDamageSink_ ? "接続済み" : "未接続（通知のみ）");

    // パーツの色は変化しないので、この数が0になると連鎖では削れなくなる
    int poppableParts = 0;
    const int poppableClusters = graph_.CountPoppableClusters(parameters_.Chain().minMatch, &poppableParts);
    ImGui::Text("破壊可能な塊: %d 個（パーツ %d 枚ぶん）", poppableClusters, poppableParts);
    if (poppableClusters == 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "連鎖で削れる塊が残っていません");
    }

    ImGui::SeparatorText("色残量");
    for (Color color : palette_.GetUsedColors()) {
        const Vector4 rgba = palette_.GetRgba(color);
        ImGui::TextColored(ImVec4(rgba.x, rgba.y, rgba.z, rgba.w), "%-7s : %d",
                           BossColorPalette::GetIdText(color), graph_.CountAlive(color));
    }

    ImGui::SeparatorText("見た目");
    BossLayoutParams &layout = parameters_.Layout();
    bool layoutChanged = false;
    layoutChanged |= ImGui::DragFloat("全体の半径", &layout.radius, 0.05f, 0.1f, 30.0f);
    layoutChanged |= ImGui::DragFloat("パーツの厚み", &layout.partThickness, 0.01f, 0.05f, 1.5f);
    layoutChanged |= ImGui::DragFloat("パーツの大きさ(0で自動)", &layout.partScale, 0.02f, 0.0f, 10.0f);
    layoutChanged |= ImGui::DragFloat("コアの大きさ", &layout.coreScale, 0.01f, 0.1f, 1.2f);
    layoutChanged |= ImGui::SliderInt("分割数(0=12/1=42/2=162)", &layout.subdivision, 0, 2);

    const bool rebuildPressed = ImGui::Button("パーツを作り直す");
    if (layoutChanged) {
        ApplyLayoutChanges(); // 半径・厚みはトランスフォームの更新だけで済む
    } else if (rebuildPressed) {
        RebuildParts();
    }
    ImGui::TextDisabled("厚み1.0で真球、小さいほど平たい板になります");

    ImGui::SeparatorText("デバッグ");
    ImGui::Checkbox("隣接グラフを描画", &drawGraphDebug_);
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
