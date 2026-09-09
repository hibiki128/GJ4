#pragma once
#include "src/Boss/Data/BossColorPalette.h"
#include "src/Character/ColorStruct.h"
#include "src/UI/Text/UiText.h"
#include "debug/param/GameParamHub.h"
#include "type/Vector2.h"
#include "type/Vector4.h"

/// <summary>
/// HUD が毎フレーム受け取る値。
///
/// HUD は Player も Boss も知らない。シーンがここへ詰めて渡すので、
/// 表示したいものが増えても他へ影響しない
/// </summary>
struct GameHudSnapshot {
    // --- プレイヤー ---
    int hp = 0;                        // 残りHP（ハートの数）
    int maxHp = 0;                     // 最大HP
    Color playerColor = Color::RED;    // いま選んでいる色
    int ammo = 0;                      // 選んでいる色の残弾
    int maxAmmo = 0;                   // 弾の上限

    // --- 補給（回復エリアに乗っているあいだ）---
    // エリアの色は選んでいる色とは限らない。選んでいる色しか出していないと
    // 「戻っているのかどうか」が画面から分からないので、戻っている色を別に出す
    bool reloadActive = false;         // いま補給中か（エリアに乗っているか）
    Color reloadColor = Color::RED;    // 戻っている色（エリアの色）
    int reloadAmmo = 0;                // その色の残弾

    // --- ボス ---
    // 体力は「まとっている球の数」。撃った球がくっついたときは増えるので、
    // そのぶんバーも伸びる（仕様どおり）
    float bossHp = 0.0f;
    float bossMaxHp = 0.0f;
    bool bossVisible = false; // ボスを出しているか（出していないならバーを隠す）
};

/// <summary>
/// ゲーム中のHUD。
///
/// 置くものは4つ:
///   ・左上   … プレイヤーの体力（ハート）。いま選んでいる色に染まる
///   ・上中央 … ボスの体力バー。名前を入れる余白を上に空けてある
///   ・左下   … 色の選択。十字ボタンの図の上に、前・今・次の3色が浅い弧に並ぶ。
///              色を変えると弧に沿って流れ、選んでいる色が真ん中の一番上へ来る。
///              残弾はその上に出る（全部の色は出さない。前後が分かれば足りる）
///   ・その上 … 補給中の色と残弾。回復エリアに乗っているあいだだけ出る。
///              エリアの色は選んでいる色とは限らないので、選択とは別に出さないと
///              戻っていることが画面から分からない
///
/// 触ったとき・食らったときに小さく反応する。反応はどれも「押された瞬間に1になって
/// 時間で0へ戻る値」を1つ持たせ、その値で拡大や揺れを作っている。
/// </summary>
class GameHud {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>表示部品を作る（シーンの初期化から1回だけ）</summary>
    /// <param name="palette">色マスタ（チップとハートの色に使う）</param>
    void Init(const BossColorPalette &palette);

    /// <summary>抱えているスプライトを解放する</summary>
    void Finalize();

    /// <summary>配置と大きさをデバッグUIへ登録する（Init の後に一度だけ）</summary>
    void RegisterParams();

    /// <summary>表示値と反応の演出を進める</summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    /// <param name="snapshot">今フレームの値</param>
    void Update(float deltaTime, const GameHudSnapshot &snapshot);

    /// <summary>描く（スプライトの描画フェーズから）</summary>
    void Draw();

    /// <summary>状態の確認用UI</summary>
    void DrawImGui();

    /// <summary>
    /// 体力（ハート）を出すか。チュートリアルのように、体力を見せたくない場面で切る
    /// </summary>
    void SetHeartsVisible(bool visible) { showHearts_ = visible; }

    /// <summary>ボスの体力バーを出すか</summary>
    void SetBossBarVisible(bool visible) { showBossBar_ = visible; }

    /// ===================================================
    /// 反応（起きた瞬間にシーンから呼ぶ）
    /// ===================================================

    /// <summary>被弾した（ハートが揺れて、減ったぶんが弾ける）</summary>
    void PlayDamaged();

    /// <summary>色を変えた（チップが回って弾む）</summary>
    void PlayColorChanged();

    /// <summary>撃った（残弾の数字が弾む）</summary>
    void PlayShot();

    /// <summary>ボスの球が減った（バーが光って揺れる）</summary>
    void PlayBossDamaged();

    /// <summary>補給で1発戻った（補給表示が弾む）</summary>
    void PlayReloadTick();

private:
    /// ===================================================
    /// private method
    /// ===================================================

    void DrawHearts();
    void DrawBossBar();
    void DrawColorPanel();
    void DrawReloadPanel();
    void DrawMenuButton();

    /// <summary>色マスタから引いた表示色</summary>
    /// <param name="color">色</param>
    /// <param name="alpha">不透明度</param>
    Hagine::Vector4 ColorOf(Color color, float alpha = 1.0f) const;

    /// ===================================================
    /// private variables
    /// ===================================================

    BossColorPalette palette_{};
    GameHudSnapshot snapshot_{};

    // --- 表示のための内部状態 ---
    float bossRatio_ = 0.0f;      // バーの表示値。実際の値へ追いつくように動かす
    float damageAnim_ = 0.0f;     // 被弾の反応（1→0）
    float colorAnim_ = 0.0f;      // 色替えの反応（1→0）
    float shotAnim_ = 0.0f;       // 射撃の反応（1→0）
    float bossHitAnim_ = 0.0f;    // ボス被弾の反応（1→0）
    float reloadTickAnim_ = 0.0f; // 補給で1発戻った反応（1→0）
    float reloadShow_ = 0.0f;     // 補給表示の出具合（0で隠れ、1で出きっている）
    int previousReloadAmmo_ = -1; // 前フレームの補給中の残弾（増えた瞬間を拾う。-1で未計測）
    int previousColorIndex_ = 0;  // 前フレームの色（回る向きを決めるのに使う）
    // 弧に沿って流れている量（-1〜1、0で落ち着いている）。
    // 色を変えた瞬間に ±1 になり、時間で 0 へ戻る＝1つぶん回ったように見える
    float arcSlide_ = 0.0f;
    float time_ = 0.0f;           // 揺れの位相に使う経過時間

    // --- 表示部品 ---
    static constexpr int kMaxHearts = 8;   // 描けるハートの上限
    static constexpr int kChipCount = 3; // 前・今・次の3つだけ出す
    static constexpr int kRectCapacity = 24;

    GameUi::UiRect rects_;
    GameUi::UiSprite hearts_[kMaxHearts];
    GameUi::UiSprite chips_[kChipCount];
    GameUi::UiSprite dpad_;
    GameUi::UiSprite triggerRt_;
    // 色送り（LB で前・RB で次）。十字ボタンと併用できることを図で示す
    GameUi::UiSprite buttonLb_;
    GameUi::UiSprite buttonRb_;
    // メニュー（ポーズ）
    GameUi::UiSprite buttonMenu_;
    GameUi::UiNumber ammoNumber_;
    // 補給中の色の球と残弾。色チップと数字は1フレームに1回しか描けないので別に持つ
    GameUi::UiSprite reloadChip_;
    GameUi::UiNumber reloadNumber_;
    GameUi::UiText reloadHint_;
    GameUi::UiText shootHint_;
    GameUi::UiText pauseHint_;
    bool isInitialized_ = false;

    // 出す・出さないの切り替え（チュートリアルでは体力まわりを伏せる）
    bool showHearts_ = true;
    bool showBossBar_ = true;

    // --- 調整パラメータ ---
    Hagine::Vector2 heartOrigin_ = {74.0f, 74.0f}; // 左上のハート1つ目の中心
    float heartSize_ = 66.0f;                      // ハートの高さ
    float heartSpacing_ = 74.0f;                   // ハートの間隔

    Hagine::Vector2 bossBarCenter_ = {880.0f, 96.0f}; // ボスのバーの中心
    float bossBarWidth_ = 760.0f;
    float bossBarHeight_ = 34.0f;
    float bossNameSpace_ = 46.0f; // バーの上に空ける、名前を入れるぶんの高さ

    Hagine::Vector2 colorPanelCenter_ = {190.0f, 748.0f}; // 十字ボタン（＝色の輪の中心）
    float chipSize_ = 66.0f;         // 選んでいる色（弧の真ん中）のチップの大きさ
    float chipSideScale_ = 0.66f;    // 前後の色の縮み具合
    float chipArcRadiusX_ = 88.0f;   // 弧の横の広がり
    float chipArcRadiusY_ = 34.0f;   // 弧の縦の膨らみ（横より小さいほど平たい弧になる）
    float chipArcAngleDeg_ = 62.0f;  // 真ん中から前後のチップまでの角度
    float dpadOffsetY_ = 84.0f;      // 弧の中心から十字ボタンの図までの下方向の距離
    float ammoTextSize_ = 34.0f;  // 残弾の数字の高さ
    float dpadSize_ = 72.0f;      // 十字ボタンの図の大きさ
    float hintSize_ = 26.0f;      // 「しゃげき」の文字の高さ
    float cycleButtonSize_ = 46.0f; // 色送り（LB/RB）の図の高さ
    float cycleButtonGap_ = 26.0f;  // チップの端から色送りの図までの間隔

    // --- 補給の表示。色の弧の上に出す ---
    // 位置は弧の中心からの差で持つ。弧ごと動かしても付いてくるようにするため
    Hagine::Vector2 reloadOffset_ = {0.0f, -146.0f}; // 弧の中心からの位置
    float reloadPanelWidth_ = 176.0f;  // 板の横幅
    float reloadPanelHeight_ = 64.0f;  // 板の高さ
    float reloadChipSize_ = 44.0f;     // 補給中の色の球の大きさ
    float reloadTextSize_ = 36.0f;     // 残弾の数字の高さ
    float reloadHintSize_ = 26.0f;     // 「かいふく」の文字の高さ

    // --- メニュー（ポーズ）。右下 ---
    Hagine::Vector2 menuCenter_ = {1560.0f, 906.0f}; // メニューの図の中心
    float menuSize_ = 62.0f;   // メニューの図の高さ
    float menuTextSize_ = 30.0f; // 「ぽーず」の文字の高さ
    float menuTextGap_ = 18.0f;  // 図と文字の間隔

    Hagine::GameParamOwner params_{"Hud"};
};
