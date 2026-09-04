#include "BossSpiderLeg.h"
#include "Easing.h"
#include "MyMath.h"
#include "camera/projection/ViewProjection.h"
#include "object/base/BaseObject.h"
#include <algorithm>
#include <cmath>
#include <numbers>

using namespace Hagine;

namespace {

/// <summary>胴を上から見たときの向きから、水平方向のベクトルを作る</summary>
Vector3 MakeHorizontalDirection(float angle) {
    return Vector3{std::cos(angle), 0.0f, std::sin(angle)};
}

/// <summary>
/// 上腿・下腿ひと節ぶんの球の数を求める。
/// 折れ線の全長の半分を球の直径で割れば、球どうしが接して連なる数になる
/// </summary>
int AutoSphereCountPerBone(const BossSpiderParams &params) {
    const float diameter = (std::max)(0.01f, params.legSphereRadius * 2.0f);
    const float boneLength = BossSpiderLeg::ResolvePathLength(params) * 0.5f;
    // 切り上げ側に寄せて、隙間が空くより少し重なるようにする
    // （2%程度の隙間は見えないので、そのぶんだけ許して球が増えすぎないようにしている）
    const float span = std::ceil(boneLength / diameter * 0.98f);
    return (std::max)(2, static_cast<int>(span) + 1);
}

/// <summary>
/// 向きだけを混ぜる（長さは変えない）。
/// from から to へ、途中で長さがつぶれないように正規化しながら寄せていく
/// </summary>
Vector3 BlendDirection(const Vector3 &from, const Vector3 &to, float t) {
    const Vector3 target = (to.LengthSq() > 0.0001f) ? to.Normalize() : from;
    const Vector3 mixed = Lerp(from, target, t);
    return (mixed.LengthSq() > 0.0001f) ? mixed.Normalize() : target;
}

/// <summary>踏み替えのどこで足がいちばん高くなるか（0〜1）。前寄りだと素早く持ち上がる</summary>
constexpr float kStepApexRatio = 0.35f;

} // namespace

float BossSpiderLeg::ResolvePathLength(const BossSpiderParams &params) {
    // 膝で折るぶん、折れ線は真っ直ぐ伸ばしたときより kneeLift の2倍だけ長くなる。
    // 上腿と下腿はこの長さを球の数の比で分け合う
    return (std::max)(0.2f, params.legLength + params.kneeLift * 2.0f);
}

int BossSpiderLeg::ResolveUpperSphereCount(const BossSpiderParams &params) {
    if (params.upperSphereCount > 0) {
        return (std::max)(2, params.upperSphereCount);
    }
    return AutoSphereCountPerBone(params);
}

int BossSpiderLeg::ResolveLowerSphereCount(const BossSpiderParams &params) {
    if (params.lowerSphereCount > 0) {
        return (std::max)(2, params.lowerSphereCount);
    }
    return AutoSphereCountPerBone(params);
}

int BossSpiderLeg::ResolveSphereCount(const BossSpiderParams &params) {
    // 膝の球は上腿と下腿で共有するので、単純な足し算から1つ引く
    return ResolveUpperSphereCount(params) + ResolveLowerSphereCount(params) - 1;
}

float BossSpiderLeg::ResolveAzimuth(int legIndex, int legCount, const BossSpiderParams &params) {
    constexpr float kPi = std::numbers::pi_v<float>;
    const int count = (std::max)(1, legCount);

    // まず円周に等間隔で並べる。真正面・真後ろに脚が来ないよう半分ずらすと、
    // 前後に2本ずつ・左右に3本ずつという蜘蛛らしい並びになる
    float azimuth = (static_cast<float>(legIndex) + 0.5f) / static_cast<float>(count) * (2.0f * kPi);

    // 左右それぞれの真横を中心に、中心からのずれを legSpread/180 倍へ縮める。
    // 180 なら等間隔のまま、小さくするほど脚が真横へ寄って密集する。
    // 並び順（円周をぐるりと回る順）は変えないので、隣の脚と同時に浮かせない歩容の規則もそのまま効く
    const bool isNearSide = (azimuth < kPi);
    const float center = isNearSide ? (kPi * 0.5f) : (kPi * 1.5f);
    const float ratio = std::clamp(params.legSpread, 0.0f, 360.0f) / 180.0f;
    azimuth = center + (azimuth - center) * ratio;

    // 扇全体を前後へずらす。正面へ寄せる向きは左右で逆になる
    const float offset = params.legSpreadOffset * (kPi / 180.0f);
    azimuth += isNearSide ? -offset : offset;
    return azimuth;
}

void BossSpiderLeg::EnsureSpheres(const std::string &namePrefix, int count, float radius) {
    // 足りない分だけ作る。減らすときも破棄はしない（実行中の解放はGPUが参照中で危険）
    while (static_cast<int>(spheres_.size()) < count) {
        auto sphere = std::make_unique<BossSphere>();
        sphere->InitSphere(namePrefix + "_" + std::to_string(spheres_.size()), radius);
        spheres_.push_back(std::move(sphere));
    }
}

void BossSpiderLeg::Configure(const std::string &namePrefix, int legIndex, int legCount,
                              const BossSpiderParams &params, const BossColorPalette &palette) {
    legIndex_ = legIndex;

    azimuth_ = ResolveAzimuth(legIndex, legCount, params);

    const std::vector<Color> &usedColors = palette.GetUsedColors();
    upperSphereCount_ = ResolveUpperSphereCount(params);
    lowerSphereCount_ = ResolveLowerSphereCount(params);
    const int jointCount = upperSphereCount_ + lowerSphereCount_ - 1;

    EnsureSpheres(namePrefix + std::to_string(legIndex), jointCount, params.legSphereRadius);
    activeSphereCount_ = jointCount;
    isHidden_ = false;

    for (int joint = 0; joint < static_cast<int>(spheres_.size()); ++joint) {
        BossSphere *sphere = spheres_[static_cast<size_t>(joint)].get();
        if (joint >= jointCount) {
            sphere->Deactivate(); // 余った球は隠すだけ
            continue;
        }

        // 付け根から足先へ向かって色が移り変わるようにする
        const Color color = usedColors.empty()
                                ? Color::RED
                                : usedColors[static_cast<size_t>(legIndex + joint) % usedColors.size()];
        // 脚の球は格子には属さないので、セルは識別用にだけ使う
        sphere->Place(ShellCell{legIndex, joint}, Vector3{0.0f, 0.0f, 0.0f}, color, palette.GetRgba(color));
        sphere->SetSphereRadius(params.legSphereRadius);
    }
}

void BossSpiderLeg::SetHidden(bool hidden) {
    isHidden_ = hidden;
    for (int joint = 0; joint < static_cast<int>(spheres_.size()); ++joint) {
        BossSphere *sphere = spheres_[static_cast<size_t>(joint)].get();
        const bool visible = !hidden && joint < activeSphereCount_;
        sphere->SetIsAlive(visible);
        sphere->SetIsModelDraw(visible);
    }
}

Vector3 BossSpiderLeg::CalcHomePosition(const Vector3 &bodyPosition, float bodyYaw,
                                        const BossSpiderParams &params) const {
    const Vector3 direction = MakeHorizontalDirection(bodyYaw + azimuth_);
    // 高さは球の半径ぶん上げる。0にすると足の球が地面に半分めり込む
    return Vector3{bodyPosition.x + direction.x * params.footRadius, params.legSphereRadius,
                   bodyPosition.z + direction.z * params.footRadius};
}

Vector3 BossSpiderLeg::CalcHipPosition(const Vector3 &bodyPosition, float bodyYaw,
                                       const BossSpiderParams &params) const {
    const Vector3 direction = MakeHorizontalDirection(bodyYaw + azimuth_);
    // 胴の少し上側から生やすと、脚が胴に埋まらず「肩」らしく見える
    return bodyPosition + direction * (params.bodyRadius * 0.85f) +
           Vector3{0.0f, params.bodyRadius * 0.35f, 0.0f};
}

void BossSpiderLeg::ResetFoot(const Vector3 &bodyPosition, float bodyYaw, const BossSpiderParams &params) {
    footPosition_ = CalcHomePosition(bodyPosition, bodyYaw, params);
    stepFrom_ = footPosition_;
    stepTo_ = footPosition_;
    stepTimer_ = 0.0f;
    isStepping_ = false;

    PlacePose(bodyPosition, bodyYaw, params);
}

void BossSpiderLeg::Update(const Vector3 &bodyPosition, float bodyYaw, const Vector3 &moveDirection,
                           const BossSpiderParams &params, bool canStartStep, float deltaTime) {
    const Vector3 home = CalcHomePosition(bodyPosition, bodyYaw, params);

    if (isStepping_) {
        stepTimer_ += deltaTime;
        const float duration = (std::max)(0.01f, params.stepTime);
        const float progress = std::clamp(stepTimer_ / duration, 0.0f, 1.0f);

        // 水平は緩やかに寄せる（両端で速度が0になるので、踏み出しも着地も滑らか）
        Vector3 position = Lerp(stepFrom_, stepTo_, SmoothInOut(progress));

        // 上下は山なり。sin をそのまま使うと着地の瞬間まで落下速度が残ったままで、
        // 接地した途端に速度が0へ飛ぶのでカクついて見える。
        // 頂点で2本のイージングに分けて、上げは素早く・下ろしは速度0で着地させる
        if (progress < kStepApexRatio) {
            position.y += ApplyEasing(EasingType::InOutQuad, 0.0f, params.stepHeight, progress, kStepApexRatio);
        } else {
            position.y += ApplyEasing(EasingType::InOutCubic, params.stepHeight, 0.0f,
                                      progress - kStepApexRatio, 1.0f - kStepApexRatio);
        }
        footPosition_ = position;

        if (progress >= 1.0f) {
            footPosition_ = stepTo_;
            isStepping_ = false;
        }
    } else {
        // 接地中の足はワールドに貼り付いたまま。胴が離れすぎたら踏み替える
        const Vector3 offset = footPosition_ - home;
        const float distance = std::sqrt(offset.x * offset.x + offset.z * offset.z);
        if (canStartStep && distance > params.stepTrigger) {
            stepFrom_ = footPosition_;
            // 進行方向へ少し踏み越すと、歩みが前へ進む
            stepTo_ = home + moveDirection * params.stepLead;
            stepTo_.y = params.legSphereRadius;
            stepTimer_ = 0.0f;
            isStepping_ = true;
        }
    }

}

void BossSpiderLeg::PlacePose(const Vector3 &bodyPosition, float bodyYaw, const BossSpiderParams &params,
                              float growth, float bend) {
    if (spheres_.empty() || isHidden_ || upperSphereCount_ < 2 || lowerSphereCount_ < 2) {
        return;
    }

    const Vector3 hip = CalcHipPosition(bodyPosition, bodyYaw, params);
    const Vector3 restKnee = SolveKnee(hip, params);

    // 出現姿勢は付け根から真横へ真っ直ぐ、通常姿勢は膝を折って足を地面へ。
    // このふたつは「節の向き」だけを混ぜる。球の位置を直接混ぜると、途中で節が
    // 縮んで球が寄り集まり、そこから伸び直すぶんが巻き戻し（逆再生）に見える。
    // 向きだけを混ぜれば節の長さは変わらないので、関節がただ折れていくように見える
    const float blend = std::clamp(bend, 0.0f, 1.0f);
    const float upperLength = (restKnee - hip).Length();
    const float lowerLength = (footPosition_ - restKnee).Length();

    const Vector3 emergeDirection = MakeHorizontalDirection(bodyYaw + azimuth_);
    const Vector3 upperDirection = BlendDirection(emergeDirection, restKnee - hip, blend);
    const Vector3 lowerDirection = BlendDirection(emergeDirection, footPosition_ - restKnee, blend);

    const Vector3 knee = hip + upperDirection * upperLength;
    const Vector3 foot = knee + lowerDirection * lowerLength;

    // 何個目まで生えたか。端数がその球の「生えかけ具合」になる
    const float emerged = std::clamp(growth, 0.0f, 1.0f) * static_cast<float>(activeSphereCount_);

    for (int joint = 0; joint < activeSphereCount_; ++joint) {
        BossSphere *sphere = spheres_[static_cast<size_t>(joint)].get();

        const float appear = std::clamp(emerged - static_cast<float>(joint), 0.0f, 1.0f);
        const bool isVisible = appear > 0.0f;
        sphere->SetIsAlive(isVisible);
        sphere->SetIsModelDraw(isVisible);
        if (!isVisible) {
            continue;
        }

        // 生えかけの球は、ひとつ内側の球の位置から自分の位置へ押し出されてくる。
        // こうすると先端が伸びていくように見えて、いきなり列が現れない
        const float slot = Lerp(static_cast<float>((std::max)(0, joint - 1)), static_cast<float>(joint), appear);
        sphere->SetLocalPosition(PointAlongLeg(slot, hip, knee, foot));
        // 出てくる瞬間だけ小さく、押し出されるにつれて本来の大きさになる
        sphere->SetSphereRadius(params.legSphereRadius *
                                ApplyEasing(EasingType::OutQuad, 0.0f, 1.0f, appear, 1.0f));
    }
}

Vector3 BossSpiderLeg::SolveKnee(const Vector3 &hip, const BossSpiderParams &params) {
    // 上腿と下腿の長さを「球と球のあいだの数」の比で分ける。こうすると節が違う
    // 本数でも球の間隔がそろうので、片側だけ重なって反対側に隙間が空くことがない
    const int upperSpan = upperSphereCount_ - 1;
    const int lowerSpan = lowerSphereCount_ - 1;
    float pathLength = ResolvePathLength(params);

    const Vector3 toFoot = footPosition_ - hip;
    // 伸ばしきっても届かない距離なら、折れ線が閉じるところまで内側に見なす
    const float distance = std::clamp(toFoot.Length(), 0.01f, pathLength * 0.999f);

    const float weight = static_cast<float>(upperSpan) / static_cast<float>(upperSpan + lowerSpan);

    // 片方の節が長すぎると膝が足を通り越し、脚が地面へ突き刺さって見える。
    // そうなる手前まで、比は保ったまま折れ線を縮める（膝が足元へ寄り、脚がまっすぐに近づく）。
    // 比を歪めると片側だけ間隔が変わってしまうので、縮める方を選んでいる
    const float bias = std::fabs(weight * 2.0f - 1.0f);
    if (bias > 0.0001f && pathLength * pathLength * bias > distance * distance) {
        pathLength = distance / std::sqrt(bias) * 0.999f;
    }

    const float upperBone = pathLength * weight;
    const float lowerBone = pathLength - upperBone;
    sphereSpacing_ = pathLength / static_cast<float>(upperSpan + lowerSpan);

    const Vector3 direction = (toFoot.LengthSq() > 0.0001f) ? toFoot.Normalize() : Vector3{0.0f, -1.0f, 0.0f};

    // 膝を折る向き。脚の伸びる向きと直交する成分のうち、上向きを選ぶ
    Vector3 up{0.0f, 1.0f, 0.0f};
    Vector3 kneeUp = up - direction * up.Dot(direction);
    kneeUp = (kneeUp.LengthSq() > 0.0001f) ? kneeUp.Normalize() : Vector3{0.0f, 1.0f, 0.0f};

    // 2辺の長さと底辺から頂点を求める（余弦定理）。
    // along は付け根から膝までの「足へ向かう向き」の距離、height はそこから折れ上がる距離
    const float along = (distance * distance + upperBone * upperBone - lowerBone * lowerBone) / (2.0f * distance);
    const float height = std::sqrt((std::max)(0.0f, upperBone * upperBone - along * along));
    return hip + direction * along + kneeUp * height;
}

Vector3 BossSpiderLeg::PointAlongLeg(float index, const Vector3 &hip, const Vector3 &knee,
                                     const Vector3 &foot) const {
    // 膝の球は両方の節の端なので、上腿の最後の1個をそのまま使い回す
    const float upperSpan = static_cast<float>(upperSphereCount_ - 1);
    if (index <= upperSpan) {
        return Lerp(hip, knee, index / upperSpan);
    }
    const float lowerSpan = static_cast<float>(lowerSphereCount_ - 1);
    return Lerp(knee, foot, (index - upperSpan) / lowerSpan);
}

void BossSpiderLeg::Draw(const ViewProjection &viewProjection) {
    if (isHidden_) {
        return;
    }
    for (int joint = 0; joint < activeSphereCount_ && joint < static_cast<int>(spheres_.size()); ++joint) {
        spheres_[static_cast<size_t>(joint)]->Draw(viewProjection);
    }
}
