#pragma once
#include "src/Boss/Data/BossColorPalette.h"
#include "src/Boss/Data/BossParameters.h"
#include "src/Boss/Shell/BossShellMetaBall.h"
#include "debug/param/GameParamHub.h"
#include "object/base/BaseObject.h"
#include <string>
#include <vector>

/// <summary>
/// クリア画面に転がっている、倒したあとのボス。
///
/// このオブジェクト自身が「剥き出しになったコア」で、周りには纏っていた殻の
/// 破片が転がっている。破片は Boss と同じ BossShellMetaBall で描くので、
/// 見た目（融合した水滴のような塊・脈動）はゲーム中の殻とそろう。
///
/// Boss クラスは攻撃・状態遷移・被弾判定まで抱えていて、クリア画面では
/// どれも要らない。ここは「コアが転がっていて、周りに殻が散っている」という
/// 絵だけを作る演出用のクラスにしてある。
///
/// 殻の破片はコアの子ではなくワールド空間に直接置く。子にすると、コアが
/// ぐらぐら揺れるのに合わせて地面の破片まで一緒に回ってしまうため。
/// </summary>
class ClearBossRemains final : public Hagine::BaseObject {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>コアを作り、周りへ殻の破片を撒く</summary>
    /// <param name="objectName">オブジェクト名</param>
    void Init(const std::string objectName) override;

    /// <summary>コアの揺れを進める</summary>
    void Update() override;

    /// <summary>調整パラメータをデバッグUIへ登録する（Init の後に一度だけ）</summary>
    void RegisterParams();

    /// <summary>
    /// 殻の破片の形をGPUで作り直す。
    /// DrawSystem のコンピュートフェーズ（kGPUParticleCompute）から呼ぶこと
    /// </summary>
    void DispatchShellCompute();

    /// <summary>殻の破片を描く（コア自身は BaseObject の描画に任せる）</summary>
    /// <param name="viewProjection">ビュープロジェクション</param>
    void DrawDebris(const Hagine::ViewProjection &viewProjection);

    /// <summary>転がっている場所（地面の座標）を決める</summary>
    void SetGroundPosition(const Hagine::Vector3 &position) { groundPosition_ = position; }

    /// <summary>転がっている場所を取得する</summary>
    const Hagine::Vector3 &GetGroundPosition() const { return groundPosition_; }

    /// <summary>コアの半径（カメラの寄せ具合を決めるのに使う）</summary>
    float GetCoreRadius() const { return coreRadius_; }

private:
    /// ===================================================
    /// private method
    /// ===================================================

    /// <summary>殻の破片を撒き直す（散らばり方のパラメータを変えたときに呼ぶ）</summary>
    void ScatterDebris();

    /// <summary>コアの大きさをスケールへ反映する</summary>
    void ApplyCoreSize();

    /// ===================================================
    /// private variables
    /// ===================================================

    BossColorPalette palette_{};       // 色マスタ（ボスと共通）
    BossMetaBallParams metaBallParams_{}; // 殻の見た目（ボスデータから読む）
    BossShellMetaBall debris_{};       // 散らばった殻（色ごとに融合したメッシュ）

    Hagine::Vector3 groundPosition_ = {0.0f, 0.0f, 0.0f}; // 転がっている場所（地面）
    float rockTime_ = 0.0f;                               // 揺れの経過時間（秒）

    // --- 調整パラメータ ---
    float coreRadius_ = 1.5f;       // コアの半径
    float debrisSphereRadius_ = 0.5f; // 破片1粒の半径（ゲーム中の殻の球と同じ大きさ）
    int clumpCount_ = 10;           // 破片の塊の数
    int spheresPerClump_ = 5;       // 1つの塊に入れる粒の数
    float scatterInner_ = 2.2f;     // 塊を置く輪の内側の半径
    float scatterOuter_ = 6.0f;     // 塊を置く輪の外側の半径
    float clumpSpread_ = 0.55f;     // 塊の中で粒がばらける広さ
    int scatterSeed_ = 20260909;    // 散らばり方のシード（変えると撒き直す）
    float rockAmplitude_ = 4.0f;    // コアの揺れの大きさ（度）
    float rockPeriod_ = 2.6f;       // コアの揺れの周期（秒）
    float spinSpeed_ = 6.0f;        // コアがゆっくり回る速さ（度/秒）

    int builtSeed_ = 0;             // いま撒いてある破片のシード（変わったら撒き直す）

    // 登録した調整パラメータは破棄時にまとめて解除する
    Hagine::GameParamOwner params_{"Clear/BossRemains"};
};
