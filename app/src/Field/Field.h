#pragma once
#include "collider/type/CylinderCollider.h"
#include "src/Interface/IFieldBounds.h"
#include "type/Vector3.h"
#include "type/Vector4.h"
#include "Utility/Debug/Param/GameParamHub.h"
#include <memory>
#include <string>

/// <summary>
/// 戦う場所の外周。円柱を1本立てて、その内側だけを動けるようにする。
///
/// 形の持ち主はエンジンの CylinderCollider（inward = 内側へ閉じ込める向き）で、
/// 押し戻しもそのまま CylinderCollider::Clamp に任せている。ただし
/// CollisionManager には登録しない。登録すると「中身の詰まった円柱」として
/// 他のコライダーと判定され、内側にいるものを外へ押し出してしまうため。
/// 位置を範囲内へ収めるのは、対象それぞれが自分の更新の最後に呼ぶ形にしてある。
///
/// 見た目はモデルではなく線だけ（LineRenderer）。線の描画はエンジン側が
/// USE_IMGUI のときにしか流さないので、Release ビルドでは判定だけが残る。
/// </summary>
class Field final : public IFieldBounds {
public:
    /// <summary>
    /// 円柱のコライダーを作り、調整パラメータを登録する（シーンの初期化から1回だけ）
    /// </summary>
    /// <param name="name">コライダー名（保存JSONのファイル名になる）</param>
    void Init(const std::string &name = "Field");

    /// <summary>
    /// 外周の線を積む（毎フレーム。線はフレーム単位で積み直される）
    /// </summary>
    void DrawLine() const;

    /// <summary>
    /// 大きさと線の見た目を触るUI（シーンの「オブジェクト設定」窓から呼ぶ）
    /// </summary>
    void DrawImGui();

    /// ===================================================
    /// IFieldBounds
    /// ===================================================

    /// <summary>範囲の外に出ていたら境界上へ戻し、外向きの速度を消す</summary>
    void ClampToField(Hagine::Vector3 &position, Hagine::Vector3 &velocity) const override;

    /// <summary>範囲の中にいるか（高さは見ない。XZの円で判定する）</summary>
    bool Contains(const Hagine::Vector3 &position) const override;

    // 基底の1引数版（速度を持たない相手向け）も名前解決に含める
    using IFieldBounds::ClampToField;

    /// ===================================================
    /// getter / setter
    /// ===================================================

    const Hagine::Vector3 &GetCenter() const { return center_; }
    float GetRadius() const { return radius_; }
    float GetHeight() const { return height_; }

private:
    /// <summary>
    /// 調整した値をコライダーへ反映する（半径・高さを触ったときに呼ぶ）
    /// </summary>
    void ApplyShape();

    // 形の持ち主。押し戻しの計算もこれが持っている
    std::unique_ptr<Hagine::CylinderCollider> collider_;

    // --- 調整パラメータ ---
    Hagine::Vector3 center_ = {0.0f, 0.0f, 0.0f}; // 円の中心（yは線を描く底の高さ）
    float radius_ = 50.0f;                        // 半径
    float height_ = 20.0f;                        // 線を描く高さ（判定はXZだけなので見た目用）
    Hagine::Vector4 lineColor_ = {0.2f, 0.9f, 1.0f, 1.0f}; // 外周の線の色
    int ringCount_ = 4;                           // 縦に並べる輪の枚数（上下の輪を含む）
    bool isLineVisible_ = true;                   // 線を出すか

    // 登録した調整パラメータは破棄時にまとめて解除する（ポインタ失効を防ぐ）
    Hagine::GameParamOwner params_{"Field"};
};
