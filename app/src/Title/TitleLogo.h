#pragma once
#include "src/Boss/Data/BossColorPalette.h"
#include "src/Character/Player/Components/Reaction/PlayerComponentReaction.h"
#include "src/UI/Text/UiText.h"
#include "debug/param/GameParamHub.h"
#include "type/Vector2.h"
#include "type/Vector4.h"
#include <array>
#include <vector>

/// <summary>
/// タイトルロゴ「からぽっぷ」。
///
/// 1文字ずつ別のテクスチャに焼いて、1文字ずつ動かしている。
/// 5文字をまとめて1枚に焼くと文字ごとに時間差をつけられないので、
/// 文字数ぶんスプライトを持つ形にしてある。
/// 焼くのはフォントの縦の基準（ベースライン）が全文字で同じ位置に来る作りなので、
/// 同じ高さ・同じY座標で並べれば、字面は自然にそろう。
///
/// 動きはこのゲームの手ざわりに寄せて3つ重ねている:
///   ・登場  … 左から順に落ちてきて、着地でぷにっと潰れる
///   ・待機  … プレイヤーと同じ「ぷるぷる」が左から右へ波になって流れる
///   ・ポップ… ときどき左から順に1文字ずつはじける。はじけた瞬間だけ
///             ボスの殻と同じ色に染まる（撃って弾けるあの色）
///
/// 潰れ方・着地・呼吸は、プレイヤーの演出コンポーネント
/// （PlayerComponentReaction）を文字ごとに1つずつ持って任せている。
/// 自前で似た式を書くとゲーム中と手ざわりがずれるため。
/// </summary>
class TitleLogo {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>文字ごとのテクスチャを焼く（シーンの初期化から1回だけ。PNGを書き出すので重い）</summary>
    void Init();

    /// <summary>調整パラメータをデバッグUIへ登録する（Init の後に一度だけ）</summary>
    void RegisterParams();

    /// <summary>動きを進める</summary>
    /// <param name="deltaTime">前フレームからの経過（秒）</param>
    void Update(float deltaTime);

    /// <summary>描く（スプライトのレイヤーから毎フレーム）</summary>
    void Draw();

    /// <summary>調整UI（シーンの「オブジェクト設定」窓から呼ぶ）</summary>
    void DrawImGui();

    /// <summary>登場からやり直す</summary>
    void Restart();

    /// <summary>はじけたときに使う色マスタを受け取る（ボスの殻と同じ色にするため）</summary>
    void SetPalette(const BossColorPalette &palette, const std::vector<Color> &usedColors);

private:
    /// ===================================================
    /// private method
    /// ===================================================

    /// <summary>1文字ぶんの状態</summary>
    struct Glyph {
        GameUi::UiText text{};                 // 焼いた文字のスプライト
        PlayerComponentReaction reaction{};    // 潰れ・伸びの計算（ゲーム中と同じもの）
        bool hasLanded = false;                // 着地済みか（着地のぷにっを1回だけ出すため）
        float popTime = -1.0f;                 // はじけてからの経過（負なら非再生）
        Color colorId = Color::RED;            // はじけたときに乗る色
    };

    /// <summary>登場の落下ぶんのYずれと不透明度を求める</summary>
    /// <param name="index">何文字目か</param>
    /// <param name="outAlpha">不透明度（出力）</param>
    /// <returns>float: 定位置からのYのずれ（負で上）</returns>
    float CalcDropOffset(int index, float &outAlpha) const;

    /// <summary>はじけ具合（0〜1）。立ち上がりが速く、戻りがゆっくり</summary>
    /// <param name="glyph">対象の文字</param>
    static float CalcPopAmount(const Glyph &glyph, float popDuration);

    /// ===================================================
    /// private variables
    /// ===================================================

    // ロゴの文字。UTF-8で1文字ずつ持つ（焼くのも並べるのもこの単位）
    static constexpr int kCharCount = 5;
    static constexpr const char *kChars[kCharCount] = {"か", "ら", "ぽ", "っ", "ぷ"};

    // 文字ごとのはじける強さ。「ぷ」が言い切りなので、そこをいちばん大きくはじけさせる
    static constexpr float kPopStrength[kCharCount] = {0.9f, 0.9f, 1.1f, 0.7f, 1.35f};

    std::array<Glyph, kCharCount> glyphs_{};
    BossColorPalette palette_{};      // 色マスタ
    std::vector<Color> usedColors_{}; // ボスが使っている色（はじけたときに乗る）

    float elapsed_ = 0.0f;   // 登場が始まってからの経過（秒）
    float popTimer_ = 0.0f;  // 次のひと巡りまでの待ち（秒）
    float waveTime_ = -1.0f; // ひと巡りの経過（負なら非再生）

    // --- 調整パラメータ（デバッグUIから触って保存できる）---
    Hagine::Vector2 position_ = {840.0f, 118.0f}; // ロゴの中心（画面は1760x990）
    float charHeight_ = 380.0f;                   // 1文字の高さ（ピクセル）
    float tracking_ = -54.0f;                     // 字間の詰め（負で詰まる）
    Hagine::Vector4 baseColor_ = {1.0f, 1.0f, 1.0f, 1.0f}; // 文字の色（フチは黒のまま）

    // 登場
    float dropHeight_ = 420.0f;   // どこから落ちてくるか（上へのずれ）
    float dropTime_ = 0.42f;      // 落ちきるまでの時間（秒）
    float dropInterval_ = 0.11f;  // 1文字ずつずらす間隔（秒）

    // 待機（プレイヤーの「ぷるぷる」と同じ要求の出し方）
    float idleAmplitude_ = 0.07f; // 潰れる量
    float idlePeriod_ = 1.6f;     // 1往復にかける時間（秒）
    float idleSharpness_ = 0.25f; // 潰れ方の鋭さ
    float idleWave_ = 0.14f;      // 1文字ぶんの位相のずれ（波が左から右へ流れる）
    float idleBob_ = 9.0f;        // 上下に漂う量（ピクセル）
    float tiltDegrees_ = 3.2f;    // 傾きの振れ幅（度）
    float tiltPeriod_ = 3.4f;     // 傾きの1往復（秒）

    // ポップ
    float popInterval_ = 3.2f;  // ひと巡りの間隔（秒）
    float popStagger_ = 0.09f;  // 1文字ずつずらす間隔（秒）
    float popDuration_ = 0.42f; // 1文字がはじけて戻るまで（秒）
    float popScale_ = 0.22f;    // ふくらむ量（0.22 で 1.22倍）
    float popHop_ = 26.0f;      // 跳ね上がる量（ピクセル）
    float popTilt_ = 7.0f;      // はじけたときのひねり（度）
    float popColorMix_ = 0.85f; // 色の乗り具合（1で殻の色そのもの）

    Hagine::GameParamOwner params_{"Title/Logo"};
};
