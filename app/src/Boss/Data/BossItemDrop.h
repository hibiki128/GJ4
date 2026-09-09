#pragma once
#include "src/Interface/IFieldBounds.h"
#include "src/Interface/ITargetLocator.h"
#include "src/Item/HealItemManager.h"
#include "type/Vector3.h"

/// <summary>
/// 床の高さ。フィールドの床（plane）もプレイヤーの足元も 0 にそろえてある。
/// アイテムはここから HealItemParams::spawnHeight ぶん浮いた位置に出る
/// </summary>
inline constexpr float kBossDropGroundHeight = 0.0f;

/// <summary>
/// 体のふちと膜のふちの間に空ける距離。
///
/// ずらす量は「体の半径 ＋ 膜の半径 ＋ ここ」で決めている。体の半径に倍率を掛ける形に
/// しないのは、膜の大きさがボスの倍率とは無関係の固定値だから。
/// 倍率で割った形にすると、等倍のときに膜がボスを飲み込んでしまう
/// </summary>
inline constexpr float kBossDropClearance = 0.6f;

/// <summary>
/// ひるんだ隙に拾ってもらう回復アイテムを1つ落とす。
///
/// 落とすのは体の足元。ただし真下だと体に埋もれて見えず、狙って撃つこともできないので、
/// 狙っている相手（プレイヤー）のほうへ体の大きさぶんずらす。
/// ずらす量を体の半径から決めているので、ボス全体の倍率を変えても埋もれない。
///
/// 相手が分からないときは足元へそのまま落とす。
/// 壁際でひるんだときに範囲の外へ出ないよう、最後に行動範囲の内側へ寄せる。
///
/// 出し方は形態で変えていない。第1形態でも第2形態でも「ひるんだら足元に落ちる」を
/// 同じ形にしておかないと、プレイヤーが拾いに行く間合いを覚え直すことになるため
/// </summary>
/// <param name="bodyPosition">落とし主の位置（ワールド。高さは見ない）</param>
/// <param name="bodyRadius">落とし主の体の半径（ずらす量の基準）</param>
/// <param name="target">狙っている相手（未接続なら nullptr）</param>
/// <param name="field">行動範囲（未接続なら nullptr）</param>
inline void DropHealItemOnStagger(const Hagine::Vector3 &bodyPosition, float bodyRadius,
                                  const ITargetLocator *target, const IFieldBounds *field) {
    HealItemManager *items = HealItemManager::GetInstance();
    Hagine::Vector3 drop = {bodyPosition.x, kBossDropGroundHeight, bodyPosition.z};

    if (target != nullptr) {
        Hagine::Vector3 toTarget = target->GetTargetPosition() - drop;
        toTarget.y = 0.0f;
        // 真上から見て重なっているときはずらす向きが決まらないので、足元のままにする
        if (toTarget.LengthSq() > 0.0001f) {
            const float distance = bodyRadius + items->GetParams().sealRadius + kBossDropClearance;
            drop += toTarget.Normalize() * distance;
        }
    }

    if (field != nullptr) {
        field->ClampToField(drop);
    }
    // 範囲へ寄せる計算で高さが動いても、拾える高さへ戻す
    drop.y = kBossDropGroundHeight;

    items->Spawn(drop);
}
