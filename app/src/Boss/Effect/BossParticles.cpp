#include "BossParticles.h"
#include "Particle/gpu/ParticleCSEmitter.h"
#include "Particle/gpu/ParticleCSSpawner.h"
#include <algorithm>
#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

using namespace Hagine;

namespace {

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
        effect.next = 0;
    }
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
