#pragma once
#include "src/Character/ColorStruct.h"
#include "src/Character/Player/Core/PlayerAimReport.h"
#include "src/Character/Player/Core/PlayerContext.h"
#include "src/Interface/IBossTargetQuery.h"
#include <functional>

#include <src/Character/Player/Weapon/PlayerWeapon.h>

/// <summary>
/// プレイヤーの射撃。
///
/// 照準はカメラの射線（PlayerContext の aimOrigin_ / aimDirection_ ＝ 画面中心）を使う。
/// その射線を相手へ飛ばして「最初に当たる点」＝着弾地点を先に確定させ、
/// 弾はプレイヤーの位置からその一点へ向けて撃つ。
/// こうすると弾の誘導は「カメラとマズルの視差ぶんを詰める」だけの仕事になるので、
/// ホーミングが弱くても狙ったところ（画面中心）に当たる。
///
/// 撃つ前にエイムアシストを一段はさむ。照準が球に乗っているときは、その表面ではなく
/// 球の真ん中を狙う先にする。照準（aimPoint_）は動かさないので画面中央の十字は固定のまま。
///
/// また射線の起点がカメラとマズルで違うので、同じ一点を狙っていても弾は途中の球へ
/// 先にぶつかることがある。その「弾が実際に当たる点」も毎フレーム求めて PlayerAimReport で
/// 知らせており、照準とずれていれば画面に発射レティクル（水色の円）が出る。
///
/// ソフトロックオンは色の判定と強調表示のためだけに残してあり、弾の誘導には使わない。
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

    /// <summary>
    /// このフレームの狙いの決まり方（レティクルの表示に使う）。
    /// 中身は弾を撃つのに使ったものと同じ値で、表示のために作り直した値は入っていない
    /// </summary>
    const PlayerAimReport& GetAimReport() const { return aimReport_; }

    // デバッグ表示用
    const LockOnResult& GetLockOnResult() const { return lockOn_; }
    const BulletHitResult& GetLastHitResult() const { return lastHit_; }
    const Hagine::Vector3& GetAimPoint() const { return aimPoint_; }
    bool& DrawAimLineFlag() { return drawAimLine_; }

private:

    // 入力で撃つ色を切り替える
    void UpdateColorSelection(const PlayerContext& context);

    /// <summary>いま撃つ相手（提供元が未設定・相手不在なら nullptr）</summary>
    IBossTargetQuery* ActiveTarget() const;

    /// <summary>
    /// 画面中心の射線を飛ばして着弾地点を求める。
    /// 何にも当たらなければ射程の端（origin + direction * aimRayLength_）を返すので、
    /// 相手がいない方向へ撃っても弾は素直に真っ直ぐ飛ぶ
    /// </summary>
    /// <param name="target">いま撃つ相手（nullptr なら射程の端をそのまま返す）</param>
    /// <param name="origin">射線の起点（カメラ基準）</param>
    /// <param name="direction">射線の向き（＝画面中心）</param>
    /// <returns>Vector3: 着弾地点（ワールド）</returns>
    Hagine::Vector3 ResolveAimPoint(IBossTargetQuery* target, const Hagine::Vector3& origin,
                                    const Hagine::Vector3& direction);

    /// <summary>
    /// エイムアシストを効かせた「実際に狙う一点」を求める。
    ///
    /// 照準が球に乗っているとき、その球の表面（aimPoint_）ではなく真ん中を狙う。
    /// 照準そのものは動かさないので画面中央の十字は固定のまま。動くのは弾の行き先だけで、
    /// その結果は発射レティクルとして画面に出る（仕様書 9.1 の照準補正にあたる）
    /// </summary>
    /// <returns>Vector3: 実際に狙う一点（ワールド）</returns>
    Hagine::Vector3 ResolveAssistPoint() const;

    /// <summary>
    /// マズルから狙う先へ向かう発射方向を求める。
    /// 弾を撃つときと発射レティクルを出すときの両方がここを通るので、
    /// 「表示している向きと実際に飛ぶ向きが違う」ことが起こりえない（仕様書 17.1 / 17.3）
    /// </summary>
    /// <param name="muzzle">弾が出る位置（プレイヤーの位置）</param>
    /// <param name="fallback">狙う先とマズルが重なっているときに使う向き</param>
    /// <returns>Vector3: 正規化された発射方向</returns>
    Hagine::Vector3 ResolveFireDirection(const Hagine::Vector3& muzzle,
                                         const Hagine::Vector3& fallback) const;

    /// <summary>
    /// 発射方向へ射線を飛ばして、弾が最初に当たる点を求める。
    /// 照準（ResolveAimPoint）がカメラから飛ばすのに対し、こちらはマズルから飛ばす。
    /// 同じ一点を狙っていても起点が違うので、途中の球に先にぶつかることがある。
    /// そのズレを見せるのが発射レティクルの役目
    /// </summary>
    /// <param name="target">いま撃つ相手（nullptr なら射程の端をそのまま返す）</param>
    /// <param name="muzzle">射線の起点（プレイヤーの位置）</param>
    /// <param name="direction">射線の向き（＝発射方向）</param>
    /// <returns>Vector3: 弾が最初に当たる点（ワールド）</returns>
    Hagine::Vector3 ResolveFirePoint(IBossTargetQuery* target, const Hagine::Vector3& muzzle,
                                     const Hagine::Vector3& direction);

    /// <summary>照準方向からロックオン対象を探し、強調表示を更新する</summary>
    void UpdateLockOn(IBossTargetQuery* target, const Hagine::Vector3& origin,
                      const Hagine::Vector3& aimDirection);

    /// <summary>弾を1発撃つ（狙う先は確定済みの aimPoint_）</summary>
    /// <returns>bool: 弾が出れば true（プールに空きが無ければ false）</returns>
    bool FireBullet(PlayerContext& context, IBossTargetQuery* target);

    /// <summary>照準線・着弾地点・弾が通る線を表示する（確認用）</summary>
    void DrawAimLine(const PlayerContext& context) const;

    PlayerWeapon* weapon_ = nullptr;
    TargetProvider targetProvider_{};

    float cooldown_ = 0.0f;
    Color selectedColor_ = Color::RED;

    LockOnResult lockOn_{};     // 現在のロックオン結果（色の判定と強調表示にだけ使う）
    BulletHitResult lastHit_{}; // 直近の着弾結果（デバッグUI表示用）

    Hagine::Vector3 aimPoint_{}; // いま画面中心が指している着弾地点（ワールド）
    bool aimPointHit_ = false;   // 着弾地点が相手にヒットして決まったか（false なら射程の端）

    Hagine::Vector3 aimHitCenter_{}; // 照準が乗っている球の中心（エイムアシストの寄せ先）
    Hagine::Vector3 assistPoint_{};  // アシストを効かせた後の、実際に狙う一点

    Hagine::Vector3 firePoint_{}; // いま撃ったら弾が最初に当たる点（ワールド。マズル基準）
    bool firePointHit_ = false;   // 発射の点が相手にヒットして決まったか

    PlayerAimReport aimReport_{}; // 上をひとまとめにした、表示側への報告

    float aimRayLength_ = 200.0f; // 照準レイの長さ＝当たらなかったときの着弾距離

    // エイムアシスト。照準が球に乗っているときだけ効くので、乗せるところまでは自分で狙う。
    // 強さ 1 で球の真ん中ぴったり、0 で寄せない
    bool aimAssistEnabled_ = true;
    float aimAssistStrength_ = 1.0f;

    bool drawAimLine_ = true; // 照準線・着弾地点を線で表示する

};
