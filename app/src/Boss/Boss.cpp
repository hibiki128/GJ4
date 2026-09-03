#include "Boss.h"
#include "src/Boss/Attack/BossAttackSlam.h"
#include "src/Boss/Attack/BossAttackSpin.h"
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

    const BossShellParams &shell = parameters_.Shell();

    // シーンに保存済みの配置があればそれを尊重し、無いときだけ既定値を入れる
    if (!objectData_ || !objectData_->Contains("translation")) {
        transform_->translation_ = Vector3{0.0f, GetBodyRadius(), 0.0f};
    }
    if (!objectData_ || !objectData_->Contains("scale")) {
        const float coreSize = (shell.shellRadius - cluster_.GetSphereRadius()) * shell.coreScale;
        transform_->scale_ = Vector3{coreSize, coreSize, coreSize};
    }
    transform_->UpdateMatrix();
    SetTexture(kBossTexturePath);
    SetColor(Vector4{0.16f, 0.16f, 0.20f, 1.0f});

    // --- 殻の球を生成して自分にぶら下げる ---
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
        cluster_.DebugDraw();
    }
}

void Boss::Draw(const ViewProjection &viewProjection) {
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
    // このボスはHPを削って倒すのではなく、殻の球をすべて破壊するのが撃破条件。
    // 球を減らすのは同色消去そのものなので、ここでは怯みだけを受け取る
    if (IsDead()) {
        return;
    }

    // 連続ヒットで怯みが短くならないよう、長い方を採用する
    staggerTimer_ = (std::max)(staggerTimer_, info.staggerTime);
}

bool Boss::FindLockOnTarget(const LockOnRequest &request, LockOnResult &out) {
    return cluster_.FindLockOnTarget(request, parameters_.LockOn().requireFacing, out);
}

BulletHitResult Boss::RaycastAttach(const Vector3 &worldStart, const Vector3 &worldEnd, Color color) {
    const bool wasAlive = !IsDead();

    const BulletHitResult result =
        cluster_.RaycastAttach(worldStart, worldEnd, color, parameters_.Chain(), palette_);

    // 消去が起きたぶんだけ怯みが入る（付着しただけなら何も起きない）
    if (result.destroyed) {
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
    stateMachine_.Start(*this, BossStateId::Idle);
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

void Boss::ApplyCoreLayout() {
    const BossShellParams &shell = parameters_.Shell();

    // コアの大きさと接地高さを殻へ追従させる
    const float coreSize = (shell.shellRadius - cluster_.GetSphereRadius()) * shell.coreScale;
    transform_->scale_ = Vector3{coreSize, coreSize, coreSize};
    homePosition_.y = GetBodyRadius();

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
    BossShellParams &shell = parameters_.Shell();

    // --- 殻の形（球の大きさは即反映。帯を変えたら作り直しが要る）---
    GameParamHub::Options radiusOptions{};
    radiusOptions.speed = 0.01f;
    radiusOptions.min = 0.05f;
    radiusOptions.max = 3.0f;
    radiusOptions.onChange = [this] { ApplyShellChanges(); };
    hub->Register(paramOwnerLabel_, "殻:球の半径", &shell.sphereRadius, radiusOptions);

    GameParamHub::Options bandOptions{};
    bandOptions.speed = 0.05f;
    bandOptions.min = 0.1f;
    bandOptions.max = 30.0f;
    // 帯を動かすと球の数が変わるので、作り直す
    bandOptions.onChange = [this] { RebuildShell(); };
    hub->Register(paramOwnerLabel_, "殻:基本殻の半径", &shell.shellRadius, bandOptions);

    GameParamHub::Options layerOptions{};
    layerOptions.speed = 1.0f;
    layerOptions.min = 0.0f;
    layerOptions.max = 5.0f;
    layerOptions.onChange = [this] { RebuildShell(); };
    hub->Register(paramOwnerLabel_, "殻:分割数(0=12/1=42/2=162)", &shell.subdivision, layerOptions);
    hub->Register(paramOwnerLabel_, "殻:外側へ付着できる層数", &shell.outerLayers, layerOptions);
    hub->Register(paramOwnerLabel_, "殻:内側へ付着できる層数", &shell.innerLayers, layerOptions);

    GameParamHub::Options coreOptions{};
    coreOptions.speed = 0.01f;
    coreOptions.min = 0.1f;
    coreOptions.max = 1.2f;
    coreOptions.onChange = [this] { ApplyShellChanges(); };
    hub->Register(paramOwnerLabel_, "殻:コアの大きさ", &shell.coreScale, coreOptions);

    hub->Register(paramOwnerLabel_, "連鎖:最低連結数", &chain.minMatch, {1.0f, 2.0f, 8.0f});
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

    // 撃破条件は「中心のコアを除く色付きの球をすべて破壊すること」。
    // 残りの球数がそのまま撃破までの進捗になる
    const float initialCount = (std::max)(1.0f, GetMaxHp());
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
    ImGui::TextDisabled("攻撃の当て先: %s", pTargetDamageSink_ ? "接続済み" : "未接続（通知のみ）");

    ImGui::SeparatorText("色残量");
    for (Color color : palette_.GetUsedColors()) {
        const Vector4 rgba = palette_.GetRgba(color);
        ImGui::TextColored(ImVec4(rgba.x, rgba.y, rgba.z, rgba.w), "%-7s : %d",
                           BossColorPalette::GetIdText(color), cluster_.CountAlive(color));
    }

    ImGui::SeparatorText("殻の形");
    BossShellParams &shell = parameters_.Shell();
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

    const bool rebuildPressed = ImGui::Button("殻を作り直す");
    if (bandChanged || rebuildPressed) {
        // 帯を変えると球の数が変わるので作り直す
        RebuildShell();
    } else if (radiusChanged) {
        // 球の大きさだけなら位置と見た目の更新で済む
        ApplyShellChanges();
    }
    ImGui::TextDisabled("基本殻はハニカム状に均等配置。弾はその外側・内側の層へ付着します");

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
