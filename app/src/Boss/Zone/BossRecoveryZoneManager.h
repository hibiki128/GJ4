#pragma once
#include "src/Boss/Data/BossColorPalette.h"
#include "src/Boss/Data/BossParameters.h"
#include "src/Boss/Zone/BossRecoveryZone.h"
#include "src/Character/ColorStruct.h"
#include "type/Vector3.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

class IAmmoRecoverySink;
class IFieldBounds;
class ITargetLocator;

namespace Hagine {
class ViewProjection;
}

/// <summary>
/// 残弾を回復するエリアをまとめて受け持つ。
///
/// ボスがひと続きの攻撃を終えるたびに1つ出し、寿命が来たら畳む。
/// 色は毎回ランダムなので、「いま欲しい色が出ているか」で動き方が変わる。
///
/// エリアは実行中に破棄せず、使い終わったものを寝かせて使い回す（増やすだけのプール）。
/// 円盤は BaseObject なので、前フレームのGPUコマンドが掴んだまま解放すると落ちるため
/// </summary>
class BossRecoveryZoneManager {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>初期化（シーンの初期化から1回だけ）</summary>
    /// <param name="namePrefix">オブジェクト名の接頭辞</param>
    /// <param name="params">調整値（ボスが持つものを参照する。実行時の変更が即反映される）</param>
    void Init(const std::string &namePrefix, BossRecoveryZoneParams *params);

    /// <summary>
    /// ボスが攻撃を1つ終えたことを知らせる。ここでエリアが1つ生まれる
    /// </summary>
    /// <param name="bossPosition">エリアが飛び出す元</param>
    /// <param name="palette">色を選ぶもとのパレット（ボスが使っている色から選ぶ）</param>
    void NotifyAttackFinished(const Hagine::Vector3 &bossPosition, const BossColorPalette &palette);

    /// <summary>全エリアを進める（毎フレーム）</summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    void Update(float deltaTime);

    /// <summary>描画する</summary>
    /// <param name="viewProjection">ビュープロジェクション</param>
    void Draw(const Hagine::ViewProjection &viewProjection);

    /// <summary>出ているエリアをすべて畳む</summary>
    void ClearAll();

    /// <summary>調整UI</summary>
    /// <param name="bossPosition">「いますぐ出す」で使うボスの位置</param>
    /// <param name="palette">「いますぐ出す」で使うパレット</param>
    /// <param name="onSave">「この値を保存」で呼ぶ処理（未設定なら保存ボタンを出さない）</param>
    void DrawImGui(const Hagine::Vector3 &bossPosition, const BossColorPalette &palette,
                   const std::function<void()> &onSave = {});

    /// ===================================================
    /// setter
    /// ===================================================

    /// <summary>乗っているか調べる相手（プレイヤー）</summary>
    void SetTargetLocator(const ITargetLocator *locator) { pTargetLocator_ = locator; }

    /// <summary>回復を要求する先（プレイヤー）</summary>
    void SetAmmoSink(IAmmoRecoverySink *sink) { pAmmoSink_ = sink; }

    /// <summary>落とせる範囲（フィールドの外へ出さないため）</summary>
    void SetFieldBounds(const IFieldBounds *field) { pFieldBounds_ = field; }

private:
    /// ===================================================
    /// private method
    /// ===================================================

    /// <summary>寝ているエリアを1つ借りる（無ければ増やす）</summary>
    /// <returns>BossRecoveryZone*: 使えるエリア</returns>
    BossRecoveryZone *Acquire();

    /// <summary>落とす場所を決める</summary>
    /// <param name="bossPosition">飛び出す元</param>
    /// <returns>Vector3: 落とす場所（地面の高さ）</returns>
    Hagine::Vector3 PickLanding(const Hagine::Vector3 &bossPosition) const;

    /// <summary>出ている数が上限を超えていたら、古いものから畳む</summary>
    void TrimOldest();

    /// <summary>いま出ている（畳み始めていない）数</summary>
    int CountOpen() const;

    /// ===================================================
    /// private variables
    /// ===================================================

    // 増やすだけのプール。実行中に破棄すると、前フレームのGPUコマンドが
    // まだ円盤のリソースを参照していて落ちる
    std::vector<std::unique_ptr<BossRecoveryZone>> zones_{};
    std::string namePrefix_{};

    // 調整値（非所有）。調整UIからそのまま触るので非const で持つ
    BossRecoveryZoneParams *pParams_ = nullptr;
    const ITargetLocator *pTargetLocator_ = nullptr;  // 乗っているか調べる相手（非所有）
    IAmmoRecoverySink *pAmmoSink_ = nullptr;          // 回復の要求先（非所有）
    const IFieldBounds *pFieldBounds_ = nullptr;      // 落とせる範囲（非所有）

    int spawnCount_ = 0;      // これまでに出した数（名前とデバッグ表示に使う）
    int lastColorIndex_ = -1; // 直前に出した色（続けて同じ色にならないようにする）
};
