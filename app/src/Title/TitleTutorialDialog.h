#pragma once
#include "src/UI/Text/UiText.h"
#include "type/Vector2.h"
#include "type/Vector4.h"

/// <summary>
/// タイトルで A を押したときに出す「チュートリアルをプレイしますか？」の問いかけ。
///
/// 板の見た目はチュートリアルのテロップに合わせてある（暗い板＋左の色帯）。
/// どちらを選んでいるかは、色の反転・大きさ・脈打ちの3つで見せる。
/// 色だけだと明るさの近い画面では見分けづらいため。
///
/// 選ぶのは左スティックか十字キーの左右、決めるのは A。
/// スティックは倒しっぱなしでも1回しか動かない（倒し切った瞬間だけ見る）ので、
/// 2択が行ったり来たりしない。
///
/// 決まってもすぐには返さない。選んだほうが光るのを見せてから返すので、
/// 「何を選んだか分からないまま画面が変わった」にならない。
/// </summary>
class TitleTutorialDialog {
public:
    /// ===================================================
    /// public types
    /// ===================================================

    /// <summary>選ばれた答え</summary>
    enum class Result {
        None, // まだ決まっていない
        Yes,  // する（チュートリアルへ）
        No,   // しない（本編へ）
    };

    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>板と文字を作り、保存してある調整値を読み戻す（シーンの初期化から1回だけ）</summary>
    void Init();

    /// <summary>いまの調整値を Assets/jsons/Title/TutorialDialog.json へ書き出す</summary>
    void Save() const;

    /// <summary>問いかけを開く</summary>
    void Open();

    /// <summary>閉じる（答えは返さない）</summary>
    void Close();

    /// <summary>出ているか</summary>
    bool IsOpen() const { return state_ != State::Closed; }

    /// <summary>
    /// 進める
    /// </summary>
    /// <param name="deltaTime">前フレームからの経過（秒）</param>
    /// <returns>Result: 決まった答え。決まったフレームに1回だけ返す</returns>
    Result Update(float deltaTime);

    /// <summary>描く（スプライトのレイヤーから毎フレーム）</summary>
    void Draw();

    /// <summary>調整UI（シーンの「オブジェクト設定」窓から呼ぶ）</summary>
    void DrawImGui();

private:
    /// ===================================================
    /// private types
    /// ===================================================

    /// <summary>今の状態</summary>
    enum class State {
        Closed,   // 出ていない
        Opening,  // 開いている最中（この間は操作を受け付けない）
        Choosing, // 選んでもらっている
        Decided,  // 決まった（選んだほうを見せている最中）
    };

    /// <summary>このフレームの操作</summary>
    struct DialogInput {
        bool left = false;   // 左を選ぶ
        bool right = false;  // 右を選ぶ
        bool decide = false; // 決める
        bool cancel = false; // やめる
    };

    /// ===================================================
    /// private method
    /// ===================================================

    /// <summary>保存してある調整値を読み戻す（無ければコードの既定値のまま）</summary>
    void Load();

    /// <summary>操作をまとめて取り出す（左右は倒し切った瞬間だけ拾う）</summary>
    DialogInput PollInput();

    /// <summary>選んでいるほうを脈打たせる倍率</summary>
    float SelectionPulse() const;

    /// ===================================================
    /// private variables
    /// ===================================================

    static constexpr int kOptionCount = 2; // する / しない

    State state_ = State::Closed;
    int selectedIndex_ = 0;       // 0=する 1=しない
    float stateTimer_ = 0.0f;     // いまの状態になってからの経過（秒）
    float elapsed_ = 0.0f;        // 開いてからの経過（脈打ちに使う）
    int axisDirection_ = 0;       // 前フレームの倒し向き（-1/0/+1。倒しっぱなし対策）
    bool isResultReported_ = false; // 答えを返し終えたか

    // 表示部品
    GameUi::UiRect rects_{};                     // 暗幕・板・選択肢の枠
    GameUi::UiText title_{};                     // 問いかけ
    GameUi::UiText options_[kOptionCount]{};     // する / しない
    GameUi::UiText hintSelect_{};                // 「でえらぶ」
    GameUi::UiText hintDecide_{};                // 「できめる」
    GameUi::UiSprite glyphStick_{};              // 左スティックの図
    GameUi::UiSprite glyphDpad_{};               // 十字キーの図
    GameUi::UiSprite glyphDecide_{};             // Aボタンの図

    // --- 調整パラメータ（デバッグUIから触って保存できる）---
    Hagine::Vector2 panelCenter_ = {880.0f, 470.0f}; // 板の中心（画面は1760x990）
    Hagine::Vector2 panelSize_ = {900.0f, 360.0f};   // 板の大きさ
    Hagine::Vector2 optionBoxSize_ = {260.0f, 120.0f}; // 選択肢1つぶんの枠
    float optionGap_ = 60.0f;   // 選択肢どうしの間
    float titleSize_ = 80.0f;   // 問いかけの文字の大きさ
    float optionSize_ = 96.0f;  // 選択肢の文字の大きさ
    float hintSize_ = 40.0f;    // 操作説明の文字の大きさ
    float glyphSize_ = 48.0f;   // 操作説明の図の大きさ
    float openTime_ = 0.22f;    // 開ききるまでの時間（秒）
    float decideTime_ = 0.45f;  // 決めてから答えを返すまで（秒。選んだほうを見せる間）
};
