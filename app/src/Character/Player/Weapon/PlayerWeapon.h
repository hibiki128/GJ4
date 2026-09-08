#pragma once
#include "src/Character/Player/Weapon/Bullet/PlayerBullet.h"
#include <Math/type/Vector3.h>

class PlayerBulletManager;

/// <summary>
/// プレイヤーの武器。
/// 弾の飛び方（速さ・寿命・追尾の強さ・大きさ）と連射間隔はここが持つ。
/// 「どこへ・何色で撃つか」「どこへ当たりを聞くか」は撃つ側が決めて渡す
/// </summary>
class PlayerWeapon {
public:
    /// <summary>撃つたびに変わる情報</summary>
    struct FireRequest {
        Hagine::Vector3 origin = {0.0f, 0.0f, 0.0f};
        Hagine::Vector3 direction = {0.0f, 0.0f, 1.0f};
        Hagine::Vector4 rgba = {1.0f, 1.0f, 1.0f, 1.0f};

        // 追尾先。空なら補正なしで真っ直ぐ飛ぶ
        PlayerBullet::TargetPositionGetter targetPositionGetter{};
        // 着弾の問い合わせ先。空なら寿命が尽きるまで飛び続ける
        PlayerBullet::HitTester hitTester{};
    };

    /// <summary>弾の飛び方の調整値（デバッグUIから触る）</summary>
    struct Params {
        float fireInterval = 0.18f;  // 連射間隔（秒）
        float speed = 45.0f;         // 弾速（単位/秒）
        float lifeTime = 3.0f;       // 弾の寿命（秒）
        float correctionRate = 2.0f; // 軌道補正の強さ（1秒あたりの補正割合）
        // 1秒あたりに曲がってよい角度の上限。
        // 狙う先は発射時に確定した一点なので、補正はマズルとカメラの視差を詰めるだけで足りる
        float maxTurnDegreesPerSecond = 60.0f;
        float radius = 0.3f; // 弾の半径
    };

    /// <summary>待機中の弾を1発撃つ</summary>
    /// <param name="bullets">弾のプール</param>
    /// <param name="request">今回の発射内容</param>
    /// <returns>bool: 撃てれば true（プールに空きが無ければ false）</returns>
    bool Fire(PlayerBulletManager& bullets, const FireRequest& request);

    float GetFireInterval() const { return params_.fireInterval; }

    // 実行時調整（GameParamHub）へポインタを渡すため非constで返す
    Params& GetParams() { return params_; }
    const Params& GetParams() const { return params_; }

private:
    Params params_{};
};
