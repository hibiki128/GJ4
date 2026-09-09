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
        StaggerRing, // ひるみ中に頭上を回る粒（両形態で共通）
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

    /// <summary>
    /// ひるみ中に頭上を回る粒を進める。ひるんでいるあいだ毎フレーム呼ぶ。
    /// 粒は置いた場所に留まるので、置く位置を円周に沿って進めることで輪が回って見える
    /// </summary>
    /// <param name="headCenter">頭の中心（ワールド）。輪はこの上に出る</param>
    /// <param name="deltaTime">経過時間（秒）</param>
    void UpdateStaggerRing(const Hagine::Vector3 &headCenter, float deltaTime);

    /// <summary>輪を止める（次に回し始めるとき、すぐ1周目が出るようにそろえる）</summary>
    void StopStaggerRing();

    /// <summary>
    /// ボスの大きさに合わせて効果も大きくする。
    ///
    /// エミッターの発生範囲と、頭上の輪の置き方に掛かる。
    /// json の値には掛けず、読み込んだ値を基準に毎回掛け直すので二重にはならない
    /// （粒そのものの大きさや飛び方は ParticleCS の json 側で決まる）
    /// </summary>
    /// <param name="scale">倍率</param>
    void SetMasterScale(float scale);

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
        // json に書いてある発生範囲の大きさ。倍率はここから毎回掛け直す
        std::vector<Hagine::Vector3> baseScales;
        // json に書いてある飛び散る速さ。倍率はここから毎回掛け直す
        Hagine::Vector3 baseVelocityMin{};
        Hagine::Vector3 baseVelocityMax{};
        size_t next = 0;              // 次に使うエミッター
        const char *templateName = ""; // json のファイル名
        const char *label = "";        // 調整UIでの表示名
    };

    /// <summary>効果を引く</summary>
    /// <param name="id">効果</param>
    /// <returns>Effect&amp;: 該当の持ち物</returns>
    Effect &Get(Id id) { return effects_[static_cast<size_t>(id)]; }

    /// <summary>頭上を回る輪の置き方（見た目そのものはエミッターの json 側）</summary>
    struct RingLayout {
        float radius = 2.6f;         // 頭の中心から輪までの距離
        float height = 2.2f;         // 頭の中心から輪までの高さ
        float spinSpeed = 300.0f;    // 輪が回る速さ（度/秒）
        float emitInterval = 0.045f; // 粒を置く間隔（秒）。短いほど輪が濃くなる
    };

    /// <summary>輪の置き方を json から読む</summary>
    void LoadRingLayout();

    /// <summary>いまの倍率を、生きているエミッターの発生範囲へ掛け直す</summary>
    void ApplyMasterScaleToEmitters();

    /// <summary>json を読み直したいときに、その効果のエミッターを出し直す</summary>
    /// <param name="id">効果</param>
    void Reload(Id id);

    /// ===================================================
    /// private variables
    /// ===================================================

    std::array<Effect, static_cast<size_t>(Id::Count)> effects_{};
    Hagine::Vector3 testPosition_ = {0.0f, 0.0f, 0.0f}; // 調整UIの試し撃ち位置

    RingLayout ring_{};            // 頭上の輪の置き方
    float ringAngle_ = 0.0f;       // 輪がいまどこまで回ったか（ラジアン）
    float ringEmitTimer_ = 0.0f;   // 次に粒を置くまでの計測
    bool ringPreview_ = false;     // 調整UIで輪を回して見ているか
    float masterScale_ = 1.0f;     // ボスの大きさに合わせた倍率
};
