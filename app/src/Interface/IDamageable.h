#pragma once
#include "type/Vector3.h"

/// <summary>
/// ダメージ1回分の情報
/// </summary>
struct DamageInfo {
    float amount = 0.0f;             // ダメージ量
    Hagine::Vector3 hitPoint{};      // 着弾位置（演出用）
    int chainSize = 0;               // 連鎖規模（連鎖以外の攻撃は 0）
    float staggerTime = 0.0f;        // 怯み時間（秒）
};

/// <summary>
/// ダメージを受け取れるもののインターフェース。
/// ボスが実装する。プレイヤー側も同じ口を実装すれば、ボスの攻撃をそのまま流し込める。
/// </summary>
class IDamageable {
public:
    virtual ~IDamageable() = default;

    /// <summary>
    /// ダメージを適用する
    /// </summary>
    /// <param name="info">ダメージ情報</param>
    virtual void ApplyDamage(const DamageInfo &info) = 0;

    /// <summary>
    /// 残りHPを取得する
    /// </summary>
    /// <returns>float: 残りHP</returns>
    virtual float GetHp() const = 0;

    /// <summary>
    /// 死亡しているか
    /// </summary>
    /// <returns>bool: 死亡していれば true</returns>
    virtual bool IsDead() const = 0;
};
