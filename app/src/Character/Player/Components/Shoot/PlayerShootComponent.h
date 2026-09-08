#pragma once
#include "src/Character/ColorStruct.h"
#include "src/Character/Player/Core/PlayerContext.h"
#include "src/Interface/IBossTargetQuery.h"
#include <functional>

#include <src/Character/Player/Weapon/PlayerWeapon.h>

/// <summary>
/// プレイヤーの射撃。
///
/// 照準はカメラの射線（PlayerContext の aimOrigin_ / aimDirection_）を使い、
/// そこから同色の的をソフトロックオンして、プレイヤーの位置から弾を撃つ。
/// 相手の具象クラス（Boss / BossSpider）は知らず、IBossTargetQuery 越しにだけ触る。
/// </summary>
class PlayerShootComponent {
public:
	/// <summary>いま撃つ相手を返す関数（形態の切り替えはシーン側が判断する）</summary>
	using TargetProvider = std::function<IBossTargetQuery *()>;

	PlayerShootComponent() = default;
	~PlayerShootComponent() = default;

    void Update(PlayerContext& context);

    // 使用する武器をセットする（実体は Player が持つ）
    void SetWeapon(PlayerWeapon* weapon) { weapon_ = weapon; }

    // 撃つ相手の提供元をセットする（配線はシーン側で行う）
    void SetTargetProvider(TargetProvider provider) { targetProvider_ = std::move(provider); }

    // 撃つ色（ボスは IColorProvider 越しにこの色を見て、同じ色かどうかを判定する）
    Color GetSelectedColor() const { return selectedColor_; }
    void SetSelectedColor(Color color) { selectedColor_ = color; }

    // 調整パラメータをデバッグUIへ登録する（Player::Init から一度だけ呼ぶ。SetWeapon の後）
    void RegisterParams();

    // 射撃の状態を表示する（シーンの「オブジェクト設定」窓から呼ぶ）
    void DrawImGui();

    // デバッグ表示用
    const LockOnResult& GetLockOnResult() const { return lockOn_; }
    const BulletHitResult& GetLastHitResult() const { return lastHit_; }
    bool& DrawAimLineFlag() { return drawAimLine_; }

private:

    // 入力で撃つ色を切り替える
    void UpdateColorSelection(const PlayerContext& context);

    /// <summary>いま撃つ相手（提供元が未設定・相手不在なら nullptr）</summary>
    IBossTargetQuery* ActiveTarget() const;

    /// <summary>照準方向からロックオン対象を探し、強調表示を更新する</summary>
    void UpdateLockOn(IBossTargetQuery* target, const Hagine::Vector3& origin,
                      const Hagine::Vector3& aimDirection);

    /// <summary>弾を1発撃つ</summary>
    void FireBullet(PlayerContext& context, IBossTargetQuery* target,
                    const Hagine::Vector3& aimDirection);

    /// <summary>照準線とロックオン位置を線で表示する（確認用）</summary>
    void DrawAimLine(IBossTargetQuery* target, const Hagine::Vector3& origin,
                     const Hagine::Vector3& aimDirection) const;

    PlayerWeapon* weapon_ = nullptr;
    TargetProvider targetProvider_{};

    float cooldown_ = 0.0f;
    Color selectedColor_ = Color::RED;

    LockOnResult lockOn_{};     // 現在のロックオン結果
    BulletHitResult lastHit_{}; // 直近の着弾結果（デバッグUI表示用）

    bool drawAimLine_ = true; // 照準線・ロックオン位置を線で表示する

};
