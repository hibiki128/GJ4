#include "BossParticles.h"
#include "Particle/gpu/ParticleCSEmitter.h"
#include "Particle/gpu/ParticleCSSpawner.h"
#include "data/DataHandler.h"
#include "frame/Frame.h"
#include <cmath>
#include <numbers>
#include <algorithm>
#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

using namespace Hagine;

namespace {

/// <summary>頭上の輪の置き方を書き出す先（Assets/jsons/Boss/&lt;これ&gt;.json）</summary>
constexpr const char *kRingLayoutFile = "StaggerRing";

/// <summary>効果の作り方の表</summary>
struct EffectDesc {
    BossParticles::Id id;
    const char *templateName; // Assets/jsons/ParticleCS/<これ>.json
    const char *label;        // 調整UIでの表示名
    int instanceCount;        // 同じフレームに何回まで重ねられるか
};

/// 脚は8本あって同じフレームに複数が着くので、脚の砂ぼこりだけ多めに持つ
constexpr EffectDesc kEffectDescs[] = {
    {BossParticles::Id::SlamDust, "Boss_SlamDust", "球体: 落下の着弾", 1},
    {BossParticles::Id::DashTrail, "Boss_DashTrail", "球体: 突進の土煙", 1},
    {BossParticles::Id::JumpDust, "Spider_JumpDust", "蜘蛛: 踏み切り", 1},
    {BossParticles::Id::LandDust, "Spider_LandDust", "蜘蛛: 着地", 1},
    {BossParticles::Id::StepDust, "Spider_StepDust", "蜘蛛: 脚の砂ぼこり", 4},
    {BossParticles::Id::DefeatBurst, "Spider_DefeatBurst", "蜘蛛: 撃破の破片", 1},
    // 輪は3体で1/3周ずつ受け持つ。1体だと粒が弧にしか並ばず、輪がつながらない
    {BossParticles::Id::StaggerRing, "Boss_StaggerRing", "ひるみ: 頭上を回る輪", 3},
};

/// <summary>テンプレートから1体出して、ボス用の使い方に合わせる</summary>
/// <param name="templateName">テンプレート名</param>
/// <returns>ParticleCSEmitter*: 出せなければ nullptr</returns>
ParticleCSEmitter *SpawnOne(const char *templateName) {
    ParticleCSEmitter *emitter = ParticleCSSpawner::GetInstance()->Spawn(templateName);
    if (!emitter) {
        return nullptr;
    }
    emitter->SetAuto(false);     // 出したいときにこちらから1回ずつ出す
    emitter->SetVisible(false);  // 発生範囲のワイヤーはゲーム画面に要らない
    // Spawn 直後は "<名前>_1" のような複製名になっている。
    // このままだと調整UIの保存が別ファイルへ行ってしまうので、テンプレート名へ戻す
    emitter->SetName(templateName);
    return emitter;
}

} // namespace

BossParticles *BossParticles::GetInstance() {
    static BossParticles instance;
    return &instance;
}

void BossParticles::Init() {
    ParticleCSSpawner *spawner = ParticleCSSpawner::GetInstance();

    for (const EffectDesc &desc : kEffectDescs) {
        Effect &effect = Get(desc.id);
        effect.templateName = desc.templateName;
        effect.label = desc.label;

        // シーンを切り替えると ParticleCSSpawner が実体を捨てるので、
        // 死んだぶんだけ落として足りない数を出し直す
        std::erase_if(effect.emitters,
                      [spawner](const ParticleCSEmitter *emitter) { return !spawner->IsAlive(emitter); });

        while (static_cast<int>(effect.emitters.size()) < desc.instanceCount) {
            ParticleCSEmitter *emitter = SpawnOne(desc.templateName);
            if (!emitter) {
                break; // json が無い。ここで諦めても他の効果には影響しない
            }
            effect.emitters.push_back(emitter);
        }
        // 倍率を掛け直す基準は「json に書いてある値」。出し直すたびに取り直す。
        // 発生範囲はエミッターから読めるが、飛び散る速さは読み出す口が無いので
        // 同じ json をこちらでも開いて基準を取る（書き込みはしない）
        effect.baseScales.clear();
        for (const ParticleCSEmitter *emitter : effect.emitters) {
            effect.baseScales.push_back(emitter->GetScale());
        }
        DataHandler source("ParticleCS", desc.templateName);
        effect.baseVelocityMin = source.Load<Vector3>("group_0_minVelocity", Vector3{});
        effect.baseVelocityMax = source.Load<Vector3>("group_0_maxVelocity", Vector3{});
        effect.next = 0;
    }

    LoadRingLayout();
    ApplyMasterScaleToEmitters();
}

void BossParticles::SetMasterScale(float scale) {
    masterScale_ = (std::max)(0.01f, scale);
    ApplyMasterScaleToEmitters();
}

void BossParticles::ApplyMasterScaleToEmitters() {
    ParticleCSSpawner *spawner = ParticleCSSpawner::GetInstance();
    for (Effect &effect : effects_) {
        for (size_t index = 0; index < effect.emitters.size() && index < effect.baseScales.size();
             ++index) {
            ParticleCSEmitter *emitter = effect.emitters[index];
            if (!emitter || !spawner->IsAlive(emitter)) {
                continue;
            }
            // 発生範囲だけ広げても粒がその場に固まるので、飛び散る速さも一緒に掛ける。
            // ボスが2倍なら土煙も2倍の範囲へ広がる
            emitter->SetScale(effect.baseScales[index] * masterScale_);
            emitter->SetMinVelocity(effect.baseVelocityMin * masterScale_);
            emitter->SetMaxVelocity(effect.baseVelocityMax * masterScale_);
        }
    }
}

void BossParticles::LoadRingLayout() {
    DataHandler data("Boss", kRingLayoutFile);
    ring_.radius = data.Load<float>("radius", ring_.radius);
    ring_.height = data.Load<float>("height", ring_.height);
    ring_.spinSpeed = data.Load<float>("spinSpeed", ring_.spinSpeed);
    ring_.emitInterval = data.Load<float>("emitInterval", ring_.emitInterval);
}

void BossParticles::UpdateStaggerRing(const Vector3 &headCenter, float deltaTime) {
    Effect &effect = Get(Id::StaggerRing);
    if (effect.emitters.empty()) {
        return;
    }

    constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;
    ringAngle_ += ring_.spinSpeed * kDegToRad * deltaTime;

    // 粒は置いた場所に留まるので、置く位置を円周に沿って進めるだけで輪が回って見える。
    // 毎フレーム置くと濃くなりすぎるので間隔をあける
    ringEmitTimer_ += deltaTime;
    const float interval = (std::max)(0.005f, ring_.emitInterval);
    if (ringEmitTimer_ < interval) {
        return;
    }
    ringEmitTimer_ = 0.0f;

    ParticleCSSpawner *spawner = ParticleCSSpawner::GetInstance();
    const size_t count = effect.emitters.size();
    for (size_t index = 0; index < count; ++index) {
        ParticleCSEmitter *emitter = effect.emitters[index];
        if (!emitter || !spawner->IsAlive(emitter)) {
            continue;
        }
        // 体数で円周を等分して受け持つ。1体だと弧にしかならず、輪がつながらない
        const float angle = ringAngle_ + std::numbers::pi_v<float> * 2.0f *
                                             static_cast<float>(index) / static_cast<float>(count);
        // 輪の大きさはボスに合わせる。json には倍率を掛ける前の値が入っている
        const float radius = ring_.radius * masterScale_;
        const float height = ring_.height * masterScale_;
        emitter->SetTranslate(Vector3{headCenter.x + std::cos(angle) * radius,
                                      headCenter.y + height,
                                      headCenter.z + std::sin(angle) * radius});
        emitter->EmitOnce();
    }
}

void BossParticles::StopStaggerRing() {
    // 次に回し始めたとき、待たされずに1周目が出るようにそろえておく
    ringEmitTimer_ = 0.0f;
}

void BossParticles::Burst(Id id, const Vector3 &position) {
    if (id == Id::Count) {
        return;
    }
    Effect &effect = Get(id);
    if (effect.emitters.empty()) {
        return;
    }

    ParticleCSEmitter *emitter = effect.emitters[effect.next];
    effect.next = (effect.next + 1) % effect.emitters.size();
    if (!ParticleCSSpawner::GetInstance()->IsAlive(emitter)) {
        return;
    }

    emitter->SetTranslate(position);
    emitter->EmitOnce();
}

void BossParticles::BurstOnGround(Id id, const Vector3 &point) {
    // 地面ちょうどだと粒の下半分が床に隠れるので、少しだけ浮かせる
    constexpr float kGroundHeight = 0.3f;
    Burst(id, Vector3{point.x, kGroundHeight, point.z});
}

void BossParticles::Reload(Id id) {
    Effect &effect = Get(id);
    if (effect.templateName[0] == '\0') {
        return;
    }

    ParticleCSSpawner *spawner = ParticleCSSpawner::GetInstance();
    const size_t count = effect.emitters.size();
    for (ParticleCSEmitter *emitter : effect.emitters) {
        spawner->Despawn(emitter);
    }
    effect.emitters.clear();
    effect.next = 0;

    for (size_t i = 0; i < count; ++i) {
        ParticleCSEmitter *emitter = SpawnOne(effect.templateName);
        if (!emitter) {
            break;
        }
        effect.emitters.push_back(emitter);
    }
}

void BossParticles::DrawImGui() {
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader("ボスの土煙##BossParticles")) {
        return;
    }
    ImGui::Indent();
    ImGui::TextWrapped("見た目を触って「GPU設定を保存」を押すと Assets/jsons/ParticleCS/<名前>.json に書き戻る。"
                       "次に出したときからその値になる。");

    ImGui::SeparatorText("ひるみの輪の置き方");
    ImGui::TextDisabled("粒そのものの見た目は下の「ひるみ: 頭上を回る輪」で調整します");
    ImGui::DragFloat("輪の半径", &ring_.radius, 0.05f, 0.1f, 20.0f);
    ImGui::DragFloat("頭からの高さ", &ring_.height, 0.05f, -5.0f, 20.0f);
    ImGui::DragFloat("回る速さ(度/秒)", &ring_.spinSpeed, 5.0f, -1440.0f, 1440.0f);
    ImGui::DragFloat("粒を置く間隔(秒)", &ring_.emitInterval, 0.002f, 0.005f, 0.5f);
    if (ImGui::Button("輪の置き方を保存")) {
        DataHandler data("Boss", kRingLayoutFile);
        data.Save("radius", ring_.radius);
        data.Save("height", ring_.height);
        data.Save("spinSpeed", ring_.spinSpeed);
        data.Save("emitInterval", ring_.emitInterval);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("保存先: Assets/jsons/Boss/%s.json", kRingLayoutFile);
    ImGui::Checkbox("試しに回す（下の位置で）", &ringPreview_);
    if (ringPreview_) {
        UpdateStaggerRing(testPosition_, Frame::DeltaTime());
    }

    ImGui::SeparatorText("粒の見た目");
    for (size_t index = 0; index < effects_.size(); ++index) {
        Effect &effect = effects_[index];
        if (effect.emitters.empty()) {
            ImGui::TextDisabled("%s: 出せていない（json を確認）", effect.label);
            continue;
        }

        ImGui::PushID(static_cast<int>(index));
        if (ImGui::TreeNode(effect.label)) {
            if (ImGui::Button("ここで出す")) {
                Burst(static_cast<Id>(index), testPosition_);
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(200.0f);
            ImGui::DragFloat3("試し撃ちの位置", &testPosition_.x, 0.1f);

            if (effect.emitters.size() > 1) {
                // 編集できるのは1体目だけなので、保存した値を残りへ行き渡らせる手段を用意しておく
                if (ImGui::Button("保存した設定を全部に反映")) {
                    Reload(static_cast<Id>(index));
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(%zu体で使い回し中)", effect.emitters.size());
            }

            if (!effect.emitters.empty()) {
                effect.emitters[0]->DrawImGui();
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    ImGui::Unindent();
#endif // USE_IMGUI
}
