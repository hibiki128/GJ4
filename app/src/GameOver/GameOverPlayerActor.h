#pragma once
#include "src/Boss/Data/BossColorPalette.h"
#include "debug/param/GameParamHub.h"
#include "object/base/BaseObject.h"
#include <random>
#include <string>

/// <summary>
/// ゲームオーバー画面で倒れているプレイヤー役。
///
/// クリア画面の ClearPlayerActor と対になるクラス。あちらが跳ねて喜ぶのに対し、
/// こちらは地面に潰れたまま、少しだけ戻ってはまた潰れる「ぷにぷに」を繰り返し、
/// ときどき力なくぴくりと動く。傾けはしない（潰れた形だけで倒れて見せる）。
/// Player クラスには入力も体力もぶら下がっていて演出には要らないので、
/// 見た目（同じスライムのモデル・同じ色マスタ）だけを借りている。
/// </summary>
class GameOverPlayerActor final : public Hagine::BaseObject {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>モデルを作って倒れた姿勢にする</summary>
    /// <param name="objectName">オブジェクト名</param>
    void Init(const std::string objectName) override;

    /// <summary>ぷにぷにとひくつきを進める</summary>
    void Update() override;

    /// <summary>調整パラメータをデバッグUIへ登録する（Init の後に一度だけ）</summary>
    void RegisterParams();

    /// <summary>色マスタを受け取る（ゲーム中のプレイヤーと同じ色に見せるため）</summary>
    void SetPalette(const BossColorPalette &palette) { palette_ = palette; }

    /// <summary>倒れている色を決める</summary>
    void SetColorId(Color color) { colorId_ = color; }

    /// <summary>倒れている場所を決める</summary>
    void SetGroundPosition(const Hagine::Vector3 &position) { groundPosition_ = position; }

    /// <summary>倒れている場所を取得する</summary>
    const Hagine::Vector3 &GetGroundPosition() const { return groundPosition_; }

private:
    /// ===================================================
    /// private variables
    /// ===================================================

    BossColorPalette palette_{};  // 色マスタ（シーンから渡される）
    Color colorId_ = Color::RED;  // 出している色

    Hagine::Vector3 groundPosition_ = {0.0f, 0.0f, 0.0f}; // 倒れている場所（足元）
    float puniTime_ = 0.0f;        // ぷにぷにの経過時間（秒）
    float twitchTimer_ = 1.5f;     // 次にひくつくまでの残り時間（秒）
    float twitchTime_ = -1.0f;     // ひくつきの経過時間（負なら再生していない）
    std::mt19937 random_{20260909}; // ひくつきの間隔をばらす（毎回同じ並びで再現する）

    // --- 調整パラメータ ---
    // 潰れ具合は 0 で丸いまま・大きいほど平たい。ずっと潰れきったままだと死んで見えるので、
    // 「潰れきり」と「少し戻ったところ」のあいだをゆっくり行き来させる
    Hagine::Vector3 baseScale_ = {1.0f, 1.0f, 1.0f}; // 大きさの基準
    float flattenDeep_ = 0.50f;    // いちばん潰れているときの潰れ具合
    float flattenEase_ = 0.28f;    // いちばん戻ったときの潰れ具合
    float puniPeriod_ = 3.6f;      // 潰れて戻ってくるまでの周期（秒）。大きいほどゆっくり
    float puniJiggle_ = 0.12f;     // 戻りぎわ・潰れぎわの揺り返し（これが「ぷにぷに」の正体）
    float twitchIntervalMin_ = 2.0f; // ひくつきの間隔の下限（秒）
    float twitchIntervalMax_ = 5.0f; // ひくつきの間隔の上限（秒）
    float twitchDuration_ = 0.35f; // ひとつのひくつきの長さ（秒）
    float twitchAmount_ = 0.12f;   // ひくつきで跳ねる量

    // 登録した調整パラメータは破棄時にまとめて解除する
    Hagine::GameParamOwner params_{"GameOver/Player"};
};
