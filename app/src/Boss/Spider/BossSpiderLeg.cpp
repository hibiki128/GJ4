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

    namePrefix_ = namePrefix + std::to_string(legIndex);
    azimuth_ = ResolveAzimuth(legIndex, legCount, params);

    const std::vector<Color> &usedColors = palette.GetUsedColors();
    upperSphereCount_ = ResolveUpperSphereCount(params);
    lowerSphereCount_ = ResolveLowerSphereCount(params);
    const int jointCount = upperSphereCount_ + lowerSphereCount_ - 1;

    // 組み直したらくっついていたぶんは無かったことにする
    for (const AttachedSlot &slot : attached_) {
        slot.sphere->Deactivate();
        freeAttached_.push_back(slot.sphere);
    }
    attached_.clear();
    removedBase_ = 0;
    extension_ = 0.0f;
    extendFrom_ = 0.0f;
    extendTarget_ = 0.0f;
    extendTimer_ = extendDuration_;

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
        // Place は殻の球向けに描画を切るので、脚の球はここで一度だけ戻しておく。
        // 以降 PlacePose では可視フラグを触らない（毎フレーム切り替えないため）
        sphere->SetIsAlive(true);
        sphere->SetIsModelDraw(true);
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

    // くっついたぶんも一緒に隠す
    for (const AttachedSlot &slot : attached_) {
        slot.sphere->SetIsAlive(!hidden);
        slot.sphere->SetIsModelDraw(!hidden);
    }
}

Vector3 BossSpiderLeg::CalcHomePosition(const Vector3 &bodyPosition, float bodyYaw,
                                        const BossSpiderParams &params) const {
    const Vector3 direction = MakeHorizontalDirection(bodyYaw + azimuth_);
    // くっついた球のぶんだけ足を置く位置も外へ伸ばす（脚が実際に長くなる）
    // 脚が消されたぶん縮むが、胴に食い込むほど短くはしない
    const float reach = (std::max)(params.bodyRadius * 1.1f,
                                   params.footRadius + extension_ * CalcSpacing(params));
    // 高さは球の半径ぶん上げる。0にすると足の球が地面に半分めり込む
    return Vector3{bodyPosition.x + direction.x * reach, params.legSphereRadius,
                   bodyPosition.z + direction.z * reach};
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
    // 脚が伸び縮みしたぶん、接地している足を外／内へ滑らせる。
    const float grown = AdvanceExtension(deltaTime) * CalcSpacing(params);
    if (std::fabs(grown) > 0.0f) {
        Vector3 outward = footPosition_ - bodyPosition;
        outward.y = 0.0f;
        if (outward.LengthSq() > 0.0001f) {
            const Vector3 slide = outward.Normalize() * grown;
            footPosition_ += slide;
            stepFrom_ += slide;
            stepTo_ += slide;
        }
    }

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

float BossSpiderLeg::CalcSpacing(const BossSpiderParams &params) const {
    // 基本の脚での球の間隔。くっついた球もこの間隔で先へ足していく
    const int baseSpan = (upperSphereCount_ - 1) + (lowerSphereCount_ - 1);
    return ResolvePathLength(params) / static_cast<float>((std::max)(1, baseSpan));
}


void BossSpiderLeg::BeginExtend(float duration) {
    // いまの位置から目標へ、両端で速度0になる曲線で寄せ直す
    extendFrom_ = extension_;
    // 継ぎ足したぶんから、消された脚のぶんを引いた量が目標
    extendTarget_ = static_cast<float>(static_cast<int>(attached_.size()) - removedBase_);
    extendTimer_ = 0.0f;
    extendDuration_ = (std::max)(0.01f, duration);
}

float BossSpiderLeg::AdvanceExtension(float deltaTime) {
    const float previous = extension_;
    if (extendTimer_ >= extendDuration_) {
        extension_ = extendTarget_;
        return extension_ - previous;
    }

    extendTimer_ += deltaTime;
    const float progress = std::clamp(extendTimer_ / extendDuration_, 0.0f, 1.0f);
    extension_ = Lerp(extendFrom_, extendTarget_, SmoothInOut(progress));
    return extension_ - previous;
}

int BossSpiderLeg::GetTipRunLength() const {
    Color tipColor = Color::RED;
    bool hasTip = false;
    int run = 0;

    // 先端から付け根へ向かって、同じ色が続くあいだ数える。
    // まずはくっついたぶん
    for (auto it = attached_.rbegin(); it != attached_.rend(); ++it) {
        if (!hasTip) {
            tipColor = it->color;
            hasTip = true;
        }
        if (it->color != tipColor) {
            return run;
        }
        ++run;
    }

    // 続けて、もともと脚だった球も同じ列として数える。
    // ただし膝（と、そのすぐ先の1個）は残す。脚が付け根まで消えると
    // 折れ線が閉じられず、足が地面に着けなくなるため
    for (int joint = activeSphereCount_ - 1; joint > upperSphereCount_; --joint) {
        const Color color = spheres_[static_cast<size_t>(joint)]->GetSphereColor();
        if (!hasTip) {
            tipColor = color;
            hasTip = true;
        }
        if (color != tipColor) {
            return run;
        }
        ++run;
    }
    return run;
}

bool BossSpiderLeg::Attach(Color color, const Vector3 &hitPoint, const BossColorPalette &palette,
                           const BossSpiderParams &params, const BossEffectParams &effect) {
    if (isHidden_) {
        return false;
    }
    // 伸ばしすぎないよう上限を設ける（0以下なら無制限）
    if (params.maxAttachPerLeg > 0 && static_cast<int>(attached_.size()) >= params.maxAttachPerLeg) {
        return false;
    }

    if (freeAttached_.empty()) {
        // 足りなければ増やすだけ。実行中に破棄するとGPUが参照中のリソースを解放して落ちる
        auto sphere = std::make_unique<BossSphere>();
        sphere->InitSphere(namePrefix_ + "_add" + std::to_string(attachedPool_.size()),
                           params.legSphereRadius);
        freeAttached_.push_back(sphere.get());
        attachedPool_.push_back(std::move(sphere));
    }

    BossSphere *sphere = freeAttached_.back();
    freeAttached_.pop_back();

    // セルは識別用（脚の番号と、付け根から数えた並び順）
    const int chainIndex = GetBaseSphereCount() + static_cast<int>(attached_.size());
    sphere->Place(ShellCell{legIndex_, chainIndex}, footPosition_, color, palette.GetRgba(color));
    sphere->SetSphereRadius(params.legSphereRadius);
    sphere->SetIsModelDraw(true);
    // 着弾点から吸い寄せられて先端に生える
    sphere->BeginAttach(hitPoint, effect.attachTime, effect.attachStartScale);

    attached_.push_back(AttachedSlot{sphere, color});
    // 継ぎ足し量を実数で滑らかに寄せる。整数で切り替えると脚が一瞬で詰め直される
    BeginExtend(effect.attachTime);
    return true;
}

int BossSpiderLeg::TryEliminate(int minMatch, const BossEffectParams &effect) {
    const int run = GetTipRunLength();
    if (run < (std::max)(2, minMatch)) {
        return 0;
    }

    // 先端から run 個ぶんを消す。くっついたぶんを使い切ったら、
    // もともと脚だった球も先端側から消していく（膝より内側は残る）
    for (int index = 0; index < run; ++index) {
        BossSphere *sphere = nullptr;
        bool fromAttachedPool = false;

        if (!attached_.empty()) {
            sphere = attached_.back().sphere;
            attached_.pop_back();
            fromAttachedPool = true;
        } else if (activeSphereCount_ > upperSphereCount_ + 1) {
            --activeSphereCount_;
            ++removedBase_;
            sphere = spheres_[static_cast<size_t>(activeSphereCount_)].get();
        } else {
            break; // これ以上は膝側なので消さない
        }

        // 消え切ったところで UpdateMotions が片付ける。ここで返すと
        // 演出中の球が次の着弾で再利用されてしまう
        sphere->BeginVanish(effect.vanishTime, effect.vanishDrift,
                            effect.vanishSpread * static_cast<float>(index));
        vanishing_.push_back(VanishSlot{sphere, fromAttachedPool});
    }

    // 消えたぶんだけ、脚も滑らかに縮む
    BeginExtend(effect.vanishTime);
    return run;
}

void BossSpiderLeg::UpdateMotions(float deltaTime, const BossSpiderParams &params) {
    for (AttachedSlot &slot : attached_) {
        slot.sphere->UpdateMotion(deltaTime, params.legSphereRadius);
    }

    for (size_t index = 0; index < vanishing_.size();) {
        VanishSlot &slot = vanishing_[index];
        if (slot.sphere->UpdateMotion(deltaTime, params.legSphereRadius)) {
            ++index;
            continue;
        }
        slot.sphere->Deactivate();
        // 継ぎ足し用の球だけをプールへ返す。もともと脚だった球は spheres_ に
        // 残したままで、activeSphereCount_ の外にあるので並べ直しの対象にならない
        if (slot.fromAttachedPool) {
            freeAttached_.push_back(slot.sphere);
        }
        vanishing_[index] = vanishing_.back();
        vanishing_.pop_back();
    }
}

bool BossSpiderLeg::Raycast(const Vector3 &start, const Vector3 &end, const BossSpiderParams &params,
                            float &outDistance, Vector3 &outPoint) const {
    if (isHidden_) {
        return false;
    }

    const Vector3 segment = end - start;
    const float segmentLength = segment.Length();
    if (segmentLength <= 0.0001f) {
        return false;
    }
    const Vector3 direction = segment / segmentLength;
    const float radius = params.legSphereRadius;

    bool found = false;
    float nearest = segmentLength;

    // 脚の球はワールド座標そのままなので、線分と球の交差をそのまま解く
    auto test = [&](const Vector3 &center) {
        const Vector3 toCenter = center - start;
        const float along = toCenter.Dot(direction);
        // 球の裏側から始まる線分も拾えるよう、半径ぶんの余裕を見る
        if (along < -radius || along > segmentLength + radius) {
            return;
        }
        const float perpendicularSq = toCenter.LengthSq() - along * along;
        if (perpendicularSq > radius * radius) {
            return;
        }
        const float back = std::sqrt((std::max)(0.0f, radius * radius - perpendicularSq));
        const float distance = (std::max)(0.0f, along - back);
        if (distance > nearest) {
            return;
        }
        nearest = distance;
        found = true;
    };

    for (int index = 0; index < activeSphereCount_ && index < static_cast<int>(spheres_.size()); ++index) {
        test(spheres_[static_cast<size_t>(index)]->GetRenderPosition());
    }
    for (const AttachedSlot &slot : attached_) {
        test(slot.sphere->GetRenderPosition());
    }

    if (!found) {
        return false;
    }
    outDistance = nearest;
    outPoint = start + direction * nearest;
    return true;
}


bool BossSpiderLeg::TryGetTipPosition(Vector3 &out) const {
    if (isHidden_) {
        return false;
    }
    // くっついた球があればその先端、無ければ基本の脚の足先
    if (!attached_.empty()) {
        out = attached_.back().sphere->GetRenderPosition();
        return true;
    }
    if (activeSphereCount_ <= 0 || spheres_.empty()) {
        return false;
    }
    out = spheres_[static_cast<size_t>(activeSphereCount_ - 1)]->GetRenderPosition();
    return true;
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
    // 変形中に「毎フレーム触る状態」を位置だけに絞る。
    // 可視フラグの切り替えやスケールの上げ下げを毎フレーム行うと、
    // 描画側の状態が毎フレーム揺れる（ちらつきの調査でここを疑っている）。
    // まだ生えていない球は胴（黒い球）の中に置いておけば、フラグを触らなくても見えない
    for (int joint = 0; joint < activeSphereCount_; ++joint) {
        BossSphere *sphere = spheres_[static_cast<size_t>(joint)].get();

        const float appear = std::clamp(emerged - static_cast<float>(joint), 0.0f, 1.0f);
        if (appear <= 0.0f) {
            sphere->SetLocalPosition(bodyPosition); // 胴の中へ隠す
            continue;
        }

        // 胴の中から自分の位置へ出てくる。appear=0 のときは胴の中と一致するので、
        // 出はじめに飛ぶことがない
        const Vector3 slotPosition = PointAlongLeg(static_cast<float>(joint), hip, knee, foot);
        sphere->SetLocalPosition(Lerp(bodyPosition, slotPosition, appear));
    }

    // くっついた球は基本の脚の先へ、同じ折れ線を延長する形で並べる。
    // 上腿の長さは変わらないので、関節（膝）の球はその場から動かない。
    // いちばん先の球は「ひとつ内側の位置」から折れ線の端へ押し出されてくるので、
    // 継ぎ足しが滑らかに進むあいだ、先端が伸びていくように見える
    const float baseCount = static_cast<float>(GetBaseSphereCount());
    const float tipIndex = baseCount - 1.0f + extension_;
    for (int index = 0; index < static_cast<int>(attached_.size()); ++index) {
        BossSphere *sphere = attached_[static_cast<size_t>(index)].sphere;
        const float slot = (std::min)(baseCount + static_cast<float>(index), tipIndex);
        sphere->SetLocalPosition(PointAlongLeg(slot, hip, knee, foot));
    }
}

Vector3 BossSpiderLeg::SolveKnee(const Vector3 &hip, const BossSpiderParams &params) {
    // 上腿と下腿の長さを「球と球のあいだの数」の比で分ける。こうすると節が違う
    // 本数でも球の間隔がそろうので、片側だけ重なって反対側に隙間が空くことがない。
    // くっついた球のぶんは下腿側にだけ足すので、上腿の長さ＝膝の位置は変わらない
    const float upperSpan = static_cast<float>(upperSphereCount_ - 1);
    const float lowerSpan = (std::max)(0.5f, static_cast<float>(lowerSphereCount_ - 1) + extension_);
    float pathLength = CalcSpacing(params) * (upperSpan + lowerSpan);

    const Vector3 toFoot = footPosition_ - hip;
    // 伸ばしきっても届かない距離なら、折れ線が閉じるところまで内側に見なす
    const float distance = std::clamp(toFoot.Length(), 0.01f, pathLength * 0.999f);

    const float weight = upperSpan / (upperSpan + lowerSpan);

    // 片方の節が長すぎると膝が足を通り越し、脚が地面へ突き刺さって見える。
    // そうなる手前まで、比は保ったまま折れ線を縮める（膝が足元へ寄り、脚がまっすぐに近づく）。
    // 比を歪めると片側だけ間隔が変わってしまうので、縮める方を選んでいる
    const float bias = std::fabs(weight * 2.0f - 1.0f);
    if (bias > 0.0001f && pathLength * pathLength * bias > distance * distance) {
        pathLength = distance / std::sqrt(bias) * 0.999f;
    }

    const float upperBone = pathLength * weight;
    const float lowerBone = pathLength - upperBone;
    sphereSpacing_ = pathLength / (upperSpan + lowerSpan);

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
    // 下腿はくっついた球のぶんだけ長くなっている
    const float lowerSpan = (std::max)(0.5f, static_cast<float>(lowerSphereCount_ - 1) + extension_);
    return Lerp(knee, foot, (index - upperSpan) / lowerSpan);
}

void BossSpiderLeg::Draw(const ViewProjection &viewProjection) {
    if (isHidden_) {
        return;
    }
    for (int joint = 0; joint < activeSphereCount_ && joint < static_cast<int>(spheres_.size()); ++joint) {
        spheres_[static_cast<size_t>(joint)]->Draw(viewProjection);
    }
    for (const AttachedSlot &slot : attached_) {
        slot.sphere->Draw(viewProjection);
    }
    for (const VanishSlot &slot : vanishing_) {
        slot.sphere->Draw(viewProjection);
    }
}
