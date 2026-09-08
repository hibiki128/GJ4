#pragma once
#include "object/base/BaseObject.h"
#include "type/Vector3.h"
#include <memory>
#include <string>

/// <summary>
/// 攻撃が降ってくる場所と、当たる瞬間を地面に出す警告表示。
///
/// 外枠（warningOutLine）が攻撃範囲そのもので、内側の塗り（warningFill）が
/// 大きさ0から外枠へ向かって広がる。塗りが外枠に追いつく＝当たる瞬間なので、
/// 「どこに・いつ来るか」が一目で分かる。
/// どちらも半径1の平板なので、スケールがそのまま攻撃範囲の半径になる。
/// </summary>
class BossWarningMarker {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>
    /// 表示物を作る（最初の1回だけ作られ、以降は使い回す）。
    /// 実行中に破棄するとGPUが参照中のリソースを解放して落ちるため、消さずに隠す
    /// </summary>
    /// <param name="namePrefix">オブジェクト名の接頭辞（一意にすること）</param>
    void Ensure(const std::string &namePrefix);

    /// <summary>
    /// 出す場所と範囲を決める（地面の高さに寝かせて置く）
    /// </summary>
    /// <param name="center">攻撃が来る場所（高さは無視して地面に置く）</param>
    /// <param name="radius">攻撃範囲の半径</param>
    void Show(const Hagine::Vector3 &center, float radius);

    /// <summary>
    /// 当たるまでの進み具合を渡す（0で塗りなし、1で外枠と同じ大きさ＝命中の瞬間）
    /// </summary>
    /// <param name="ratio">進み具合（0〜1）</param>
    void SetFillRatio(float ratio);

    /// <summary>消す</summary>
    void Hide();

    /// <summary>描画する（攻撃の Draw から呼ぶ）</summary>
    void Draw(const Hagine::ViewProjection &viewProjection);

    /// <summary>出ているか</summary>
    bool IsVisible() const { return isVisible_; }

private:
    /// ===================================================
    /// private variables
    /// ===================================================

    std::unique_ptr<Hagine::BaseObject> outline_; // 攻撃範囲の外枠
    std::unique_ptr<Hagine::BaseObject> fill_;    // 当たる瞬間を示す塗り

    Hagine::Vector3 center_{}; // 出している場所
    float radius_ = 1.0f;      // 攻撃範囲の半径
    bool isVisible_ = false;   // 出ているか
};
