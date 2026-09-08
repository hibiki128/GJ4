#pragma once
#include "type/Vector3.h"

/// <summary>
/// 「動き回れる範囲」を提供するインターフェース。
/// プレイヤーもボスも Field の型を知らず、この口だけを見て自分を範囲内へ収める。
/// </summary>
class IFieldBounds {
public:
    virtual ~IFieldBounds() = default;

    /// <summary>
    /// 範囲の外に出ていたら境界上へ戻し、外へ向かう速度成分を消す
    /// </summary>
    /// <param name="position">対象の座標（範囲外なら書き換わる）</param>
    /// <param name="velocity">対象の速度（外向きの成分だけ消える）</param>
    virtual void ClampToField(Hagine::Vector3 &position, Hagine::Vector3 &velocity) const = 0;

    /// <summary>
    /// 範囲の中にいるか
    /// </summary>
    /// <param name="position">調べる座標</param>
    /// <returns>bool: 中にいれば true</returns>
    virtual bool Contains(const Hagine::Vector3 &position) const = 0;

    /// <summary>
    /// 速度を持たない相手（座標を直接動かす敵）向けの入口。
    /// 押し戻しの結果は座標にだけ現れる
    /// </summary>
    /// <param name="position">対象の座標（範囲外なら書き換わる）</param>
    void ClampToField(Hagine::Vector3 &position) const {
        Hagine::Vector3 ignoredVelocity{};
        ClampToField(position, ignoredVelocity);
    }
};
