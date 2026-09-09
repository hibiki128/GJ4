#include "PlayerShootComponent.h"
#include "Frame/Frame.h"
#include "Utility/Debug/Param/GameParamHub.h"
#include "debug/imgui/ImGuiNotification.h"
#include "line/LineRenderer.h"
#include "src/Character/Player/Components/Ammo/PlayerAmmoComponent.h"
#include "src/Character/Player/Weapon/Bullet/Manager/PlayerBulletManager.h"
#include <algorithm>
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

    // エイムアシスト。レティクルが乗っている球の真ん中へ狙いを寄せる。
    // 照準そのもの（aimPoint_）は動かさないので、白い十字は画面中央に固定のまま。
    // 動くのは「実際に狙う一点」だけで、その結果は発射レティクルとして画面に出る
    assistPoint_ = ResolveAssistPoint();

    // 弾が本当に当たる点も撃つ前から求めておく。狙う先が同じでも、射線を飛ばす起点が
    // カメラではなくマズルなので、途中の球に先にぶつかることがある。
    // このズレも発射レティクルに出る
    const Hagine::Vector3 muzzle = context.transform_->translation_;
    firePoint_ = ResolveFirePoint(target, muzzle,
                                  ResolveFireDirection(muzzle, context.aimDirection_));

    // 表示側へ渡す報告をここで作る。射撃が決めた値をそのまま入れるだけで、
    // 表示のために作り直した値は入れない（仕様書 17.1）
    aimReport_.aimPoint = aimPoint_;
    aimReport_.aimPointHit = aimPointHit_;
    aimReport_.firePoint = firePoint_;
    aimReport_.firePointHit = firePointHit_;

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

    if (!weapon_ || !context.bullets || !context.ammoComponent_) {
        return;
    }

    // 残弾を先に押さえる。ここより前の早期 return に混ぜると、
    // 撃っていないのに弾数だけ減るフレームができてしまう
    if (!context.ammoComponent_->TryConsume(selectedColor_)) {
        return; // 弾切れ。クールダウンも進めない
    }

    // プールが埋まっていると弾は出ない。その場合は押さえた残弾を戻して、
    // 撃てなかったフレームとして扱う
    if (!FireBullet(context, target)) {
        context.ammoComponent_->Refund(selectedColor_);
        return;
    }

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
    AimHit hit{};
    if (!target->RaycastPoint(origin, farPoint, selectedColor_, hit)) {
        return farPoint; // 何にも当たらない方向。射程の端を狙って真っ直ぐ飛ばす
    }

    aimPointHit_ = true;
    aimHitCenter_ = hit.center; // エイムアシストの寄せ先（当たった球の真ん中）
    return hit.point;
}

Hagine::Vector3 PlayerShootComponent::ResolveAssistPoint() const {
    // 何にも当たっていないならアシストのしようがない。照準の点をそのまま狙う
    if (!aimPointHit_ || !aimAssistEnabled_) {
        return aimPoint_;
    }

    // 照準が乗っている球の表面から、その球の真ん中へ寄せる。
    // 強さ 1 で真ん中ぴったり、0 で寄せない（＝アシスト切）。
    // 球の縁をかすっているときほど寄る距離が大きくなるので、
    // 「当たってはいるが端」という一番外しやすい状況に効く
    const float strength = std::clamp(aimAssistStrength_, 0.0f, 1.0f);
    return aimPoint_ + (aimHitCenter_ - aimPoint_) * strength;
}

Hagine::Vector3 PlayerShootComponent::ResolveFireDirection(const Hagine::Vector3& muzzle,
                                                           const Hagine::Vector3& fallback) const {
    // 狙う先はアシストを効かせた後の一点。照準の点（aimPoint_）ではない
    const Hagine::Vector3 toAssistPoint = assistPoint_ - muzzle;
    return (toAssistPoint.LengthSq() > 0.0001f) ? toAssistPoint.Normalize() : fallback;
}

Hagine::Vector3 PlayerShootComponent::ResolveFirePoint(IBossTargetQuery* target,
                                                       const Hagine::Vector3& muzzle,
                                                       const Hagine::Vector3& direction) {
    const Hagine::Vector3 farPoint = muzzle + direction * aimRayLength_;

    firePointHit_ = false;
    if (!target) {
        return farPoint;
    }

    // 弾の着弾判定（RaycastAttach）と同じ形状・同じ色の扱いを通る問い合わせ。
    // 副作用は起こさないので、毎フレーム呼んでも付着や消去は発生しない
    AimHit hit{};
    if (!target->RaycastPoint(muzzle, farPoint, selectedColor_, hit)) {
        return farPoint; // 弾は何にも当たらずに飛んでいく
    }

    firePointHit_ = true;
    return hit.point;
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

bool PlayerShootComponent::FireBullet(PlayerContext& context, IBossTargetQuery* target) {
    // 狙いは画面中心が指している着弾地点、弾が出るのはプレイヤーの位置。
    // 初速の時点でその一点を向けておくのが、狙ったところに当てるための要
    const Hagine::Vector3 muzzle = context.transform_->translation_;
    // 狙うのはエイムアシストを効かせた後の一点（球の真ん中）。
    // 初速の向きも追尾先もここへそろえないと、アシストが効かない
    const Hagine::Vector3 aimPoint = assistPoint_;

    // 発射レティクルが射線を飛ばすのに使ったものと同じ関数。
    // ここで別計算をすると、表示と弾の飛ぶ向きがずれる（仕様書 17.1 / 17.3）
    const Hagine::Vector3 direction = ResolveFireDirection(muzzle, context.aimDirection_);

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
        return weapon_->Fire(*context.bullets, request);
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

    return weapon_->Fire(*context.bullets, request);
}

void PlayerShootComponent::DrawAimLine(const PlayerContext& context) const {
    Hagine::LineRenderer* lineRenderer = Hagine::LineRenderer::GetInstance();

    // 当たって決まった着弾地点は黄、何にも当たらず射程の端に置いただけなら灰
    const Hagine::Vector4 aimColor = aimPointHit_ ? Hagine::Vector4{1.0f, 1.0f, 0.4f, 1.0f}
                                                  : Hagine::Vector4{0.4f, 0.4f, 0.45f, 1.0f};

    // カメラの射線（画面中心）。この先にあるのが照準レティクルの指す点
    lineRenderer->AddLine(context.aimOrigin_, aimPoint_, {0.4f, 0.4f, 0.45f, 1.0f});
    lineRenderer->AddSphere(aimPoint_, 0.7f, aimColor, 12);

    // エイムアシストで寄せた先（球の真ん中）。上の黄色い点からここへ引っ張られている
    if (aimPointHit_ && aimAssistEnabled_) {
        const Hagine::Vector4 assistColor = {0.3f, 1.0f, 0.6f, 1.0f};
        lineRenderer->AddLine(aimPoint_, assistPoint_, assistColor);
        lineRenderer->AddSphere(assistPoint_, 0.35f, assistColor, 12);
    }

    // 実際に弾が通る線と、その先で最初に当たる点。
    // 同じ一点を狙っていても起点がマズルなので、途中の球へ先にぶつかることがある。
    // 上の点とここがずれているときに、画面へ発射レティクル（水色の円）が出る
    const Hagine::Vector4 fireColor = firePointHit_ ? Hagine::Vector4{1.0f, 0.3f, 0.2f, 1.0f}
                                                    : Hagine::Vector4{0.4f, 0.4f, 0.45f, 1.0f};
    lineRenderer->AddLine(context.transform_->translation_, firePoint_, fireColor);
    if (firePointHit_) {
        lineRenderer->AddSphere(firePoint_, 0.5f, fireColor, 12);
    }
}

void PlayerShootComponent::RegisterParams() {
    const std::string paramOwnerLabel = "Player/Shoot";
    Hagine::GameParamHub* hub = Hagine::GameParamHub::GetInstance();

    hub->Register(paramOwnerLabel, "DrawAimLine", &drawAimLine_);
    hub->Register(paramOwnerLabel, "AimRayLength", &aimRayLength_, {1.0f, 10.0f, 1000.0f});
    hub->Register(paramOwnerLabel, "AimAssist", &aimAssistEnabled_);
    hub->Register(paramOwnerLabel, "AimAssistStrength", &aimAssistStrength_, {0.01f, 0.0f, 1.0f});

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

    ImGui::SeparatorText("エイムアシスト");
    if (!aimAssistEnabled_) {
        ImGui::TextDisabled("切（照準の点をそのまま狙う）");
    } else if (!aimPointHit_) {
        ImGui::TextDisabled("照準が球に乗っていないので効かない");
    } else {
        // 表面から真ん中へ何メートル寄せたか。球の縁をかすっているときほど大きくなる
        ImGui::Text("寄せた距離: %.2f （強さ %.2f）", (assistPoint_ - aimPoint_).Length(),
                    aimAssistStrength_);
    }

    ImGui::SeparatorText("弾が実際に当たる点（発射レティクル）");
    if (!firePointHit_) {
        ImGui::TextDisabled("弾は何にも当たらずに飛んでいく（発射レティクルは出ない）");
    } else {
        ImGui::Text("命中: (%.1f, %.1f, %.1f)", firePoint_.x, firePoint_.y, firePoint_.z);
        // ここが 0 に近いほど「狙ったところに当たる」。
        // 離れるほど画面上でも照準レティクルから発射レティクルが離れていく
        ImGui::Text("照準の点とのズレ: %.2f", (firePoint_ - aimPoint_).Length());
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
