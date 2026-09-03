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

    /// <summary>ロックオン中の強調表示を切り替える</summary>
    /// <param name="highlight">強調するなら true</param>
    void SetHighlight(bool highlight);

    /// <summary>
    /// 登場演出で「どこから飛んでくるか」と「動き出しの遅れ」を設定する。
    /// 球ごとに遅れをばらすと、集束が波打って見える
    /// </summary>
    /// <param name="localStart">飛来元のローカル座標</param>
    /// <param name="delaySeconds">動き出しの遅れ（秒）</param>
    void SetAppearStart(const Hagine::Vector3 &localStart, float delaySeconds) {
        appearStart_ = localStart;
        appearDelay_ = delaySeconds;
    }

    const Hagine::Vector3 &GetAppearStart() const { return appearStart_; }
    float GetAppearDelay() const { return appearDelay_; }

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
    bool isHighlighted_ = false;                         // ロックオン強調中か

    Hagine::Vector3 appearStart_{}; // 登場演出での飛来元（ローカル）
    float appearDelay_ = 0.0f;      // 登場演出の動き出しの遅れ（秒）
};
