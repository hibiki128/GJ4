#include "BossSpiderLeg.h"
#include "Easing.h"
#include "MyMath.h"
#include "Random.h"
#include "camera/projection/ViewProjection.h"
#include "object/base/BaseObject.h"
#include "src/Boss/Effect/BossParticles.h"
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

    // 組み直したら、くっついた球も切り落とし中の球もいったん片付ける
    for (const ChainSlot &slot : chain_) {
        if (slot.fromAttachedPool) {
            slot.sphere->Deactivate();
            freeAttached_.push_back(slot.sphere);
        }
    }
    for (const SeveredPiece &piece : severed_) {
        piece.sphere->Deactivate();
        piece.sphere->SetSphereRadius(params.legSphereRadius);
        if (piece.fromAttachedPool) {
            freeAttached_.push_back(piece.sphere);
        }
    }
    severed_.clear();
    chain_.clear();
    lastAttachIndex_ = -1;
    extension_ = 0.0f;
    extendFrom_ = 0.0f;
    extendTarget_ = 0.0f;
    extendTimer_ = extendDuration_;

    EnsureSpheres(namePrefix + std::to_string(legIndex), jointCount, params.legSphereRadius);
    baseChainCount_ = jointCount;
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
        chain_.push_back(ChainSlot{sphere, color, false});
    }
}

void BossSpiderLeg::SetHidden(bool hidden) {
    isHidden_ = hidden;
    // 使っていない球まで出さないよう、いま脚に付いているものだけを切り替える
    for (const std::unique_ptr<BossSphere> &sphere : spheres_) {
        sphere->SetIsAlive(false);
        sphere->SetIsModelDraw(false);
    }
    for (const ChainSlot &slot : chain_) {
        slot.sphere->SetIsAlive(!hidden);
        slot.sphere->SetIsModelDraw(!hidden);
    }
}

void BossSpiderLeg::ApplySphereRadius(float radius) {
    // 連なりに並べ直すのは PlacePose が毎フレームやってくれるので、
    // ここは持っている球すべてへ新しい半径を配るだけでよい
    for (const std::unique_ptr<BossSphere> &sphere : spheres_) {
        sphere->SetSphereRadius(radius);
    }
    for (const std::unique_ptr<BossSphere> &sphere : attachedPool_) {
        sphere->SetSphereRadius(radius);
    }
}

float BossSpiderLeg::CalcFootReach(const BossSpiderParams &params) const {
    // 脚が消されたぶん縮み、攻撃で広げたぶん伸びる。胴に食い込むほど短くはしない
    return (std::max)(params.bodyRadius * 1.1f,
                      (params.footRadius + extension_ * CalcSpacing(params)) * reachScale_);
}

Vector3 BossSpiderLeg::CalcHomePosition(const Vector3 &bodyPosition, float bodyYaw,
                                        const BossSpiderParams &params) const {
    const Vector3 direction = MakeHorizontalDirection(bodyYaw + azimuth_);
    const float reach = CalcFootReach(params);
    // 高さは球の半径ぶん上げる。0にすると足の球が地面に半分めり込む
    Vector3 home{bodyPosition.x + direction.x * reach, params.legSphereRadius,
                 bodyPosition.z + direction.z * reach};

    // 跳躍中は足を地面へ置けないので、脚を胴の下へ畳む
    if (legTuck_ > 0.0f) {
        const Vector3 under{bodyPosition.x + direction.x * reach * 0.35f,
                            bodyPosition.y - params.bodyRadius * 1.2f,
                            bodyPosition.z + direction.z * reach * 0.35f};
        home = Lerp(home, under, std::clamp(legTuck_, 0.0f, 1.0f));
    }
    return home;
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

    // 畳んでいるあいだは踏み替えをせず、足は胴について回る
    if (legTuck_ > 0.0f) {
        footPosition_ = home;
        stepFrom_ = home;
        stepTo_ = home;
        isStepping_ = false;
        return;
    }

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
            // 踏み下ろした足元に小さく砂ぼこりを立てる
            BossParticles::GetInstance()->BurstOnGround(BossParticles::Id::StepDust, footPosition_);
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
    // 組み立て直後からどれだけ増減したかが目標
    extendTarget_ = static_cast<float>(static_cast<int>(chain_.size()) - baseChainCount_);
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

int BossSpiderLeg::FindRunStart(int index) const {
    // 差し込んだ球と同じ色が、どこから続いているかを探す。
    // 付け根の球まで含めて消せるようにしてあるので、並びの内側に下限は置かない。
    // 付け根で消せば、そこから先はまとめて切り落とされて脚が根元から無くなる
    const Color color = chain_[static_cast<size_t>(index)].color;
    int start = index;
    while (start > 0 && chain_[static_cast<size_t>(start - 1)].color == color) {
        --start;
    }
    return start;
}

int BossSpiderLeg::FindRunEnd(int index) const {
    const Color color = chain_[static_cast<size_t>(index)].color;
    int end = index;
    const int last = static_cast<int>(chain_.size()) - 1;
    while (end < last && chain_[static_cast<size_t>(end + 1)].color == color) {
        ++end;
    }
    return end;
}

bool BossSpiderLeg::Attach(Color color, const Vector3 &hitPoint, int hitIndex,
                           const BossColorPalette &palette, const BossSpiderParams &params,
                           const BossEffectParams &effect) {
    if (isHidden_ || chain_.empty()) {
        return false;
    }
    // 伸ばしすぎないよう上限を設ける（0以下なら無制限）
    const int added = static_cast<int>(chain_.size()) - baseChainCount_;
    if (params.maxAttachPerLeg > 0 && added >= params.maxAttachPerLeg) {
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

    // 当たった球のすぐ外側へ差し込む。先端に当たれば先へ伸び、
    // 途中や付け根に当たればそこへ割り込んで、その先はまとめて外側へ押し出される。
    // 膝より内側を守る下限は置かない（狙った球の隣に必ず繋がる）
    const int insertAt = std::clamp(hitIndex + 1, 0, static_cast<int>(chain_.size()));

    sphere->Place(ShellCell{legIndex_, insertAt}, footPosition_, color, palette.GetRgba(color));
    sphere->SetSphereRadius(params.legSphereRadius);
    sphere->SetIsAlive(true);
    sphere->SetIsModelDraw(true);
    // 着弾点から吸い寄せられて生える
    sphere->BeginAttach(hitPoint, effect.attachTime, effect.attachStartScale);

    chain_.insert(chain_.begin() + insertAt, ChainSlot{sphere, color, true});
    lastAttachIndex_ = insertAt;

    // 継ぎ足し量を実数で滑らかに寄せる。整数で切り替えると脚が一瞬で詰め直される
    BeginExtend(effect.attachTime);
    return true;
}

int BossSpiderLeg::TryEliminate(int minMatch, const BossEffectParams &effect,
                                const BossSpiderParams &params, int &outSevered) {
    outSevered = 0;
    if (lastAttachIndex_ < 0 || lastAttachIndex_ >= static_cast<int>(chain_.size())) {
        return 0;
    }

    // 差し込んだ球を含む同色の並びを見る（消えるのはこの並びだけ）
    const int start = FindRunStart(lastAttachIndex_);
    const int end = FindRunEnd(lastAttachIndex_);
    const int run = end - start + 1;
    lastAttachIndex_ = -1;
    if (run < (std::max)(2, minMatch)) {
        return 0;
    }

    // 消える並びより先にまだ球が残っていれば、そこから先は繋がりを失う。
    // 消すのではなく「切り落として飛び散らせる」
    const int chainEnd = static_cast<int>(chain_.size()) - 1;
    for (int index = end + 1; index <= chainEnd; ++index) {
        BeginSever(chain_[static_cast<size_t>(index)], index, params);
        ++outSevered;
    }

    // 消える並びは、これまでどおり消滅演出で消す
    for (int index = start; index <= end; ++index) {
        ChainSlot &slot = chain_[static_cast<size_t>(index)];
        slot.sphere->BeginVanish(effect.vanishTime, effect.vanishDrift,
                                 effect.vanishSpread * static_cast<float>(index - start));
        vanishing_.push_back(VanishSlot{slot.sphere, slot.fromAttachedPool});
    }

    // 並びの手前までが残る脚になる
    chain_.erase(chain_.begin() + start, chain_.end());

    // 短くなったぶんだけ、脚も滑らかに縮む
    BeginExtend(effect.vanishTime);
    return run;
}

void BossSpiderLeg::BeginSever(const ChainSlot &slot, int index, const BossSpiderParams &params) {
    const BossSpiderSeverParams &sever = params.sever;

    SeveredPiece piece{};
    piece.sphere = slot.sphere;
    piece.fromAttachedPool = slot.fromAttachedPool;
    piece.position = slot.sphere->GetRenderPosition();
    piece.maxLife = (std::max)(0.1f, sever.life);
    piece.life = piece.maxLife;

    // 脚の伸びていた向きへ飛ばす。先の球ほど勢いよく飛ぶと、
    // ちぎれた先が振り抜かれたように見える
    Vector3 outward = piece.position - footPosition_;
    outward.y = 0.0f;
    if (outward.LengthSq() <= 0.0001f) {
        outward = MakeHorizontalDirection(azimuth_);
    } else {
        outward = outward.Normalize();
    }
    const float order = static_cast<float>(index) * 0.15f;
    piece.velocity = outward * (sever.speed + order) + Vector3{0.0f, sever.lift, 0.0f};
    piece.velocity += Vector3{Random::Range(-sever.scatter, sever.scatter),
                              Random::Range(0.0f, sever.scatter),
                              Random::Range(-sever.scatter, sever.scatter)};

    severed_.push_back(piece);
}

void BossSpiderLeg::UpdateSevered(float deltaTime, const BossSpiderParams &params) {
    const BossSpiderSeverParams &sever = params.sever;
    const float radius = params.legSphereRadius;

    for (size_t index = 0; index < severed_.size();) {
        SeveredPiece &piece = severed_[index];

        piece.velocity.y -= sever.gravity * deltaTime;
        piece.position += piece.velocity * deltaTime;

        // 地面で跳ねてから転がって止まる
        if (piece.position.y < radius) {
            piece.position.y = radius;
            piece.velocity.y = -piece.velocity.y * std::clamp(sever.bounce, 0.0f, 1.0f);
            piece.velocity.x *= 0.7f;
            piece.velocity.z *= 0.7f;
        }
        piece.sphere->SetLocalPosition(piece.position);

        // 終わりぎわだけ縮めて消す（唐突に消えないように）
        piece.life -= deltaTime;
        const float fade = std::clamp(piece.life / (piece.maxLife * 0.35f), 0.0f, 1.0f);
        piece.sphere->SetSphereRadius((std::max)(radius * 0.02f, radius * fade));

        if (piece.life > 0.0f) {
            ++index;
            continue;
        }
        piece.sphere->Deactivate();
        piece.sphere->SetSphereRadius(radius);
        if (piece.fromAttachedPool) {
            freeAttached_.push_back(piece.sphere);
        }
        severed_[index] = severed_.back();
        severed_.pop_back();
    }
}

void BossSpiderLeg::UpdateMotions(float deltaTime, const BossSpiderParams &params) {
    for (ChainSlot &slot : chain_) {
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
        // 残したままで、chain_ から外れているので並べ直しの対象にならない
        if (slot.fromAttachedPool) {
            freeAttached_.push_back(slot.sphere);
        }
        vanishing_[index] = vanishing_.back();
        vanishing_.pop_back();
    }

    UpdateSevered(deltaTime, params);
}

bool BossSpiderLeg::Raycast(const Vector3 &start, const Vector3 &end, const BossSpiderParams &params,
                            float &outDistance, Vector3 &outPoint, int &outIndex) const {
    if (isHidden_ || chain_.empty()) {
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
    int nearestIndex = -1;

    // 脚の球はワールド座標そのままなので、線分と球の交差をそのまま解く
    for (int index = 0; index < static_cast<int>(chain_.size()); ++index) {
        const Vector3 center = chain_[static_cast<size_t>(index)].sphere->GetRenderPosition();
        const Vector3 toCenter = center - start;
        const float along = toCenter.Dot(direction);
        // 球の裏側から始まる線分も拾えるよう、半径ぶんの余裕を見る
        if (along < -radius || along > segmentLength + radius) {
            continue;
        }
        const float perpendicularSq = toCenter.LengthSq() - along * along;
        if (perpendicularSq > radius * radius) {
            continue;
        }
        const float back = std::sqrt((std::max)(0.0f, radius * radius - perpendicularSq));
        const float distance = (std::max)(0.0f, along - back);
        if (found && distance > nearest) {
            continue;
        }
        nearest = distance;
        nearestIndex = index;
        found = true;
    }

    if (!found) {
        return false;
    }
    outDistance = nearest;
    outPoint = start + direction * nearest;
    outIndex = nearestIndex;
    return true;
}

bool BossSpiderLeg::TryGetTipPosition(Vector3 &out) const {
    if (isHidden_ || chain_.empty()) {
        return false;
    }
    out = chain_.back().sphere->GetRenderPosition();
    return true;
}

void BossSpiderLeg::PlacePose(const Vector3 &bodyPosition, float bodyYaw, const BossSpiderParams &params,
                              float growth, float bend) {
    if (chain_.empty() || isHidden_ || upperSphereCount_ < 2 || lowerSphereCount_ < 2) {
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
    const int count = static_cast<int>(chain_.size());
    const float emerged = std::clamp(growth, 0.0f, 1.0f) * static_cast<float>(count);

    // 変形中に「毎フレーム触る状態」を位置だけに絞る。
    // まだ生えていない球は胴（黒い球）の中に置いておけば、フラグを触らなくても見えない
    for (int joint = 0; joint < count; ++joint) {
        BossSphere *sphere = chain_[static_cast<size_t>(joint)].sphere;

        const float appear = std::clamp(emerged - static_cast<float>(joint), 0.0f, 1.0f);

        // 生えきる前は描かない。
        //
        // 以前は「胴の中へ置いておけば見えない」としていたが、変形の始めのコアは
        // 球体形態のコアの大きさを引き継いでいて小さく、脚の球のほうが大きいことがある。
        // そのときは胴からはみ出して、コアに球が固まって刺さったように見えていた
        const bool isGrowing = growth < 1.0f;
        if (appear <= 0.0f) {
            sphere->SetIsModelDraw(false);
            sphere->SetLocalPosition(bodyPosition);
            continue;
        }
        // 出しに戻すのは生えかけかどうかに関わらず行う。
        // 生えきる直前で隠したまま Active へ移ると、その球だけ出てこなくなる
        sphere->SetIsModelDraw(true);
        if (isGrowing) {
            // 生えかけは小さく。まるごと0にすると行列が潰れて法線が壊れるので、
            // ごく小さい値で止めてから膨らませる
            sphere->SetSphereRadius(params.legSphereRadius * (std::max)(0.05f, appear));
        }

        // 胴の中から自分の位置へ出てくる。appear=0 のときは胴の中と一致するので、
        // 出はじめに飛ぶことがない
        const Vector3 slotPosition = PointAlongLeg(static_cast<float>(joint), hip, knee, foot);
        sphere->SetLocalPosition(Lerp(bodyPosition, slotPosition, appear));
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
    // 下腿はくっついた球のぶんだけ詰まる。
    //
    // 目盛りは extension_ で滑らかに動かすが、それだけを基準にすると危ない。
    // 切り落としで連なりが一気に短くなると extension_ が大きな負の値になり、
    // 目盛りが下限（0.5）に張り付いて、残った球が膝と足を結ぶ線のはるか先へ
    // 飛ばされる（別の脚が枝分かれして生えたように見える）。
    // いま実際に下腿へ並んでいる数を下限にしておけば、そこまで縮まない
    const float actualSpan = static_cast<float>(chain_.size() - 1) - upperSpan;
    const float smoothedSpan = static_cast<float>(lowerSphereCount_ - 1) + extension_;
    const float lowerSpan = (std::max)(0.5f, (std::max)(actualSpan, smoothedSpan));

    // 節の外へは出さない。伸び縮みの途中で目盛りが追いつかなくても、
    // 球が足より先へはみ出すことはなくなる
    const float ratio = std::clamp((index - upperSpan) / lowerSpan, 0.0f, 1.0f);
    return Lerp(knee, foot, ratio);
}

void BossSpiderLeg::Draw(const ViewProjection &viewProjection) {
    if (isHidden_) {
        return;
    }
    for (const ChainSlot &slot : chain_) {
        slot.sphere->Draw(viewProjection);
    }
    for (const VanishSlot &slot : vanishing_) {
        slot.sphere->Draw(viewProjection);
    }
    // 切り落とされて飛んでいる球（脚から外れても、消えるまでは見えている）
    for (const SeveredPiece &piece : severed_) {
        piece.sphere->Draw(viewProjection);
    }
}

void BossSpiderLeg::SeverAll(const BossSpiderParams &params) {
    // 残っているぶんを根元から順に飛ばす。先の球ほど勢いよく飛ぶのは通常の切断と同じ
    for (int index = 0; index < static_cast<int>(chain_.size()); ++index) {
        BeginSever(chain_[static_cast<size_t>(index)], index, params);
    }
    chain_.clear();
    lastAttachIndex_ = -1;
}
