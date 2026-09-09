#include "FieldParticles.h"
#include <string>
#include "Particle/gpu/ParticleCSEmitter.h"
#include "Particle/gpu/ParticleCSSpawner.h"
#include <debug/log/Logger.h>

using namespace Hagine;

namespace {

/// <summary>Assets/jsons/ParticleCS/&lt;これ&gt;.json</summary>
constexpr const char *kTemplateName = "Field_Particle";

} // namespace

void FieldParticles::Spawn() {
    ParticleCSEmitter *emitter = ParticleCSSpawner::GetInstance()->Spawn(kTemplateName);
    if (!emitter) {
        // 黙って粒が出ないだけになるので、気づけるように残しておく
        Logger::Error("FieldParticles: Assets/jsons/ParticleCS/" + std::string(kTemplateName) +
                      ".json を読み込めなかった");
        return;
    }

    // Spawn 直後は "<名前>_1" のような複製名になっている。
    // このままだと調整UIからの保存が別ファイルへ行ってしまうので、テンプレート名へ戻す
    emitter->SetName(kTemplateName);

    // 発生範囲のワイヤーはゲーム画面に要らない（毎フレーム線で描かれてしまう）
    emitter->SetVisible(false);
}
