#pragma once
#include "src/Boss/Lattice/BossSphereLattice.h"
#include "src/Character/ColorStruct.h"
#include "object/base/BaseObject.h"

/// <summary>
/// ボスの殻を構成する球・コアに貼るテクスチャ。
/// 1x1の白なので、マテリアルの色（＝球の色）がそのまま出る。
/// プリミティブ生成時の既定は debug/uvChecker.png なので、生成後に必ず上書きする
/// </summary>
inline constexpr const char *kBossTexturePath = "debug/white1x1.png";

/// <summary>
/// 殻を構成する球1個。FCC格子のセル1つに対応する。
///
/// 生成・破棄はせずプールで使い回す（BaseObject の生成はJSON探索を伴うため、
/// 着弾のたびに作ると重い）。使っていない球は非表示にして待機させる。
///
/// 見た目そのものは BossShellMetaBall が色ごとの融合メッシュとして描くので、
/// このオブジェクトのモデルは普段は描かない。位置・色・セルを持つ「殻の中身」として働き、
/// ロックオンで強調するときだけ実体の球を融合面へ重ねて出す
/// （融合メッシュは色を1つしか持てず、1個だけ色を変えられないため）。
/// </summary>
class BossSphere final : public Hagine::BaseObject {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>球を生成する（待機状態で始まる）</summary>
    /// <param name="objectName">オブジェクト名（一意）</param>
    /// <param name="radius">球の半径</param>
    void InitSphere(const std::string &objectName, float radius);

    /// <summary>格子セルへ配置して有効化する</summary>
    /// <param name="cell">配置先のセル</param>
    /// <param name="localPosition">セル中心のローカル座標</param>
    /// <param name="color">色</param>
    /// <param name="rgba">表示色</param>
    void Place(const ShellCell &cell, const Hagine::Vector3 &localPosition,
               Color color, const Hagine::Vector4 &rgba);

    /// <summary>待機状態へ戻す（プールへ返却する）</summary>
    void Deactivate();

    /// <summary>見た目の半径を更新する</summary>
    /// <param name="radius">球の半径</param>
    void SetSphereRadius(float radius);

    /// <summary>色を変えずに位置だけ引き直す（格子定数を変えたときに使う）</summary>
    /// <param name="localPosition">セル中心のローカル座標</param>
    void SetLocalPosition(const Hagine::Vector3 &localPosition);

    /// <summary>
    /// ロックオン中の強調表示を切り替える。
    /// 強調中だけ実体の球を描き、融合面から少し出るよう大きめに見せる
    /// </summary>
    /// <param name="highlight">強調するなら true</param>
    /// <param name="scale">強調時の大きさ倍率（融合面へ埋もれないよう1より大きくする）</param>
    void SetHighlight(bool highlight, float scale);

    /// ===================================================
    /// getter
    /// ===================================================

    bool IsActive() const { return GetIsAlive(); }
    const ShellCell &GetCell() const { return cell_; }
    Color GetSphereColor() const { return color_; }

    /// <summary>球の中心のローカル座標</summary>
    const Hagine::Vector3 &GetLocalPosition() const { return localPosition_; }

    /// <summary>球の外向き法線（ワールド）。ロックオンの表裏判定に使う</summary>
    Hagine::Vector3 GetWorldNormal();

private:
    /// ===================================================
    /// private variables
    /// ===================================================

    ShellCell cell_{};                                // 占有しているセル
    Color color_ = Color::RED;                           // 現在の色
    Hagine::Vector3 localPosition_{};                    // セル中心のローカル座標
    Hagine::Vector4 baseRgba_{1.0f, 1.0f, 1.0f, 1.0f};   // 通常時の表示色
    float radius_ = 1.0f;                                // 球の半径（強調時の拡大の基準）
    bool isHighlighted_ = false;                         // ロックオン強調中か
};
