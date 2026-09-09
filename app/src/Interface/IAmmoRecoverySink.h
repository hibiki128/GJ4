#pragma once
#include "src/Character/ColorStruct.h"

/// <summary>
/// 「残弾の回復を早めてほしい」と伝える口。
///
/// 回復エリアのようなギミックが、乗っているあいだ毎フレーム呼ぶ。
/// 離れれば呼ばれなくなるので、後始末（倍率を1.0へ戻す）が要らない。
/// ギミック側はプレイヤーの具象クラスを知らずに済む。
/// </summary>
class IAmmoRecoverySink {
public:
    virtual ~IAmmoRecoverySink() = default;

    /// <summary>
    /// この1フレームだけ、指定した色の回復倍率を要求する
    /// </summary>
    /// <param name="color">早めたい色</param>
    /// <param name="scale">回復速度の倍率（2.0f で倍速）</param>
    virtual void RequestAmmoRegen(Color color, float scale) = 0;

    /// <summary>
    /// その色の弾が満タンか。満タンのエリアを畳むのに使う
    /// </summary>
    /// <param name="color">調べる色</param>
    /// <returns>bool: 満タンなら true</returns>
    virtual bool IsAmmoFull(Color color) const = 0;
};
