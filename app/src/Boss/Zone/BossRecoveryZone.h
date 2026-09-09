#pragma once
#include "src/Boss/Data/BossColorPalette.h"
#include "src/Boss/Data/BossParameters.h"
#include "src/Character/ColorStruct.h"
#include "object/base/BaseObject.h"
#include "type/Vector3.h"
#include <memory>
#include <string>

namespace Hagine {
class ViewProjection;
}

class IAmmoRecoverySink;
class ITargetLocator;

/// <summary>
/// 残弾を回復する円形のエリア1つぶん。
///
/// ボスがひと続きの攻撃を終えると、その場から弾けるように生まれてフィールドへ落ちる。
/// 乗っているあいだ、エリアの色の弾だけが早く戻る。
///
/// 見た目は警告表示と同じ2枚の円盤（外枠＋内側の塗り）を使い回している。
/// どちらも半径1のXZ平面なので、スケールをそのまま半径として扱える。
///
/// 円盤は BaseObject なので、実行中に作り直すと前フレームのGPUコマンドが
/// 掴んだままのリソースを解放して落ちる。使い終わっても破棄せず、
/// 隠して使い回す（生成はマネージャー側のプールが受け持つ）
/// </summary>
class BossRecoveryZone {
public:
    /// <summary>エリアの進み具合</summary>
    enum class Phase {
        Idle,   // 使われていない
        Pop,    // ボスから飛び出して着地するまで
        Open,   // 輪が開ききるまで
        Active, // 開いている（ここで回復する）
        Close   // 閉じて消えるまで
    };

    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>円盤を用意する（1回だけ）</summary>
    /// <param name="namePrefix">オブジェクト名の接頭辞</param>
    void Init(const std::string &namePrefix);

    /// <summary>
    /// 出現させる
    /// </summary>
    /// <param name="from">飛び出す元（ボスの位置）</param>
    /// <param name="landing">落ちる先（地面の高さ）</param>
    /// <param name="color">エリアの色</param>
    /// <param name="palette">色の見た目を引くパレット</param>
    /// <param name="params">調整値</param>
    void Spawn(const Hagine::Vector3 &from, const Hagine::Vector3 &landing, Color color,
               const BossColorPalette &palette, const BossRecoveryZoneParams &params);

    /// <summary>
    /// 1フレーム進める
    /// </summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    /// <param name="params">調整値</param>
    /// <param name="target">乗っているか調べる相手（未接続可）</param>
    /// <param name="sink">回復を要求する先（未接続可）</param>
    void Update(float deltaTime, const BossRecoveryZoneParams &params, const ITargetLocator *target,
                IAmmoRecoverySink *sink);

    /// <summary>描画する（円盤はオブジェクトマネージャーに載せていないので自分で描く）</summary>
    /// <param name="viewProjection">ビュープロジェクション</param>
    void Draw(const Hagine::ViewProjection &viewProjection);

    /// <summary>閉じ始める（役目を終えたとき）</summary>
    /// <param name="params">調整値</param>
    void BeginClose(const BossRecoveryZoneParams &params);

    /// <summary>すぐに片付ける（シーンのやり直しなど）</summary>
    void Clear();

    /// ===================================================
    /// getter
    /// ===================================================

    bool IsAlive() const { return phase_ != Phase::Idle; }
    bool IsClosing() const { return phase_ == Phase::Close; }
    Color GetColor() const { return color_; }
    const Hagine::Vector3 &GetCenter() const { return center_; }
    /// <summary>いま相手が乗っているか（表示の確認用）</summary>
    bool IsOccupied() const { return isOccupied_; }
    /// <summary>生まれてからの経過時間（秒）。古い順に畳むのに使う</summary>
    float GetAge() const { return age_; }
    const char *GetPhaseName() const;

private:
    /// ===================================================
    /// private method
    /// ===================================================

    /// <summary>円盤の位置と大きさを反映する</summary>
    /// <param name="openRatio">輪の開き具合（0〜1）</param>
    /// <param name="params">調整値</param>
    void Place(float openRatio, const BossRecoveryZoneParams &params);

    /// <summary>立ちのぼる粒を置く</summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    /// <param name="params">調整値</param>
    void UpdateAura(float deltaTime, const BossRecoveryZoneParams &params);

    /// <summary>円盤を出す・隠す</summary>
    void SetVisible(bool visible);

    /// ===================================================
    /// private variables
    /// ===================================================

    std::unique_ptr<Hagine::BaseObject> ring_{}; // 外枠（範囲そのもの）
    std::unique_ptr<Hagine::BaseObject> fill_{}; // 内側の塗り（脈打つ）

    Phase phase_ = Phase::Idle;
    Color color_ = Color::RED;
    Hagine::Vector4 rgba_{1.0f, 1.0f, 1.0f, 1.0f}; // エリアの色

    Hagine::Vector3 spawnFrom_{}; // 飛び出した元
    Hagine::Vector3 center_{};    // 落ちた先（エリアの中心）

    float timer_ = 0.0f;      // いまの段階の経過時間（秒）
    float age_ = 0.0f;        // 生まれてからの経過時間（秒）
    float pulsePhase_ = 0.0f; // 塗りの脈打ちの位相
    float auraTimer_ = 0.0f;  // 次に粒を置くまでの計測
    float auraAngle_ = 0.0f;  // 粒を置く角度（少しずつ回して偏らせない）
    bool isOccupied_ = false; // いま相手が乗っているか
};
