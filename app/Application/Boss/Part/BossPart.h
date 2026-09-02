#pragma once
#include "Application/Boss/Data/BossPartLayout.h"
#include "Application/Character/ColorStruct.h"
#include "object/base/BaseObject.h"

/// <summary>
/// ボスの各パーツ・コアに貼るテクスチャ。
/// 1x1の白なので、マテリアルの色（＝パーツの色）がそのまま出る。
/// プリミティブ生成時の既定は debug/uvChecker.png なので、生成後に必ず上書きする
/// </summary>
inline constexpr const char *kBossTexturePath = "debug/white1x1.png";

/// <summary>
/// ボスの表面を覆う色付きパーツ1枚。
/// 見た目（プリミティブ球を潰した板）と当たり判定を持つだけの薄いクラスで、
/// 隣接関係や連鎖判定は BossPartGraph が受け持つ。
/// 破棄はせず「生存フラグ・描画・コライダーの有効/無効」で表現するため、
/// 衝突コールバックの最中に壊れてもポインタが無効化されない。
/// </summary>
class BossPart final : public Hagine::BaseObject {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>
    /// パーツを生成して球面上へ配置する
    /// </summary>
    /// <param name="objectName">オブジェクト名（一意）</param>
    /// <param name="desc">配置情報（方向・隣接）</param>
    /// <param name="radius">球の半径</param>
    /// <param name="partScale">パーツの大きさ（接線方向）</param>
    /// <param name="thickness">パーツの厚み（法線方向の倍率）</param>
    void InitPart(const std::string &objectName, const BossPartDesc &desc,
                  float radius, float partScale, float thickness);

    /// <summary>当たり判定を作る（タグ登録済みであること）</summary>
    /// <param name="tag">コライダーのタグ</param>
    /// <param name="collisionMask">衝突対象タグ</param>
    /// <param name="radiusScale">当たり半径（パーツの大きさに対する倍率）</param>
    void SetupCollider(const std::string &tag, const std::string &collisionMask, float radiusScale);

    /// <summary>
    /// 配置と大きさだけを更新する（オブジェクトは作り直さない）。
    /// 見た目の調整をドラッグ操作で即座に反映するために使う
    /// </summary>
    /// <param name="radius">球の半径</param>
    /// <param name="partScale">パーツの大きさ（接線方向）</param>
    /// <param name="thickness">パーツの厚み（法線方向の倍率）</param>
    /// <param name="colliderScale">当たり半径（パーツの大きさに対する倍率）</param>
    void ApplyLayout(float radius, float partScale, float thickness, float colliderScale);

    /// <summary>色を設定する（表示色も同時に反映する）</summary>
    /// <param name="color">色</param>
    /// <param name="rgba">表示色</param>
    void SetPartColor(Color color, const Hagine::Vector4 &rgba);

    /// <summary>破壊する（描画とコライダーを止め、生存フラグを落とす）</summary>
    void Break();

    /// <summary>復活させる（リトライ・デバッグ用）</summary>
    void Restore();

    /// <summary>ロックオン中の強調表示を切り替える</summary>
    /// <param name="highlight">強調するなら true</param>
    void SetHighlight(bool highlight);

    /// ===================================================
    /// getter
    /// ===================================================

    int GetPartIndex() const { return index_; }
    Color GetPartColor() const { return color_; }
    bool IsPartAlive() const { return GetIsAlive(); }
    const Hagine::Vector3 &GetLocalDirection() const { return localDirection_; }
    Hagine::SphereCollider *GetPartCollider() const { return collider_; }

    /// <summary>パーツ表面の外向き法線（ワールド）を取得する</summary>
    Hagine::Vector3 GetWorldNormal();

private:
    /// ===================================================
    /// private variables
    /// ===================================================

    int index_ = -1;                                          // パーツ番号
    Color color_ = Color::RED;                                // 現在の色
    Hagine::Vector3 localDirection_{0.0f, 1.0f, 0.0f};        // 球面上の方向（ローカル）
    Hagine::Vector4 baseRgba_{1.0f, 1.0f, 1.0f, 1.0f};        // 通常時の表示色
    Hagine::SphereCollider *collider_ = nullptr;              // 当たり判定（所有は BaseObject 側）
    bool isHighlighted_ = false;                              // ロックオン強調中か
};
