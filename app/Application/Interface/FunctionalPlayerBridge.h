#pragma once
#include "Application/Interface/IColorProvider.h"
#include "Application/Interface/ITargetLocator.h"
#include <functional>
#include <utility>

/// <summary>
/// プレイヤーとボスを繋ぐアダプタ。
/// 取得処理を std::function で受け取るため、ボス側のコードはプレイヤーの
/// 具象クラス（Player.h）へ一切コンパイル依存しない。
/// 配線はシーン側で行う:
///   bridge = std::make_unique&lt;FunctionalPlayerBridge&gt;(
///       [p = player_.get()] { return p-&gt;GetWorldPosition(); },
///       [this] { return debugColor_; });
/// プレイヤー側に色APIが生えたら、2つ目のラムダを差し替えるだけで本接続になる。
/// </summary>
class FunctionalPlayerBridge final : public ITargetLocator, public IColorProvider {
public:
    using PositionGetter = std::function<Hagine::Vector3()>;
    using ColorGetter = std::function<Color()>;
    using RadiusGetter = std::function<float()>;
    using ValidGetter = std::function<bool()>;

    FunctionalPlayerBridge() = default;
    FunctionalPlayerBridge(PositionGetter position, ColorGetter color)
        : positionGetter_(std::move(position)), colorGetter_(std::move(color)) {}

    /// ===================================================
    /// ITargetLocator
    /// ===================================================

    Hagine::Vector3 GetTargetPosition() const override {
        return positionGetter_ ? positionGetter_() : Hagine::Vector3{0.0f, 0.0f, 0.0f};
    }

    float GetTargetRadius() const override {
        return radiusGetter_ ? radiusGetter_() : radius_;
    }

    bool IsTargetValid() const override {
        if (!positionGetter_) {
            return false;
        }
        return validGetter_ ? validGetter_() : true;
    }

    /// ===================================================
    /// IColorProvider
    /// ===================================================

    Color GetSelectedColor() const override {
        return colorGetter_ ? colorGetter_() : Color::RED;
    }

    /// ===================================================
    /// setter
    /// ===================================================

    void SetPositionGetter(PositionGetter getter) { positionGetter_ = std::move(getter); }
    void SetColorGetter(ColorGetter getter) { colorGetter_ = std::move(getter); }
    void SetRadiusGetter(RadiusGetter getter) { radiusGetter_ = std::move(getter); }
    void SetValidGetter(ValidGetter getter) { validGetter_ = std::move(getter); }
    void SetRadius(float radius) { radius_ = radius; }

private:
    PositionGetter positionGetter_{};
    ColorGetter colorGetter_{};
    RadiusGetter radiusGetter_{};
    ValidGetter validGetter_{};
    float radius_ = 1.0f;
};
