#pragma once
#include "src/Boss/Data/BossColorPalette.h"
#include "src/UI/Text/UiText.h"
#include "debug/param/GameParamHub.h"
#include "type/Vector2.h"
#include "type/Vector4.h"
#include <string>
#include <vector>

/// <summary>
/// 見出しの文字の動き方
/// </summary>
enum class ResultTitleMotion {
    Bounce, // 1文字ずつ波のように弾む（クリア）
    Wobble, // 1文字ずつばらばらに傾いて沈む（ゲームオーバー）
};

/// <summary>
/// 結果画面（クリア・ゲームオーバー）の見出しと、次へ進む案内。
///
/// 見出しは1文字ずつ別のスプライトで持つ。1枚の絵にしてしまうと文字ごとに
/// 動かせず、波打たせたり傾きをばらつかせたりができない。
///
/// 案内は右下。項目が1つなら「押せば進む」だけ、2つ以上なら上下で選んでAで決める。
/// どちらを選んだかは番号で返すだけで、シーンの行き先はこのクラスは知らない。
/// </summary>
class ResultUi {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    static constexpr int kMaxTitleChars = 12; // 見出しに置ける文字数
    static constexpr int kMaxItems = 3;       // 案内に置ける項目数

    /// <summary>
    /// 表示部品を作る（シーンの初期化から1回だけ）
    /// </summary>
    /// <param name="idPrefix">生成する文字画像の名前の頭（シーンごとに変えること）</param>
    /// <param name="titleChars">見出しの文字。1文字ずつ入れる（"く","り","あ" のように）</param>
    /// <param name="itemLabels">案内の項目。上から順に並ぶ</param>
    /// <param name="motion">見出しの動き方</param>
    void Init(const std::string &idPrefix, const std::vector<std::string> &titleChars,
              const std::vector<std::string> &itemLabels, ResultTitleMotion motion);

    /// <summary>抱えているスプライトを解放する</summary>
    void Finalize();

    /// <summary>
    /// 配置と大きさをデバッグUIへ登録する（Init の後に一度だけ）
    /// </summary>
    /// <param name="ownerName">ゲームパラメータでの出所ラベル</param>
    void RegisterParams(const std::string &ownerName);

    /// <summary>動きを進め、入力を見る</summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    void Update(float deltaTime);

    /// <summary>描く（スプライトの描画フェーズから）</summary>
    void Draw();

    /// <summary>状態の確認用UI</summary>
    void DrawImGui();

    /// ===================================================
    /// getter / setter
    /// ===================================================

    /// <summary>
    /// 決まったか。押した瞬間ではなく、決定の演出を見せ終わってから true になる
    /// </summary>
    bool IsDecided() const { return decidedIndex_ >= 0 && decideTimer_ <= 0.0f; }

    /// <summary>選ばれた項目の番号（未決定なら -1）</summary>
    int GetDecidedIndex() const { return decidedIndex_; }

    /// <summary>
    /// 入力を受け付けるか。ポーズ中など、裏で選ばれては困る場面で切る。
    /// 切った状態から戻した直後は少しのあいだ受け付けない
    /// （ポーズを閉じたAをそのまま決定として拾ってしまうため）
    /// </summary>
    void SetInputEnabled(bool enabled);

private:
    /// ===================================================
    /// private method
    /// ===================================================

    void PollInput(float deltaTime);
    void DrawTitle();
    void DrawMenu();

    /// <summary>見出し1文字ぶんの色</summary>
    /// <param name="index">何文字目か</param>
    /// <param name="alpha">不透明度</param>
    Hagine::Vector4 TitleColorOf(int index, float alpha) const;

    /// ===================================================
    /// private variables
    /// ===================================================

    BossColorPalette palette_{}; // 見出しをゲームと同じ色で塗るために持つ
    ResultTitleMotion motion_ = ResultTitleMotion::Bounce;

    // --- 表示部品 ---
    static constexpr int kRectCapacity = 8;
    GameUi::UiRect rects_;
    GameUi::UiText titleChars_[kMaxTitleChars];
    GameUi::UiText items_[kMaxItems];
    GameUi::UiSprite buttonA_;
    int titleCharCount_ = 0;
    int itemCount_ = 0;
    bool isInitialized_ = false;

    // --- 状態 ---
    float time_ = 0.0f;         // 出てからの経過時間（秒）。出現と揺れに使う
    int cursor_ = 0;            // 選んでいる項目
    float cursorAnim_ = 0.0f;   // 選び直した瞬間の反応（1→0）
    int decidedIndex_ = -1;     // 決まった項目（-1で未決定）
    float decideTimer_ = 0.0f;  // 決定の演出を見せる残り時間（秒）
    float decideAnim_ = 0.0f;   // 決めた瞬間の反応（1→0）
    bool isInputEnabled_ = true;
    float inputWakeTimer_ = 0.0f; // 入力を受け付けるまでの残り（秒）
    bool previousStickUp_ = false;   // スティックは押した瞬間が取れないので前フレームと比べる
    bool previousStickDown_ = false;

    // --- 調整パラメータ ---
    Hagine::Vector2 titleCenter_ = {880.0f, 232.0f}; // 見出しの中心（画面中央の上の方）
    float titleSize_ = 200.0f;                       // 見出しの文字の高さ
    float titleSpacing_ = 1.0f;                      // 文字の間隔（1.0で字送りそのまま）
    // 並べた見出しがこの幅に収まらなければ、収まるところまで文字を縮める。
    // 文字数はシーンごとに違うので、大きさだけ決めても長い見出しが画面からはみ出す
    float titleMaxWidth_ = 1520.0f;

    Hagine::Vector2 menuAnchor_ = {1684.0f, 906.0f}; // 案内の右下の角
    float itemSize_ = 38.0f;      // 項目の文字の高さ
    float itemSpacing_ = 68.0f;   // 項目どうしの間隔
    float itemGlyphSize_ = 52.0f; // Aボタンの図の高さ
    float itemGlyphGap_ = 18.0f;  // 図と文字の間隔

    Hagine::GameParamOwner params_{};
};
