#include "PlayerShootComponent.h"
#include "Frame/Frame.h"
#include "Utility/Debug/Param/GameParamHub.h"
#include "debug/imgui/ImGuiNotification.h"
#include "line/LineRenderer.h"
#include "src/Character/Player/Weapon/Bullet/Manager/PlayerBulletManager.h"
#include <string>
#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

void PlayerShootComponent::Update(PlayerContext& context) {
    // 色の切り替えは撃っていなくても効かせたいので、発射より先に見る
    UpdateColorSelection(context);

    cooldown_ -= Hagine::Frame::DeltaTime();

    IBossTargetQuery* target = ActiveTarget();

    // 着弾地点は撃つ前から毎フレーム求めておく。
    // 発射の瞬間に計算すると、その1発だけ照準表示と食い違う可能性がある
    aimPoint_ = ResolveAimPoint(target, context.aimOrigin_, context.aimDirection_);

    // ロックオンは撃つ前から効かせる（狙っている的が見えていないと色を選べない）
    if (target) {
        UpdateLockOn(target, context.aimOrigin_, context.aimDirection_);
    } else {
        lockOn_ = LockOnResult{};
    }

    if (drawAimLine_) {
        DrawAimLine(context);
    }

    if (!context.input_.attack) {
        return;
    }

    if (cooldown_ > 0.0f) {
        return;
    }

    if (!weapon_ || !context.bullets) {
        return;
    }

    FireBullet(context, target);

    cooldown_ = weapon_->GetFireInterval();
}

void PlayerShootComponent::UpdateColorSelection(const PlayerContext& context) {
    // 押した瞬間だけ添字が入る（-1 は変更なし）
    if (context.input_.selectColorIndex < 0) {
        return;
    }

    selectedColor_ = FromColorIndex(context.input_.selectColorIndex);
}

IBossTargetQuery* PlayerShootComponent::ActiveTarget() const {
    return targetProvider_ ? targetProvider_() : nullptr;
}

Hagine::Vector3 PlayerShootComponent::ResolveAimPoint(IBossTargetQuery* target,
                                                      const Hagine::Vector3& origin,
                                                      const Hagine::Vector3& direction) {
    const Hagine::Vector3 aim = (direction.LengthSq() > 0.0001f)
                                    ? direction.Normalize()
                                    : Hagine::Vector3{0.0f, 0.0f, 1.0f};
    const Hagine::Vector3 farPoint = origin + aim * aimRayLength_;

    aimPointHit_ = false;
    if (!target) {
        return farPoint;
    }

    // 着弾判定（RaycastAttach）と同じ形状・同じ色の扱いを通るので、
    // 「照準では当たる表示なのに弾は素通りする」というズレが出ない
    Hagine::Vector3 hitPoint{};
    if (!target->RaycastPoint(origin, farPoint, selectedColor_, hitPoint)) {
        return farPoint; // 何にも当たらない方向。射程の端を狙って真っ直ぐ飛ばす
    }

    aimPointHit_ = true;
    return hitPoint;
}

void PlayerShootComponent::UpdateLockOn(IBossTargetQuery* target, const Hagine::Vector3& origin,
                                        const Hagine::Vector3& aimDirection) {
    const LockOnRange range = target->GetLockOnRange();

    LockOnRequest request{};
    request.origin = origin;
    request.aimDirection = aimDirection;
    request.color = selectedColor_;
    request.maxAngleDegrees = range.maxAngleDegrees;
    request.maxDistance = range.maxDistance;

    if (!target->FindLockOnTarget(request, lockOn_)) {
        lockOn_ = LockOnResult{};
    }
    // 強調表示を持たない形態（蜘蛛）では何も起きない
    target->SetLockOnHighlight(lockOn_.cell, lockOn_.found);
}

void PlayerShootComponent::FireBullet(PlayerContext& context, IBossTargetQuery* target) {
    // 狙いは画面中心が指している着弾地点、弾が出るのはプレイヤーの位置。
    // 初速の時点でその一点を向けておくのが、狙ったところに当てるための要
    const Hagine::Vector3 muzzle = context.transform_->translation_;
    const Hagine::Vector3 aimPoint = aimPoint_;

    const Hagine::Vector3 toAimPoint = aimPoint - muzzle;
    const Hagine::Vector3 direction = (toAimPoint.LengthSq() > 0.0001f) ? toAimPoint.Normalize()
                                                                        : context.aimDirection_;

    PlayerWeapon::FireRequest request{};
    request.origin = muzzle;
    request.direction = direction;
    request.rgba = target ? target->GetColorRgba(selectedColor_)
                          : Hagine::Vector4{1.0f, 1.0f, 1.0f, 1.0f};

    // 追尾先は「動く的」ではなく、発射時に確定したワールドの一点。
    // 相手が動いても弾は追いかけないかわりに、補正はマズルとカメラの視差を詰めるだけの
    // 仕事で済むので、弱い補正のまま画面中心へ収束する
    request.targetPositionGetter = [aimPoint](Hagine::Vector3& out) {
        out = aimPoint;
        return true;
    };

    // 撃つ相手がいなければ、ただ飛んで消えるだけの弾になる
    if (!target) {
        weapon_->Fire(*context.bullets, request);
        return;
    }

    // 着弾は IBossTargetQuery::RaycastAttach へ「動いた線分」を渡して判定してもらう
    const Color shotColor = selectedColor_;
    request.hitTester = [this, shotColor](const Hagine::Vector3& from, const Hagine::Vector3& to) {
        IBossTargetQuery* hitTarget = ActiveTarget();
        if (!hitTarget) {
            return false;
        }

        const BulletHitResult result = hitTarget->RaycastAttach(from, to, shotColor);
        if (!result.hit) {
            return false; // 穴を素通りした。弾はそのまま飛ぶ
        }

        lastHit_ = result;
        if (result.destroyed) {
            Hagine::ImGuiNotification::Post("同色 " + std::to_string(result.clusterSize) + " 個 消去！",
                                            {1.0f, 0.8f, 0.3f, 1.0f});
        }
        return result.ShouldConsumeBullet();
    };

    weapon_->Fire(*context.bullets, request);
}

void PlayerShootComponent::DrawAimLine(const PlayerContext& context) const {
    Hagine::LineRenderer* lineRenderer = Hagine::LineRenderer::GetInstance();

    // 当たって決まった着弾地点は黄、何にも当たらず射程の端に置いただけなら灰
    const Hagine::Vector4 aimColor = aimPointHit_ ? Hagine::Vector4{1.0f, 1.0f, 0.4f, 1.0f}
                                                  : Hagine::Vector4{0.4f, 0.4f, 0.45f, 1.0f};

    // カメラの射線（画面中心）
    lineRenderer->AddLine(context.aimOrigin_, aimPoint_, {0.4f, 0.4f, 0.45f, 1.0f});

    // 実際に弾が通る線。カメラの射線とのひらき具合がそのまま視差のズレになる
    lineRenderer->AddLine(context.transform_->translation_, aimPoint_, aimColor);
    lineRenderer->AddSphere(aimPoint_, 0.7f, aimColor, 12);
}

void PlayerShootComponent::RegisterParams() {
    const std::string paramOwnerLabel = "Player/Shoot";
    Hagine::GameParamHub* hub = Hagine::GameParamHub::GetInstance();

    hub->Register(paramOwnerLabel, "DrawAimLine", &drawAimLine_);
    hub->Register(paramOwnerLabel, "AimRayLength", &aimRayLength_, {1.0f, 10.0f, 1000.0f});

    // 弾の飛び方は武器が持っている（Player::Init が SetWeapon を先に済ませている）
    if (!weapon_) {
        return;
    }

    PlayerWeapon::Params& params = weapon_->GetParams();
    hub->Register(paramOwnerLabel, "FireInterval", &params.fireInterval, {0.01f, 0.02f, 2.0f});
    hub->Register(paramOwnerLabel, "BulletSpeed", &params.speed, {0.5f, 1.0f, 200.0f});
    hub->Register(paramOwnerLabel, "BulletLifeTime", &params.lifeTime, {0.1f, 0.1f, 20.0f});
    hub->Register(paramOwnerLabel, "CorrectionRate", &params.correctionRate, {0.1f, 0.0f, 60.0f});
    hub->Register(paramOwnerLabel, "MaxTurnDegrees", &params.maxTurnDegreesPerSecond, {1.0f, 0.0f, 360.0f});
    hub->Register(paramOwnerLabel, "BulletRadius", &params.radius, {0.01f, 0.05f, 3.0f});
}

void PlayerShootComponent::DrawImGui() {
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader("プレイヤーの射撃", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::TextDisabled("[1][2][3][4] / 十字キー 色切替  ・  [J] / RT 発射");

    ImGui::SeparatorText("色");
    IBossTargetQuery* target = ActiveTarget();
    const Hagine::Vector4 rgba = target ? target->GetColorRgba(selectedColor_)
                                        : Hagine::Vector4{1.0f, 1.0f, 1.0f, 1.0f};
    ImGui::TextColored(ImVec4(rgba.x, rgba.y, rgba.z, rgba.w), "%s", GetColorIdText(selectedColor_));

    ImGui::SeparatorText("着弾地点（画面中心）");
    if (aimPointHit_) {
        ImGui::Text("命中: (%.1f, %.1f, %.1f)", aimPoint_.x, aimPoint_.y, aimPoint_.z);
    } else {
        ImGui::TextDisabled("何にも当たらない方向（射程 %.0f の端を狙う）", aimRayLength_);
    }

    ImGui::SeparatorText("ロックオン（色の判定・強調表示のみ。弾は誘導しない）");
    if (!target) {
        ImGui::TextDisabled("撃つ相手が配線されていません");
    } else if (lockOn_.IsValid()) {
        ImGui::Text("対象セル: 頂点%d 層%d  角度 %.1f度  距離 %.1f",
                    lockOn_.cell.vertex, lockOn_.cell.layer,
                    lockOn_.angleDegrees, lockOn_.distance);
    } else {
        ImGui::TextDisabled("対象なし（照準内に同色の球がありません）");
    }
    ImGui::Checkbox("照準線・着弾地点を表示", &drawAimLine_);

    ImGui::SeparatorText("直近の着弾");
    if (!lastHit_.hit) {
        ImGui::TextDisabled("未着弾（穴を素通りした）");
    } else if (!lastHit_.attached) {
        ImGui::Text("当たったが置ける隣が無く、弾は消えた");
    } else if (!lastHit_.destroyed) {
        ImGui::Text("付着した（同色 %d 個が繋がっている）", lastHit_.clusterSize);
    } else {
        ImGui::Text("同色 %d 個 消去！ 怯み %.2f秒", lastHit_.clusterSize, lastHit_.staggerTime);
    }
#endif // USE_IMGUI
}
