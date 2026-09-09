#pragma once
#include <array>
#include <cstdint>

/// <summary>
/// ゲーム中の音をまとめて受け持つ。
///
/// 攻撃クラスからも脚からもシーンからも同じ入口で鳴らしたいので、ここだけシングルトンにしている
/// （鳴らすだけの機能で、ゲーム進行の状態は持たない）。
///
/// 音量と「鳴らし直しの間隔」は Assets/jsons/Boss/Sounds.json に保存され、
/// 調整UIから触って保存できる。
///
/// エンジンの Audio は PlayWave を呼ぶたびに新しいボイスを作るので、
/// 短い間に何度も呼ぶと同じ音が重なって濁る。ここで間隔をあけて防いでいる。
/// 鳴らし続ける音（回転・ひるみ）はループ再生を1本だけ持ち、止めるときに StopWave する
/// </summary>
class GameSounds {
public:
    /// <summary>鳴らせる音</summary>
    enum class Id {
        Break,         // 連なった球がそろって消えた
        Landing,       // 地面へ着地した（球体の飛び込み・蜘蛛の跳躍で共通）
        Shot,          // 弾を撃った
        RotateSphere,  // 球体の回転突進（回っているあいだ鳴らし続ける）
        RotateSpider,  // 蜘蛛の回転（回っているあいだ鳴らし続ける）
        Stun,          // ひるみ中（鳴らし続ける）
        Appear,        // 第2形態が起き上がる
        PlayerDamaged, // プレイヤーが被弾した
        PlayerDodge,   // プレイヤーが回避した
        Bgm,           // 戦闘中のBGM
        Count
    };

    /// ===================================================
    /// public method
    /// ===================================================

    static GameSounds *GetInstance();

    /// <summary>wav を読み込む（シーンの初期化から毎回呼んでよい。2回目以降は読み直さない）</summary>
    void Init();

    /// <summary>1回鳴らす（間隔が明けていなければ鳴らさない）</summary>
    /// <param name="id">鳴らす音</param>
    void Play(Id id);

    /// <summary>鳴らし続ける（すでに鳴っていれば何もしない）</summary>
    /// <param name="id">鳴らす音</param>
    void StartLoop(Id id);

    /// <summary>鳴らし続けている音を止める（鳴っていなければ何もしない）</summary>
    /// <param name="id">止める音</param>
    void StopLoop(Id id);

    /// <summary>鳴っている音をすべて止める（シーンを抜けるとき）</summary>
    void StopAll();

    /// <summary>間隔の計測を進め、鳴り終わったボイスを片付ける（毎フレーム）</summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    void Update(float deltaTime);

    /// <summary>調整UI（音量と鳴らし直しの間隔）</summary>
    void DrawImGui();

private:
    /// ===================================================
    /// private method
    /// ===================================================

    GameSounds() = default;
    ~GameSounds() = default;
    GameSounds(const GameSounds &) = delete;
    GameSounds &operator=(const GameSounds &) = delete;

    /// <summary>音ひとつぶんの持ち物</summary>
    struct Sound {
        uint32_t handle = UINT32_MAX; // エンジンの音声インデックス（読めていなければ UINT32_MAX）
        float volume = 1.0f;          // 音量
        // 同じ音を鳴らし直すまでの最短の間隔（秒）。
        // 短い音が重なって濁るのを防ぐ。0 なら毎回鳴らす
        float minInterval = 0.0f;
        float cooldown = 0.0f;   // 次に鳴らせるまでの残り（秒）
        bool isLooping = false;  // ループ再生中か
        const char *path = "";   // sounds ルートからの相対パス
        const char *label = "";  // 調整UIでの表示名
    };

    /// <summary>音を引く</summary>
    /// <param name="id">音</param>
    /// <returns>Sound&amp;: 該当の持ち物</returns>
    Sound &Get(Id id) { return sounds_[static_cast<size_t>(id)]; }

    /// <summary>音量と間隔を json から読む</summary>
    void LoadSettings();

    /// <summary>音量と間隔を json へ書き出す</summary>
    void SaveSettings();

    /// ===================================================
    /// private variables
    /// ===================================================

    std::array<Sound, static_cast<size_t>(Id::Count)> sounds_{};
    bool isReady_ = false; // 読み込み済みか
};
