#pragma once
#include "src/Boss/Data/BossColorPalette.h"
#include "src/Character/Player/Components/Reaction/PlayerComponentReaction.h"
#include "object/base/BaseObject.h"
#include <string>

/// <summary>
/// タイトル画面に立っているプレイヤー役。
///
/// 「ゲーム中と同じ揺れ方」を見せたいので、揺れの計算はゲーム中と同じ
/// PlayerComponentReaction をそのまま使っている。待機ステートが毎フレーム出している
/// 要求（SetLoop）と同じ値をここからも出すので、タイトルとゲーム中で動きがそろう。
///
/// Player クラスそのものは使わない。入力・体力・射撃・弾のプール・フィールドの
/// 押し戻しまで付いてきて、タイトルには要らないものばかりのため。
/// 借りているのは見た目（同じスライムのモデル・同じ色マスタ）と揺れの計算だけ。
/// クリア／ゲームオーバー画面のプレイヤー役と同じ立て付けにしてある
/// </summary>
class TitlePlayerActor final : public Hagine::BaseObject {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>モデルを作って待機の姿勢にする</summary>
    /// <param name="objectName">オブジェクト名</param>
    void Init(const std::string objectName) override;

    /// <summary>揺れを進める</summary>
    void Update() override;

    /// <summary>調整パラメータをデバッグUIへ登録する（Init の後に一度だけ）</summary>
    void RegisterParams();

    /// <summary>色マスタを受け取る（ゲーム中のプレイヤーと同じ色に見せるため）</summary>
    void SetPalette(const BossColorPalette &palette) { palette_ = palette; }

    /// <summary>出している色を決める</summary>
    void SetColorId(Color color) { colorId_ = color; }

    /// <summary>立っている場所を決める（足元の高さ）</summary>
    void SetGroundPosition(const Hagine::Vector3 &position) { groundPosition_ = position; }

    /// <summary>向く先を決める（ボスのほうを向かせる）</summary>
    void LookAt(const Hagine::Vector3 &worldPoint);

private:
    /// ===================================================
    /// private variables
    /// ===================================================

    BossColorPalette palette_{}; // 色マスタ（シーンから渡される）
    Color colorId_ = Color::RED; // 出している色

    Hagine::Vector3 groundPosition_ = {0.0f, 0.0f, 0.0f}; // 立っている場所（足元）
    Hagine::Vector3 baseScale_ = {1.0f, 1.0f, 1.0f};      // 揺れの中心になる大きさ

    // 揺れの計算はゲーム中と同じものを使う
    PlayerComponentReaction reaction_{};

    // --- 調整パラメータ（ゲーム中の待機と同じ意味の値）---
    float idleAmplitude_ = 0.05f;  // 潰れる量
    float idlePeriod_ = 1.0f;      // 1往復にかける時間（秒）
    float idleSharpness_ = 0.2f;   // 潰れ方の鋭さ
    float idlePhase_ = 0.0f;       // 揺れの位相（ボスとずらすのに使う）
};
