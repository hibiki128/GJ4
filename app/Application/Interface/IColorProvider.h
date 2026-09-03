#pragma once
#include "Application/Character/ColorStruct.h"

/// <summary>
/// 「現在どの色を選んでいるか」を提供するインターフェース。
/// ボスはプレイヤーの具象クラスを知らずに、この口だけを見て色一致判定を行う。
/// </summary>
class IColorProvider {
public:
    virtual ~IColorProvider() = default;

    /// <summary>
    /// 現在選択中の色を取得する
    /// </summary>
    /// <returns>Color: 選択中の色</returns>
    virtual Color GetSelectedColor() const = 0;
};
