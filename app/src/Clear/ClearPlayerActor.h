#pragma once
#include "src/Boss/Data/BossColorPalette.h"
#include "src/Character/Player/Components/Reaction/PlayerComponentReaction.h"
#include "debug/param/GameParamHub.h"
#include "object/base/BaseObject.h"
#include <string>

/// <summary>
/// クリア画面で喜んでいるプレイヤー役。
///
/// 見た目は Player と同じスライムのモデル・同じ色マスタを使うが、中身は入れていない。
/// クリア画面に入力も体力も射撃も要らないので、Player クラスをそのまま置く代わりに
/// 「跳ねて・ぷよぷよする」だけの演出用オブジェクトとして作ってある。
/// ぷにぷにの計算だけは Player と同じ PlayerComponentReaction を使い回すので、
/// 潰れ方・伸び方はゲーム中のプレイヤーとそろう。
///
/// 動きは「軽く跳ねる → 着地でぷにっと潰れる」の繰り返し。
/// 跳ねていない間もループの伸縮を出し続けるので、止まって見える瞬間が無い。
/// </summary>
class ClearPlayerActor final : public Hagine::BaseObject {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>モデルを作って喜びの動きを始める</summary>
    /// <param name="objectName">オブジェクト名</param>
    void Init(const std::string objectName) override;

    /// <summary>跳ねとぷにぷにを進める</summary>
    void Update() override;

    /// <summary>調整パラメータをデバッグUIへ登録する（Init の後に一度だけ）</summary>
    void RegisterParams();

    /// <summary>色マスタを受け取る（ボスと同じ赤・青…に見せるため）</summary>
    void SetPalette(const BossColorPalette &palette) { palette_ = palette; }

    /// <summary>喜んでいる色を決める</summary>
    void SetColorId(Color color) { colorId_ = color; }

    /// <summary>立ち位置を決める（跳ねる高さはここからの相対で決まる）</summary>
    void SetHomePosition(const Hagine::Vector3 &position) { homePosition_ = position; }

    /// <summary>立ち位置を取得する</summary>
    const Hagine::Vector3 &GetHomePosition() const { return homePosition_; }

private:
    /// ===================================================
    /// private method
    /// ===================================================

    /// <summary>跳ねの高さを進め、着地した瞬間にぷにっを一発入れる</summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    void UpdateHop(float deltaTime);

    /// ===================================================
    /// private variables
    /// ===================================================

    BossColorPalette palette_{};        // 色マスタ（シーンから渡される）
    Color colorId_ = Color::RED;        // 出している色
    PlayerComponentReaction reaction_{}; // ぷにぷに（Player と同じもの）

    Hagine::Vector3 homePosition_ = {0.0f, 0.0f, 0.0f}; // 立ち位置（足元）
    float hopTime_ = 0.0f;   // 今の跳ねが始まってからの経過時間（秒）
    float faceYaw_ = 0.0f;   // 向き（ラジアン）

    // --- 調整パラメータ ---
    Hagine::Vector3 baseScale_ = {1.0f, 1.0f, 1.0f}; // 大きさの基準（ぷにぷにはここを中心に揺れる）
    float hopHeight_ = 0.9f;      // 跳ねの高さ
    float hopDuration_ = 0.55f;   // ひと跳ねにかける時間（秒）
    float hopInterval_ = 0.25f;   // 着地してから次に跳ぶまでの間（秒）
    float landStrength_ = 0.7f;   // 着地のぷにっの強さ（0〜1）
    float spinPerHop_ = 35.0f;    // ひと跳ねで回る角度（度）。喜んでくるくる回る
    float idleLoopAmp_ = 0.06f;   // 跳ねていない間の伸縮の大きさ
    float idleLoopPeriod_ = 0.9f; // その周期（秒）

    // 登録した調整パラメータは破棄時にまとめて解除する
    Hagine::GameParamOwner params_{"Clear/Player"};
};
