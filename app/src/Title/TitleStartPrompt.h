#pragma once
#include "src/UI/Text/UiText.h"
#include "debug/param/GameParamHub.h"
#include "type/Vector2.h"

/// <summary>
/// タイトル画面の中央下に出す「Aボタン」の案内。
///
/// 透明度だけをゆっくり上げ下げして、押せることを伝える。
/// 明滅は cos で作ってあるので、いちばん濃いところ・薄いところで
/// 折り返しても動きが途切れない（三角波だと折り返しが目に付く）。
///
/// ロゴが落ちきってから現れる。ロゴと同時に出すと、
/// 落ちてくる文字に目が行っているあいだに明滅が始まってしまい、
/// 案内として見てもらえないため。
/// </summary>
class TitleStartPrompt {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>絵を読み込む（シーンの初期化から1回だけ）</summary>
    void Init();

    /// <summary>調整パラメータをデバッグUIへ登録する（Init の後に一度だけ）</summary>
    void RegisterParams();

    /// <summary>明滅を進める</summary>
    /// <param name="deltaTime">前フレームからの経過（秒）</param>
    void Update(float deltaTime);

    /// <summary>描く（スプライトのレイヤーから毎フレーム）</summary>
    void Draw();

    /// <summary>調整UI（シーンの「オブジェクト設定」窓から呼ぶ）</summary>
    void DrawImGui();

    /// <summary>出はじめからやり直す</summary>
    void Restart() { elapsed_ = 0.0f; }

private:
    /// ===================================================
    /// private variables
    /// ===================================================

    GameUi::UiSprite sprite_{}; // Aボタンの絵
    float elapsed_ = 0.0f;      // 出はじめてからの経過（秒）

    // --- 調整パラメータ（デバッグUIから触って保存できる）---
    Hagine::Vector2 position_ = {880.0f, 848.0f}; // 置き場所（画面は1760x990）

    // 大きさは「もとの大きさ」と「倍率」に分けてある。
    // 縦横を別々に決めたいとき（絵を縦長にしたい等）は size_、
    // 形はそのままで大小だけ変えたいときは scale_ を触る。
    // ボスの大きさと同じ考え方にそろえてある
    Hagine::Vector2 size_ = {96.0f, 96.0f}; // もとの大きさ（px。元絵は256x256の正方形）
    float scale_ = 1.0f;                    // 倍率（縦横同時）

    float alphaMin_ = 0.25f;     // いちばん薄いときの濃さ
    float alphaMax_ = 1.0f;      // いちばん濃いときの濃さ
    float period_ = 1.6f;        // 薄い→濃い→薄いの1往復（秒）
    float appearDelay_ = 1.0f;   // 出てくるまでの待ち（秒。ロゴが落ちきるのが0.86秒）
    float fadeInTime_ = 0.5f;    // 出てくるのにかける時間（秒）

    Hagine::GameParamOwner params_{"Title/StartPrompt"};
};
