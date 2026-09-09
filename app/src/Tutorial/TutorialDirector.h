#pragma once
#include "src/UI/Text/UiText.h"
#include "debug/param/GameParamHub.h"
#include "type/Vector2.h"
#include "type/Vector4.h"

/// <summary>
/// チュートリアルが「今フレーム何が起きたか」を受け取るための入れ物。
///
/// チュートリアル側はプレイヤーにもボスにも直接触らない。シーンが毎フレーム
/// ここへ詰めて渡すので、判定に必要な情報が増えても他へ影響しない。
/// </summary>
struct TutorialSignals {
    Hagine::Vector2 moveStick{};   // 左スティックの倒し量
    Hagine::Vector2 lookStick{};   // 右スティックの倒し量
    bool jumped = false;           // ジャンプした瞬間
    bool dashing = false;          // ダッシュ中
    bool shot = false;             // 撃った瞬間
    int selectedColorIndex = -1;   // いま選んでいる色（-1なら分からない）
    bool chainCleared = false;     // 同色がそろって球が消えた瞬間
    bool inRecoveryZone = false;   // 弾の回復エリアに乗っているか
};

/// <summary>
/// チュートリアルの進行役。
///
/// 「やること」をチェックボックス付きのテロップで出し、実際にやれたら勝手にチェックが付く。
/// 段（ステージ）ごとに数個ずつ出し、その段が全部埋まったら次の段へ進む。
///
/// 判定に使う情報は TutorialSignals でシーンから渡してもらうので、
/// このクラスは Player も Boss も知らない。順番や条件を変えたいときは
/// このファイルの中だけで完結する。
///
/// 表示はゲーム内の他のUIと同じ部品（UiRect / UiText / UiSprite）で組んである。
/// コントローラーの図は Assets/images/Tutorial/ の画像で、ゲームの絵に合わせて
/// ベタ塗り＋黒フチにしてある。
/// </summary>
class TutorialDirector {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>テロップの部品を作る（シーンの初期化から1回だけ）</summary>
    void Init();

    /// <summary>抱えているスプライトを解放する（シーンの終了処理から）</summary>
    void Finalize();

    /// <summary>
    /// 文字やテロップの大きさをデバッグUIへ登録する（Init の後に一度だけ）。
    /// 調整は ゲームパラメータ の Tutorial から行う
    /// </summary>
    void RegisterParams();

    /// <summary>
    /// 進行を進める
    /// </summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    /// <param name="signals">今フレームに起きたこと</param>
    void Update(float deltaTime, const TutorialSignals &signals);

    /// <summary>テロップを描く（スプライトの描画フェーズから）</summary>
    void Draw();

    /// <summary>進み具合の確認と、詰まったとき用の手動スキップ</summary>
    void DrawImGui();

    /// <summary>ぜんぶ終わったか（シーンはこれを見て次へ進む）</summary>
    bool IsFinished() const { return isFinished_; }

    /// <summary>
    /// ボスを撃つ段まで来たか。
    /// ここまでは操作の練習なので、シーン側はボスを的として出さないでよい
    /// </summary>
    bool IsCombatStageReached() const { return stageIndex_ >= kStageCombat; }

    /// <summary>
    /// 弾の回復エリアを教える段まで来たか。
    /// シーンはこれを見て、エリアを1つ出してやる（本番はボスの攻撃終わりに出るが、
    /// チュートリアルのボスは攻撃してこないので自然には出ない）
    /// </summary>
    bool IsRecoveryStageReached() const { return stageIndex_ >= kStageRecover; }

private:
    /// ===================================================
    /// private types
    /// ===================================================

    /// <summary>やることの並び。テロップに出る順でもある</summary>
    enum TaskId {
        kTaskMove,   // 左スティックで動く
        kTaskLook,   // 右スティックで見回す
        kTaskJump,   // Aでジャンプ
        kTaskDash,   // RBでダッシュ
        kTaskShoot,  // RTで撃つ
        kTaskColor,  // 十字ボタンで色を変える
        kTaskChain,   // 同じ色を3つ以上つなげて消す
        kTaskClear,   // 何回か消す（ぜんぶ剥がすのは長いので回数で区切る）
        kTaskRecover, // 弾の回復エリアに乗る
        kTaskCount,
    };

    /// <summary>段（この単位でテロップが切り替わる）</summary>
    enum StageId {
        kStageMove,    // 動かしてみる
        kStageCombat,  // 撃ってみる
        kStageBreak,   // ボスを崩す
        kStageRecover, // 弾を補給する
        kStageCount,
    };

    /// <summary>やること1つぶんの進み具合</summary>
    struct Task {
        float progress = 0.0f; // 0〜1。1で達成
        bool done = false;     // チェックが付いたか
        float checkAnim = 0.0f; // 付いた瞬間の弾む演出（1→0へ減る）
    };

    /// ===================================================
    /// private method
    /// ===================================================

    /// <summary>今の段に含まれるやることの範囲を返す</summary>
    /// <param name="stage">段</param>
    /// <param name="outBegin">最初のやること</param>
    /// <param name="outEnd">最後のやることの次</param>
    static void GetStageRange(int stage, int &outBegin, int &outEnd);

    /// <summary>やることの達成状況を進める</summary>
    void UpdateTasks(float deltaTime, const TutorialSignals &signals);

    /// <summary>段が埋まったかを見て、次の段へ送る</summary>
    void UpdateStageFlow(float deltaTime);

    /// <summary>達成にする（チェックの演出もここで始める）</summary>
    void Complete(int taskId);

    /// ===================================================
    /// private variables
    /// ===================================================

    Task tasks_[kTaskCount]{};
    int stageIndex_ = kStageMove;   // 今出している段
    float stageTimer_ = 0.0f;       // 段が始まってからの時間（出現アニメに使う）
    float clearDelay_ = -1.0f;      // 段が埋まってから次へ行くまでの残り（負なら埋まっていない）
    bool isFinished_ = false;       // 全部終わったか
    float finishTimer_ = 0.0f;      // 終わってからの時間（完了テロップの演出用）

    // 押した色をビットで覚える（4色ぜんぶ押したら達成）
    int pressedColorMask_ = 0;

    // --- 調整パラメータ（文字とテロップの大きさ）---
    // 文字の大きさを変えると行や板もそれに合わせて組み直すので、
    // 見出しだけ大きくしても崩れない
    // テロップは右上に出す。画面のふちからの距離で置く
    Hagine::Vector2 panelMargin_ = {48.0f, 48.0f}; // 右・上のふちからの距離
    float stageTitleSize_ = 38.0f;  // 段の見出しの文字の高さ
    float stageHintSize_ = 24.0f;   // 段のひとこと説明の文字の高さ（板からはみ出す分は自動で縮む）
    float taskLabelSize_ = 28.0f;   // やることの文字の高さ
    float finishTitleSize_ = 58.0f; // 完了テロップの見出しの高さ
    float finishHintSize_ = 30.0f;  // 完了テロップの補足の高さ
    float glyphSize_ = 58.0f;       // コントローラーの図の高さ
    float checkSize_ = 46.0f;       // チェックボックスの大きさ
    float rowHeight_ = 76.0f;       // 1行の高さ
    float panelWidth_ = 660.0f;     // テロップの板の横幅

    // 登録した調整パラメータは破棄時にまとめて解除する
    Hagine::GameParamOwner params_{"Tutorial"};

    // --- 表示部品 ---
    static constexpr int kRectCapacity = 24; // 板の最大枚数
    GameUi::UiRect rects_;
    GameUi::UiText stageTitles_[kStageCount];
    // 段ごとのひとこと説明。操作だけでは伝わらない決まり（消える条件・補給の色）をここで言う
    GameUi::UiText stageHints_[kStageCount];
    GameUi::UiText taskLabels_[kTaskCount];
    GameUi::UiText finishTitle_;
    GameUi::UiText finishHint_;
    GameUi::UiSprite glyphs_[kTaskCount];     // やることごとのコントローラー図
    // チェックは行ごとに1枚ずつ持つ。スプライトは1フレームに1回しか描けないので、
    // 同じ絵を複数行に出すには枚数ぶん要る
    static constexpr int kMaxRowsPerStage = 4;
    GameUi::UiSprite checkOn_[kMaxRowsPerStage];  // 達成のチェック
    GameUi::UiSprite checkOff_[kMaxRowsPerStage]; // 未達成の空欄
    bool isInitialized_ = false;
};
