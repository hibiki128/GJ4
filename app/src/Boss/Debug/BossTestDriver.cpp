#include "BossTestDriver.h"
#include "Input.h"
#include "MyMath.h"
#include "camera/projection/ViewProjection.h"
#include "debug/imgui/ImGuiNotification.h"
#include "frame/Frame.h"
#include "line/LineRenderer.h"
#include <algorithm>
#include <string>
#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

using namespace Hagine;

namespace {
/// <summary>同時に存在できる弾の上限（プールの上限）</summary>
constexpr int kMaxProjectiles = 32;

/// <summary>色切り替えに使うキー（Color の並びと対応）</summary>
constexpr BYTE kColorKeys[kGameColorCount] = {DIK_1, DIK_2, DIK_3, DIK_4};

/// <summary>発射キー</summary>
constexpr BYTE kFireKey = DIK_F;
} // namespace

void BossTestDriver::Init(Boss *boss) {
    pBoss_ = boss;
    if (pBoss_ && !pBoss_->GetPalette().GetUsedColors().empty()) {
        // ボスが使っていない色を初期選択にしない
        selectedColor_ = pBoss_->GetPalette().GetUsedColors().front();
    }
}

void BossTestDriver::Update(const ViewProjection &viewProjection) {
    if (!pBoss_ || !isEnabled_) {
        return;
    }

    UpdateColorSelection();

    // カメラの位置・向きをそのまま射線として使う
    const Matrix4x4 rotateMatrix = MakeRotateXYZMatrix(viewProjection.eulerRotation_);
    const Vector3 aimDirection = TransformNormal(kWorldForward, rotateMatrix).Normalize();
    const Vector3 origin = viewProjection.translation_ + aimDirection * 1.0f;

    UpdateLockOn(origin, aimDirection);

    // --- 発射 ---
    const float deltaTime = Frame::DeltaTime();
    fireCoolDown_ = (std::max)(0.0f, fireCoolDown_ - deltaTime);
    if (Input::GetInstance()->PushKey(kFireKey) && fireCoolDown_ <= 0.0f) {
        FireProjectile(origin, aimDirection);
        fireCoolDown_ = fireInterval_;
    }

    // --- 弾の更新（衝突判定の走査外なので、ここで破棄しても安全）---
    for (std::unique_ptr<BossTestProjectile> &projectile : projectiles_) {
        projectile->Update();
    }

    if (drawAimLine_) {
        LineRenderer *lineRenderer = LineRenderer::GetInstance();
        if (lockOn_.IsValid()) {
            lineRenderer->AddLine(origin, lockOn_.worldPosition, {1.0f, 1.0f, 0.4f, 1.0f});
            lineRenderer->AddSphere(lockOn_.worldPosition, 0.7f, {1.0f, 1.0f, 0.4f, 1.0f}, 12);
        } else {
            const float length = pBoss_->GetParameters().LockOn().maxDistance;
            lineRenderer->AddLine(origin, origin + aimDirection * length, {0.4f, 0.4f, 0.45f, 1.0f});
        }
    }
}

void BossTestDriver::Draw(const ViewProjection &viewProjection) {
    for (std::unique_ptr<BossTestProjectile> &projectile : projectiles_) {
        if (!projectile->IsFinished()) {
            projectile->Draw(viewProjection);
        }
    }
}

void BossTestDriver::UpdateColorSelection() {
    Input *input = Input::GetInstance();
    for (int i = 0; i < kGameColorCount; ++i) {
        if (input->TriggerKey(kColorKeys[i])) {
            selectedColor_ = BossColorPalette::FromIndex(i);
        }
    }
}

void BossTestDriver::UpdateLockOn(const Vector3 &origin, const Vector3 &aimDirection) {
    const BossLockOnParams &lockOnParams = pBoss_->GetParameters().LockOn();

    LockOnRequest request{};
    request.origin = origin;
    request.aimDirection = aimDirection;
    request.color = selectedColor_;
    request.maxAngleDegrees = lockOnParams.maxAngleDegrees;
    request.maxDistance = lockOnParams.maxDistance;

    if (!pBoss_->FindLockOnTarget(request, lockOn_)) {
        lockOn_ = LockOnResult{};
    }
    // ロックオン中の球だけを強調表示する（死角対策の透過表示もここに足せる）
    pBoss_->SetLockOnHighlight(lockOn_.cell, lockOn_.found);
}

void BossTestDriver::FireProjectile(const Vector3 &origin, const Vector3 &aimDirection) {
    BossTestProjectile *projectile = AcquireProjectile();
    if (!projectile) {
        return;
    }

    // ロックオンできていれば対象へ、していなければ照準方向へ撃つ
    const Vector3 direction = lockOn_.IsValid()
                                  ? (lockOn_.worldPosition - origin).Normalize()
                                  : aimDirection;

    projectile->SetColor(pBoss_->GetPalette().GetRgba(selectedColor_));
    projectile->Fire(origin, direction, projectileSpeed_, projectileLife_, correctionRate_);

    // 飛翔中も対象を追い続ける（＝自動軌道補正）
    if (lockOn_.IsValid()) {
        Boss *boss = pBoss_;
        const ShellCell targetCell = lockOn_.cell;
        projectile->SetTargetPositionGetter([boss, targetCell](Vector3 &out) {
            return boss->TryGetTargetPosition(targetCell, out);
        });
    } else {
        projectile->SetTargetPositionGetter(nullptr);
    }

    // 着弾は本番のプレイヤー弾と同じ入口（IBossTargetQuery::RaycastAttach）を通す
    const Color shotColor = selectedColor_;
    projectile->SetHitTester([this, shotColor](const Vector3 &from, const Vector3 &to) {
        const BulletHitResult result = pBoss_->RaycastAttach(from, to, shotColor);
        if (!result.hit) {
            return false; // 穴を素通りした。弾はそのまま飛ぶ
        }

        lastHit_ = result;
        if (result.destroyed) {
            ImGuiNotification::Post("同色 " + std::to_string(result.clusterSize) +
                                        " 個 消去！",
                                    {1.0f, 0.8f, 0.3f, 1.0f});
        }
        return result.ShouldConsumeBullet();
    });
}

BossTestProjectile *BossTestDriver::AcquireProjectile() {
    // 役目を終えた弾を再利用する（毎回生成しないことで名前とJSON探索の無駄を避ける）
    for (std::unique_ptr<BossTestProjectile> &projectile : projectiles_) {
        if (projectile->IsFinished()) {
            return projectile.get();
        }
    }

    if (static_cast<int>(projectiles_.size()) >= kMaxProjectiles) {
        return nullptr;
    }

    auto projectile = std::make_unique<BossTestProjectile>();
    projectile->InitProjectile("BossTestBullet" + std::to_string(projectileSerial_++),
                               pBoss_->GetPalette().GetRgba(selectedColor_), projectileRadius_);
    BossTestProjectile *raw = projectile.get();
    projectiles_.push_back(std::move(projectile));
    return raw;
}

void BossTestDriver::DrawImGui() {
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader("ボス検証（デバッグ射撃）", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    if (!pBoss_) {
        ImGui::TextDisabled("ボスが設定されていません");
        return;
    }

    ImGui::Checkbox("有効", &isEnabled_);
    ImGui::SameLine();
    ImGui::TextDisabled("[1][2][3][4] 色切替 / [F] 発射");

    ImGui::SeparatorText("色");
    for (Color color : pBoss_->GetPalette().GetUsedColors()) {
        const Vector4 rgba = pBoss_->GetPalette().GetRgba(color);
        const bool selected = (color == selectedColor_);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(rgba.x, rgba.y, rgba.z, rgba.w));
        if (ImGui::RadioButton(BossColorPalette::GetIdText(color), selected)) {
            selectedColor_ = color;
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
    }
    ImGui::NewLine();

    ImGui::SeparatorText("ロックオン");
    if (lockOn_.IsValid()) {
        ImGui::Text("対象セル: 頂点%d 層%d  角度 %.1f度  距離 %.1f",
                    lockOn_.cell.vertex, lockOn_.cell.layer,
                    lockOn_.angleDegrees, lockOn_.distance);
    } else {
        ImGui::TextDisabled("対象なし（照準内に同色の球がありません）");
    }
    ImGui::Checkbox("照準線を表示", &drawAimLine_);

    ImGui::SeparatorText("直近の着弾");
    if (!lastHit_.hit) {
        ImGui::TextDisabled("未着弾（穴を素通りした）");
    } else if (!lastHit_.attached) {
        ImGui::Text("当たったが置ける隣が無く、弾は消えた");
    } else if (!lastHit_.destroyed) {
        ImGui::Text("付着した（同色 %d 個 … 消去には %d 個必要）",
                    lastHit_.clusterSize, pBoss_->GetParameters().Chain().minMatch);
    } else {
        ImGui::Text("同色 %d 個 消去！ 怯み %.2f秒",
                    lastHit_.clusterSize, lastHit_.staggerTime);
    }

    ImGui::SeparatorText("弾のパラメータ");
    ImGui::DragFloat("弾速", &projectileSpeed_, 0.5f, 1.0f, 200.0f);
    ImGui::DragFloat("軌道補正の強さ", &correctionRate_, 0.1f, 0.0f, 60.0f);
    ImGui::DragFloat("発射間隔", &fireInterval_, 0.01f, 0.02f, 2.0f);
#endif // USE_IMGUI
}
