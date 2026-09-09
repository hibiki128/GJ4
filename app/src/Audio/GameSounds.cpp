#include "GameSounds.h"
#include "audio/Audio.h"
#include "data/DataHandler.h"
#include <algorithm>
#include <string>
#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

using namespace Hagine;

namespace {

/// <summary>音量と間隔の保存先（Assets/jsons/Boss/&lt;これ&gt;.json）</summary>
constexpr const char *kSettingsFile = "Sounds";

/// <summary>音ひとつぶんの作り方</summary>
struct SoundDesc {
    GameSounds::Id id;
    const char *path;    // sounds ルートからの相対パス
    const char *label;   // 調整UIでの表示名
    float volume;        // 既定の音量
    float minInterval;   // 鳴らし直しの最短間隔（秒）
};

// 間隔は「その音が重なって濁らない程度」を目安に置いてある。
// 連鎖の消滅は同じフレームに何度も起きるので、いちばん長めに取る
constexpr SoundDesc kSoundDescs[] = {
    {GameSounds::Id::Break, "SE/boss/break.wav", "球が消える", 0.55f, 0.25f},
    {GameSounds::Id::Landing, "SE/boss/landing.wav", "着地（球体の飛び込み・蜘蛛の跳躍）", 0.70f, 0.08f},
    {GameSounds::Id::Shot, "SE/boss/langed.wav", "弾を撃つ", 0.45f, 0.05f},
    {GameSounds::Id::RotateSphere, "SE/boss/rotate.wav", "球体: 回転突進（回転中）", 0.40f, 0.0f},
    {GameSounds::Id::RotateSpider, "SE/boss/rotate_spider.wav", "蜘蛛: 回転（回転中）", 0.40f, 0.0f},
    {GameSounds::Id::Stun, "SE/boss/stun.wav", "ひるみ中", 0.35f, 0.0f},
    {GameSounds::Id::Appear, "SE/other/enter.wav", "第2形態の登場", 0.60f, 0.0f},
    {GameSounds::Id::PlayerDamaged, "SE/player/damaged.wav", "プレイヤーの被弾", 0.60f, 0.15f},
    // プレイヤーの音は鳴る回数が桁違いに多い（歩き・射撃は数秒に何度も鳴る）ので、
    // ボスの音と同じ音量にすると戦闘の音が全部それに埋もれる。かなり絞ってある
    {GameSounds::Id::PlayerDodge, "SE/player/Slime_Dodge.wav", "プレイヤーの回避", 0.30f, 0.05f},
    {GameSounds::Id::PlayerIdle1, "SE/player/Slime_Idle1.wav", "プレイヤーのぽよぽよ1", 0.16f, 0.3f},
    {GameSounds::Id::PlayerIdle2, "SE/player/Slime_Idle2.wav", "プレイヤーのぽよぽよ2", 0.16f, 0.3f},
    {GameSounds::Id::PlayerMove, "SE/player/Slime_Move.wav", "プレイヤーの足音", 0.20f, 0.1f},
    {GameSounds::Id::PlayerFire, "SE/player/Slime_Fire.wav", "プレイヤーの射撃", 0.24f, 0.05f},
    {GameSounds::Id::Bgm, "BGM/gameScene.wav", "戦闘中のBGM", 0.25f, 0.0f},
};

} // namespace

GameSounds *GameSounds::GetInstance() {
    static GameSounds instance;
    return &instance;
}

void GameSounds::Init() {
    if (isReady_) {
        // wav はシーンをまたいで持ち回れるので、読み直さない。
        // 前のシーンで鳴らしっぱなしのものだけ止めておく
        StopAll();
        return;
    }

    Audio *audio = Audio::GetInstance();
    for (const SoundDesc &desc : kSoundDescs) {
        Sound &sound = Get(desc.id);
        sound.path = desc.path;
        sound.label = desc.label;
        sound.volume = desc.volume;
        sound.minInterval = desc.minInterval;
        sound.cooldown = 0.0f;
        sound.isLooping = false;
        sound.handle = audio->LoadWave(desc.path);
    }

    LoadSettings();
    isReady_ = true;
}

void GameSounds::Play(Id id) {
    if (id == Id::Count) {
        return;
    }
    Sound &sound = Get(id);
    if (sound.handle == UINT32_MAX) {
        return; // 読めていない。鳴らないだけでゲームは止めない
    }
    // 間隔が明けていなければ鳴らさない。
    // 同じ音を重ねて鳴らすと、音量が足し合わさって割れて聞こえる
    if (sound.cooldown > 0.0f) {
        return;
    }
    sound.cooldown = sound.minInterval;
    Audio::GetInstance()->PlayWave(sound.handle, sound.volume, false);
}

void GameSounds::StartLoop(Id id) {
    if (id == Id::Count) {
        return;
    }
    Sound &sound = Get(id);
    if (sound.handle == UINT32_MAX || sound.isLooping) {
        return;
    }
    sound.isLooping = true;
    Audio::GetInstance()->PlayWave(sound.handle, sound.volume, true);
}

void GameSounds::StopLoop(Id id) {
    if (id == Id::Count) {
        return;
    }
    Sound &sound = Get(id);
    if (sound.handle == UINT32_MAX || !sound.isLooping) {
        return;
    }
    sound.isLooping = false;
    Audio::GetInstance()->StopWave(sound.handle);
}

void GameSounds::StopAll() {
    Audio *audio = Audio::GetInstance();
    for (Sound &sound : sounds_) {
        // 止めるのは鳴らし続けている音だけにする。
        // StopWave は消す前に DestroyVoice を呼ぶが、鳴り終わったボイスは
        // すでに sourceVoice が nullptr にされていて DestroyVoice を飛ばすので、
        // XAudio2が掴んだままのコールバックを解放してしまう。
        // ループ再生は終わりが来ない（OnBufferEnd が呼ばれない）ので、これは安全
        if (sound.handle == UINT32_MAX || !sound.isLooping) {
            sound.cooldown = 0.0f;
            continue;
        }
        audio->StopWave(sound.handle);
        sound.isLooping = false;
        sound.cooldown = 0.0f;
    }
}

void GameSounds::Update(float deltaTime) {
    for (Sound &sound : sounds_) {
        if (sound.cooldown > 0.0f) {
            sound.cooldown = (std::max)(0.0f, sound.cooldown - deltaTime);
        }
    }

    // ここで Audio::CleanupFinishedVoices() を呼んではいけない。
    //
    // 鳴り終わりの合図（OnBufferEnd）はXAudio2のオーディオスレッドから来て、
    // sourceVoice を nullptr にするだけで DestroyVoice を呼んでいない。
    // つまりXAudio2側はまだそのボイスと、Voice の中にあるコールバックを掴んでいる。
    // CleanupFinishedVoices はその「nullptr になったもの」を消してしまうので、
    // XAudio2が握ったままのコールバックが解放され、次に触られた瞬間に落ちる。
    // 単発の音を鳴らすたびに踏むので、鳴らし終わったボイスは片付けずに残しておく
}

void GameSounds::LoadSettings() {
    DataHandler data("Boss", kSettingsFile);
    for (const SoundDesc &desc : kSoundDescs) {
        Sound &sound = Get(desc.id);
        const std::string key = desc.path;
        sound.volume = data.Load<float>(key + "_volume", sound.volume);
        sound.minInterval = data.Load<float>(key + "_minInterval", sound.minInterval);
    }
}

void GameSounds::SaveSettings() {
    DataHandler data("Boss", kSettingsFile);

    // 差し替えた wav のキー。読む相手がいなくなっているので、保存のついでに掃除しておく
    // （回避の音は SE/player/slime.wav から Slime_Dodge.wav へ差し替えた）
    for (const char *legacyKey : {"SE/player/slime.wav_volume", "SE/player/slime.wav_minInterval"}) {
        data.Remove(legacyKey);
    }

    for (const SoundDesc &desc : kSoundDescs) {
        const Sound &sound = Get(desc.id);
        const std::string key = desc.path;
        data.Save(key + "_volume", sound.volume);
        data.Save(key + "_minInterval", sound.minInterval);
    }
}

void GameSounds::DrawImGui() {
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader("音")) {
        return;
    }
    ImGui::Indent();
    ImGui::TextWrapped("「鳴らし直しの間隔」は、同じ音が重なって濁らないようにするためのものです。"
                       "その音の長さくらいまで伸ばすと重なりません（0で毎回鳴らす）。");

    for (size_t index = 0; index < sounds_.size(); ++index) {
        Sound &sound = sounds_[index];
        ImGui::PushID(static_cast<int>(index));

        if (sound.handle == UINT32_MAX) {
            ImGui::TextColored(ImVec4{1.0f, 0.4f, 0.3f, 1.0f}, "%s: 読めていない（%s）", sound.label,
                               sound.path);
            ImGui::PopID();
            continue;
        }

        ImGui::Text("%s", sound.label);
        if (sound.isLooping) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4{0.4f, 1.0f, 0.6f, 1.0f}, "（鳴らし続け中）");
        }
        if (ImGui::SliderFloat("音量", &sound.volume, 0.0f, 1.0f)) {
            // 鳴っている音へその場で反映する（次に鳴らすときからではなく）
            Audio::GetInstance()->SetVolume(sound.handle, sound.volume);
        }
        ImGui::DragFloat("鳴らし直しの間隔(秒)", &sound.minInterval, 0.01f, 0.0f, 5.0f);
        if (ImGui::Button("試しに鳴らす")) {
            sound.cooldown = 0.0f;
            Audio::GetInstance()->PlayWave(sound.handle, sound.volume, false);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s", sound.path);
        ImGui::Separator();
        ImGui::PopID();
    }

    if (ImGui::Button("この値を保存")) {
        SaveSettings();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("保存先: Assets/jsons/Boss/%s.json", kSettingsFile);
    ImGui::Unindent();
#endif // USE_IMGUI
}
