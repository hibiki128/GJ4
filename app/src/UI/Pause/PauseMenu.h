#pragma once
#include "src/UI/Text/UiText.h"
#include <Easing.h>
#include <debug/param/GameParamHub.h>
#include <string>

/// <summary>
/// ポーズ画面。シングルトンなのでどのシーンからでも同じものを開ける。
///
/// 使い方（各シーン）:
///   Initialize() … PauseMenu::GetInstance()->Initialize() と、UIレイヤーへ Draw() の登録
///   Update()     … PauseMenu::GetInstance()->Update() を先頭で呼び、
///                  IsPaused() が true の間はゲーム側の更新を飛ばす
///
/// 開閉はコントローラーのメニュー（START）ボタン、キーボードは ESC。
/// 開くときはイージングでスケールを小さい状態から立ち上げる。
/// </summary>
class PauseMenu
{
public:
    /// <summary>
    /// シングルトンインスタンスの取得
    /// </summary>
    static PauseMenu *GetInstance();

    /// <summary>
    /// UIスプライトを生成する（2回目以降の呼び出しは何もしない）。
    /// 文字テクスチャの書き出しが走るのでシーンの初期化中に呼ぶこと
    /// </summary>
    void Initialize();

    /// <summary>
    /// 入力と開閉アニメーションを進める。毎フレーム1回だけ呼ぶ
    /// </summary>
    void Update();

    /// <summary>
    /// 描画する。DrawSystem の UI レイヤー（DrawLayer::PostEffect）から呼ぶ
    /// </summary>
    void Draw();

    /// <summary>
    /// デバッグ用UIを描画する（「シーン設定」ウィンドウの中に差し込む想定なので Begin しない）。
    /// ImGui無しのビルドでは何もしない
    /// </summary>
    void DrawImGui();

    /// <summary>
    /// 開く（開くアニメーションから始まる）
    /// </summary>
    void Open();

    /// <summary>
    /// 閉じる（閉じるアニメーションから始まる）
    /// </summary>
    void Close();

    /// <summary>
    /// アニメーションなしで閉じた状態へ戻す。シーンを切り替えるときに使う
    /// </summary>
    void CloseImmediately();

    /// <summary>
    /// ポーズ中か（閉じるアニメーション中も true）
    /// </summary>
    /// <returns>bool: ポーズ中なら true</returns>
    bool IsPaused() const { return state_ != State::Closed; }

private:
    PauseMenu() = default;
    ~PauseMenu() = default;
    PauseMenu(const PauseMenu &) = delete;
    PauseMenu &operator=(const PauseMenu &) = delete;

    /// <summary>
    /// 開閉の状態
    /// </summary>
    enum class State
    {
        Closed,  // 閉じている
        Opening, // 開くアニメーション中
        Open,    // 開いている
        Closing, // 閉じるアニメーション中
    };

    /// <summary>
    /// 表示中のページ
    /// </summary>
    enum class Page
    {
        Root,     // ゲームに戻る／設定／タイトルに戻る
        Settings, // 音量とコントローラーの設定
    };

    /// <summary>
    /// ルートページの項目
    /// </summary>
    enum RootItem
    {
        kRootResume = 0,   // ゲームに戻る
        kRootSettings,     // 設定
        kRootReturnTitle,  // タイトルに戻る
        kRootItemCount,
    };

    /// <summary>
    /// 設定ページの項目。kSettingBack より前が「値を持つ行」
    /// </summary>
    enum SettingItem
    {
        kSettingMasterVolume = 0, // 全体の音量
        kSettingSensitivity,      // スティック感度
        kSettingVibration,        // 振動の有無
        kSettingBack,             // 戻る
        kSettingItemCount,
    };

    /// <summary>
    /// 押しっぱなしでリピートする入力。カーソル移動と値の増減に使う
    /// </summary>
    struct RepeatInput
    {
        bool isHeld = false;  // 前フレームで押されていたか
        float timer = 0.0f;   // 次に反応するまでの残り秒数

        /// <summary>
        /// 状態を進めて「今フレーム反応するか」を返す
        /// </summary>
        bool Update(bool held, float deltaTime);
    };

    /// <summary>
    /// このフレームのメニュー操作
    /// </summary>
    struct MenuInput
    {
        bool toggle = false; // ポーズの開閉
        bool up = false;     // カーソル上
        bool down = false;   // カーソル下
        bool left = false;   // 値を減らす
        bool right = false;  // 値を増やす
        bool decide = false; // 決定
        bool cancel = false; // 戻る
    };

    /// ===================================================
    /// private method
    /// ===================================================

    /// <summary>入力をまとめて取り出す</summary>
    MenuInput PollInput(float deltaTime);

    /// <summary>開閉アニメーションを進める</summary>
    void UpdateAnimation(float deltaTime);

    /// <summary>ルートページの操作</summary>
    void UpdateRootPage(const MenuInput &input);

    /// <summary>設定ページの操作</summary>
    void UpdateSettingsPage(const MenuInput &input);

    /// <summary>設定ページで値を増減する</summary>
    /// <param name="item">対象の項目</param>
    /// <param name="direction">-1 で減、+1 で増</param>
    void AdjustSetting(int item, int direction);

    /// <summary>ルートページを描く</summary>
    void DrawRootPage();

    /// <summary>設定ページを描く</summary>
    void DrawSettingsPage();

    /// <summary>設定1行分（ラベル・ゲージ・値）を描く</summary>
    void DrawSettingRow(int item, float localY, bool isSelected);

    /// <summary>
    /// 板や配置に掛けるスケール。開閉アニメーションのスケールと、
    /// GameParam から触れる全体の大きさを掛け合わせたもの
    /// </summary>
    float DrawScale() const { return scale_ * uiScale_; }

    /// <summary>
    /// 文字に掛けるスケール。DrawScale() に文字だけの倍率を更に掛けたもの。
    /// 位置は DrawScale() のままなので、行の並びを保ったまま文字だけ大きくできる
    /// </summary>
    float TextScale() const { return DrawScale() * textScale_; }

    /// <summary>パネル内のローカル座標を画面座標へ変換する（描画スケールが掛かる）</summary>
    Hagine::Vector2 ToScreen(float localX, float localY) const;

    /// <summary>板を1枚描く（ローカル座標・ローカルサイズ指定）</summary>
    void DrawPanelRect(float localX, float localY, float width, float height, const Hagine::Vector4 &color);

    /// <summary>選択中の項目を少し脈打たせるための倍率</summary>
    float SelectionPulse() const;

    /// <summary>カーソルを動かしたときのフィードバック</summary>
    void OnCursorMoved();

    /// <summary>決定したときのフィードバック</summary>
    void OnDecided();

    /// ===================================================
    /// private variables
    /// ===================================================

    // ----- 状態 -----
    State state_ = State::Closed;
    Page page_ = Page::Root;
    int rootIndex_ = 0;    // ルートページの選択位置
    int settingIndex_ = 0; // 設定ページの選択位置
    float elapsed_ = 0.0f; // 開いてからの経過秒数（脈動の位相に使う）

    // ----- 開閉アニメーション -----
    Hagine::EasingData<float> scaleEase_; // パネルのスケール
    float scale_ = 0.0f;                  // 現在のスケール
    float fade_ = 0.0f;                   // 背景の暗幕の濃さ (0〜1)

    // ----- 見た目の調整（GameParamHub から触る）-----
    // ポーズ画面のスプライト全部に一律で掛かる倍率。位置も大きさもまとめて変わる
    float uiScale_ = 1.0f;
    // 文字にだけ追加で掛かる倍率。板やゲージの大きさは変わらない
    float textScale_ = 1.0f;
    Hagine::GameParamOwner params_{"UI/ポーズメニュー"};

    // ----- 入力のリピート -----
    RepeatInput repeatUp_;
    RepeatInput repeatDown_;
    RepeatInput repeatLeft_;
    RepeatInput repeatRight_;

    // ----- 描画部品 -----
    // 板は1フレームに十数枚並べるので、枚数ぶんスプライトを確保しておく
    static constexpr int kRectCapacity = 32;   // 板の最大枚数
    static constexpr int kNumberCapacity = 24; // 数値の最大文字数

    GameUi::UiRect rects_;                                // 単色の板（暗幕・パネル・ゲージ）
    GameUi::UiText titleRoot_;                            // "PAUSED"
    GameUi::UiText titleSettings_;                        // "OPTIONS"
    GameUi::UiText rootItems_[kRootItemCount];            // ルートページの項目
    GameUi::UiText settingLabels_[kSettingItemCount];     // 設定ページの項目
    GameUi::UiText valueOn_;                              // "ON"
    GameUi::UiText valueOff_;                             // "OFF"
    GameUi::UiText hintRoot_;                             // ルートページの操作説明
    GameUi::UiText hintSettings_;                         // 設定ページの操作説明
    GameUi::UiNumber number_;                             // 設定値の数値表示

    bool isInitialized_ = false; // Initialize 済みか
};
