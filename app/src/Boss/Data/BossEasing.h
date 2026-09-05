#pragma once
#include "Easing.h"
#include <algorithm>

/// <summary>
/// 両端でなめらかに止まる 0〜1 の補間。
///
/// エンジンの EasingType::InOutSine は使ってはいけない。
/// Engine/Math/Easing.cpp の EaseInOutSine だけ他の InOut 系と違って
/// t >= 1 の分岐が無く、t を 0〜2 に伸ばしたまま 0.5*(1-cos(t*PI)) を1本で使っている。
/// そのため進捗0.5で 1.0 に達したあと、進捗1.0では 0.0 へ戻ってしまい、
/// 動きが「再生 → 逆再生」に見える。
/// InOutQuad / InOutCubic には t >= 1 の分岐があり、正しく 1.0 で終わる。
/// エンジン側が直ったら EasingType::InOutSine に戻してよい。
/// </summary>
/// <param name="progress">進捗（0〜1。範囲外は丸める）</param>
/// <returns>float: なめらかにした進捗（0〜1）</returns>
inline float SmoothInOut(float progress) {
    const float clamped = std::clamp(progress, 0.0f, 1.0f);
    return Hagine::ApplyEasing(Hagine::EasingType::InOutQuad, 0.0f, 1.0f, clamped, 1.0f);
}
