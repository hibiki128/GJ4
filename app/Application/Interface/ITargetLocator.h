#pragma once
#include "type/Vector3.h"

/// <summary>
/// 「狙う相手がどこにいるか」を提供するインターフェース。
/// ボスの攻撃（突進・落下）がプレイヤーの位置を知るために使う。
/// </summary>
class ITargetLocator {
public:
    virtual ~ITargetLocator() = default;

    /// <summary>
    /// 狙う相手のワールド座標を取得する
    /// </summary>
    /// <returns>Vector3: ワールド座標</returns>
    virtual Hagine::Vector3 GetTargetPosition() const = 0;

    /// <summary>
    /// 狙う相手の当たり半径を取得する（攻撃の当たり判定の目安に使う）
    /// </summary>
    /// <returns>float: 半径</returns>
    virtual float GetTargetRadius() const { return 1.0f; }

    /// <summary>
    /// 狙う相手が有効か（未接続・死亡時は false）
    /// </summary>
    /// <returns>bool: 有効なら true</returns>
    virtual bool IsTargetValid() const { return true; }
};
