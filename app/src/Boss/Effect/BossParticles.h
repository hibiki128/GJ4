#pragma once
#include "type/Vector3.h"
#include <array>
#include <vector>

namespace Hagine {
class ParticleCSEmitter;
}

/// <summary>
/// ボスまわりの土煙や破片をまとめて受け持つ。
///
/// 攻撃クラスからも脚からも同じ入口で呼びたいので、ここだけシングルトンにしている
/// （見た目だけの機能で、ゲーム進行の状態は持たない）。
///
/// 中身はエンジンのGPUパーティクル（ParticleCSSpawner / ParticleCSEmitter）そのままで、
/// 効果ひとつが Assets/jsons/ParticleCS/&lt;名前&gt;.json 1枚に対応する。
/// 見た目の調整と保存はエンジンのエミッター編集UIをそのまま出しているので、
/// 「パーティクル設定」ウィンドウから触って保存すれば json に書き戻る。
///
/// 実体の所有は ParticleCSSpawner 側。シーンを切り替えると捨てられるため、
/// シーンの初期化のたびに Init() を呼ぶこと（生きているぶんは出し直さない）。
/// </summary>
class BossParticles {
public:
    /// <summary>出せる効果</summary>
    enum class Id {
        SlamDust,    // 球体形態: プレイヤーへ落ちてきた着弾の土煙
        DashTrail,   // 球体形態: 突進中に引きずる土煙
        JumpDust,    // 蜘蛛: 踏み切りで蹴り上げる土煙
        LandDust,    // 蜘蛛: 着地で横へ広がる土煙
        StepDust,    // 蜘蛛: 脚を踏み下ろしたときの小さな砂ぼこり
        DefeatBurst, // 蜘蛛: 撃破でコアがはじけた破片
        Count
    };

    /// ===================================================
    /// public method
    /// ===================================================

    static BossParticles *GetInstance();

    /// <summary>テンプレートを読み込んでシーンへエミッターを出す（シーンの初期化から毎回呼ぶ）</summary>
    void Init();

    /// <summary>指定の位置で1回出す</summary>
    /// <param name="id">効果</param>
    /// <param name="position">出す位置（ワールド）</param>
    void Burst(Id id, const Hagine::Vector3 &position);

    /// <summary>地面で1回出す（高さだけ地面すれすれに置き換える）</summary>
    /// <param name="id">効果</param>
    /// <param name="point">出す位置。y は使わない</param>
    void BurstOnGround(Id id, const Hagine::Vector3 &point);

    /// <summary>調整UI（エンジンのエミッター編集をそのまま出す）</summary>
    void DrawImGui();

private:
    /// ===================================================
    /// private method
    /// ===================================================

    BossParticles() = default;
    ~BossParticles() = default;
    BossParticles(const BossParticles &) = delete;
    BossParticles &operator=(const BossParticles &) = delete;

    /// <summary>効果ひとつぶんの持ち物</summary>
    struct Effect {
        // 同じ効果が同じフレームに重なっても取りこぼさないよう、必要な数だけ用意して順番に使う。
        // 実体は ParticleCSSpawner が持っているので、ここは参照するだけ
        std::vector<Hagine::ParticleCSEmitter *> emitters;
        size_t next = 0;              // 次に使うエミッター
        const char *templateName = ""; // json のファイル名
        const char *label = "";        // 調整UIでの表示名
    };

    /// <summary>効果を引く</summary>
    /// <param name="id">効果</param>
    /// <returns>Effect&amp;: 該当の持ち物</returns>
    Effect &Get(Id id) { return effects_[static_cast<size_t>(id)]; }

    /// <summary>json を読み直したいときに、その効果のエミッターを出し直す</summary>
    /// <param name="id">効果</param>
    void Reload(Id id);

    /// ===================================================
    /// private variables
    /// ===================================================

    std::array<Effect, static_cast<size_t>(Id::Count)> effects_{};
    Hagine::Vector3 testPosition_ = {0.0f, 0.0f, 0.0f}; // 調整UIの試し撃ち位置
};
