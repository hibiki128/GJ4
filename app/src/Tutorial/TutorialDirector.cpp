#include "TutorialDirector.h"
#include "src/Character/ColorStruct.h"
#include "Easing.h"
#include "WinApp.h"
#include <algorithm>
#include <cmath>
#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

using namespace Hagine;

namespace {

/// <summary>コントローラーの図の置き場</summary>
constexpr const char *kGlyphFolder = "Tutorial/";

/// <summary>やることごとの図。TaskId の並びと合わせること</summary>
constexpr const char *kGlyphFiles[] = {
    "stick_l.png",   // 移動
    "stick_r.png",   // 視点
    "btn_a.png",     // ジャンプ
    "btn_lt.png",    // ダッシュ
    "btn_rt.png",    // 射撃
    "dpad.png",      // 色変え
    "icon_chain.png", // 3つつなげる
    "icon_strip.png", // 何回か消す
    "icon_zone.png",  // 弾の回復エリア
};

/// <summary>達成の判定に使う量。増やすとゆっくり進む</summary>
constexpr float kMoveHoldTime = 1.2f;  // 移動し続ける時間（秒）
constexpr float kLookHoldTime = 1.0f;  // 見回し続ける時間（秒）
constexpr float kDashHoldTime = 0.5f;  // ダッシュし続ける時間（秒）
constexpr int kShootCount = 3;         // 撃つ回数
constexpr int kClearCount = 5;         // 球を消す回数（ぜんぶ剥がすのは長すぎるので回数で区切る）
constexpr float kZoneHoldTime = 3.0f;  // 回復エリアに乗っていてほしい時間（秒）

/// <summary>スティックを「倒した」とみなす量</summary>
constexpr float kStickDeadZone = 0.45f;

/// <summary>段が埋まってから次の段へ送るまでの間（秒）。チェックを見せる時間</summary>
constexpr float kStageClearWait = 1.1f;

/// <summary>チェックが付いた瞬間の弾む時間（秒）</summary>
constexpr float kCheckPopTime = 0.35f;

/// --- テロップの配置 ---
constexpr float kPanelPadding = 22.0f;

const Vector4 kPanelColor = {0.07f, 0.09f, 0.15f, 0.84f};
const Vector4 kAccentColor = {0.35f, 0.78f, 1.00f, 1.0f};
const Vector4 kTitleColor = {1.0f, 1.0f, 1.0f, 1.0f};
const Vector4 kLabelColor = {0.92f, 0.94f, 1.0f, 1.0f};
const Vector4 kLabelDoneColor = {0.55f, 0.95f, 0.68f, 1.0f};
const Vector4 kBarBackColor = {1.0f, 1.0f, 1.0f, 0.16f};
const Vector4 kWhite = {1.0f, 1.0f, 1.0f, 1.0f};

} // namespace

void TutorialDirector::GetStageRange(int stage, int &outBegin, int &outEnd) {
    switch (stage) {
    case kStageMove:
        outBegin = kTaskMove;
        outEnd = kTaskDash + 1;
        return;
    case kStageCombat:
        outBegin = kTaskShoot;
        outEnd = kTaskColor + 1;
        return;
    case kStageBreak:
        outBegin = kTaskChain;
        outEnd = kTaskClear + 1;
        return;
    default:
        outBegin = kTaskRecover;
        outEnd = kTaskRecover + 1;
        return;
    }
}

void TutorialDirector::Init() {
    if (isInitialized_) {
        return;
    }
    isInitialized_ = true;

    rects_.Initialize(kRectCapacity);

    stageTitles_[kStageMove].Create("Tutorial_Stage0", "うごかしてみよう");
    stageTitles_[kStageCombat].Create("Tutorial_Stage1", "うってみよう");
    stageTitles_[kStageBreak].Create("Tutorial_Stage2", "てきを けずろう");
    stageTitles_[kStageRecover].Create("Tutorial_Stage3", "たまを ほきゅうしよう");

    // 段ごとのひとこと。操作を並べただけでは伝わらない決まりをここで言う
    stageHints_[kStageMove].Create("Tutorial_Hint0", "まずは 体の うごかし方から", 3.0f);
    stageHints_[kStageCombat].Create("Tutorial_Hint1", "うつ たまの 色は じぶんで えらべる", 3.0f);
    stageHints_[kStageBreak].Create("Tutorial_Hint2", "同じ色が 3つ そろうと 消える。ぜんぶ 消せば たおせる", 3.0f);
    stageHints_[kStageRecover].Create("Tutorial_Hint3", "ゆかと 同じ色の たまが はやく もどる", 3.0f);

    // 文言はボタン名を先に置く。図と読み上げの順が揃っていたほうが探しやすい
    taskLabels_[kTaskMove].Create("Tutorial_TaskMove", "左を たおして うごく", 3.0f);
    taskLabels_[kTaskLook].Create("Tutorial_TaskLook", "右を たおして 見まわす", 3.0f);
    taskLabels_[kTaskJump].Create("Tutorial_TaskJump", "A で とぶ", 3.0f);
    taskLabels_[kTaskDash].Create("Tutorial_TaskDash", "LT で はやく はしる", 3.0f);
    taskLabels_[kTaskShoot].Create("Tutorial_TaskShoot", "RT で たまを うつ", 3.0f);
    taskLabels_[kTaskColor].Create("Tutorial_TaskColor", "十字 と LB RB で 色をかえる", 3.0f);
    taskLabels_[kTaskChain].Create("Tutorial_TaskChain", "同じ色を 3つ つなげて 消す", 3.0f);
    // ぜんぶ剥がすのは時間がかかりすぎるので、回数で区切る
    taskLabels_[kTaskClear].Create("Tutorial_TaskClear", "色だまを 5かい 消す", 3.0f);
    taskLabels_[kTaskRecover].Create("Tutorial_TaskRecover", "まるい ゆかに 3びょう のる", 3.0f);

    finishTitle_.Create("Tutorial_FinishTitle", "れんしゅう かんりょう！");
    finishHint_.Create("Tutorial_FinishHint", "ほんばんへ すすみます", 3.0f);

    for (int index = 0; index < kTaskCount; ++index) {
        glyphs_[index].Initialize(std::string(kGlyphFolder) + kGlyphFiles[index]);
    }
    for (int row = 0; row < kMaxRowsPerStage; ++row) {
        checkOn_[row].Initialize(std::string(kGlyphFolder) + "check_on.png");
        checkOff_[row].Initialize(std::string(kGlyphFolder) + "check_off.png");
    }
}

void TutorialDirector::RegisterParams() {
    // 文字の大きさはここから変えられる。行の高さや板の幅も一緒に置いてあるので、
    // 文字を大きくしたときに窮屈にならないよう合わせて広げられる
    params_.Register("PanelMargin", &panelMargin_, {1.0f});
    params_.Register("StageTitleSize", &stageTitleSize_, {0.5f, 8.0f, 120.0f});
    params_.Register("StageHintSize", &stageHintSize_, {0.5f, 8.0f, 120.0f});
    params_.Register("TaskLabelSize", &taskLabelSize_, {0.5f, 8.0f, 120.0f});
    params_.Register("FinishTitleSize", &finishTitleSize_, {0.5f, 8.0f, 160.0f});
    params_.Register("FinishHintSize", &finishHintSize_, {0.5f, 8.0f, 120.0f});
    params_.Register("GlyphSize", &glyphSize_, {0.5f, 8.0f, 160.0f});
    params_.Register("CheckSize", &checkSize_, {0.5f, 8.0f, 160.0f});
    params_.Register("RowHeight", &rowHeight_, {0.5f, 20.0f, 200.0f});
    params_.Register("PanelWidth", &panelWidth_, {1.0f, 200.0f, 1400.0f});
}

void TutorialDirector::Finalize() {
    rects_.Finalize();
    for (GameUi::UiText &title : stageTitles_) {
        title.Finalize();
    }
    for (GameUi::UiText &hint : stageHints_) {
        hint.Finalize();
    }
    for (GameUi::UiText &label : taskLabels_) {
        label.Finalize();
    }
    finishTitle_.Finalize();
    finishHint_.Finalize();
    for (GameUi::UiSprite &glyph : glyphs_) {
        glyph.Finalize();
    }
    for (int row = 0; row < kMaxRowsPerStage; ++row) {
        checkOn_[row].Finalize();
        checkOff_[row].Finalize();
    }
    isInitialized_ = false;
}

void TutorialDirector::Complete(int taskId) {
    Task &task = tasks_[taskId];
    if (task.done) {
        return;
    }
    task.progress = 1.0f;
    task.done = true;
    task.checkAnim = 1.0f;
}

void TutorialDirector::Update(float deltaTime, const TutorialSignals &signals) {
    if (isFinished_) {
        finishTimer_ += deltaTime;
        return;
    }

    stageTimer_ += deltaTime;

    // チェックが付いた瞬間の弾みを進める
    for (Task &task : tasks_) {
        if (task.checkAnim > 0.0f) {
            task.checkAnim = (std::max)(0.0f, task.checkAnim - deltaTime / kCheckPopTime);
        }
    }

    UpdateTasks(deltaTime, signals);
    UpdateStageFlow(deltaTime);
}

void TutorialDirector::UpdateTasks(float deltaTime, const TutorialSignals &signals) {
    int begin = 0;
    int end = 0;
    GetStageRange(stageIndex_, begin, end);

    // 今の段のやることだけを見る。先の段の条件を先取りで達成させない
    // （順番に教えたいので、撃てるようになる前に撃たれても数えない）
    for (int taskId = begin; taskId < end; ++taskId) {
        Task &task = tasks_[taskId];
        if (task.done) {
            continue;
        }

        switch (taskId) {
        case kTaskMove: {
            const float amount = signals.moveStick.Length();
            if (amount > kStickDeadZone) {
                task.progress += deltaTime / kMoveHoldTime;
            }
            break;
        }
        case kTaskLook: {
            const float amount = signals.lookStick.Length();
            if (amount > kStickDeadZone) {
                task.progress += deltaTime / kLookHoldTime;
            }
            break;
        }
        case kTaskJump:
            if (signals.jumped) {
                task.progress = 1.0f;
            }
            break;
        case kTaskDash:
            if (signals.dashing) {
                task.progress += deltaTime / kDashHoldTime;
            }
            break;
        case kTaskShoot:
            if (signals.shot) {
                task.progress += 1.0f / static_cast<float>(kShootCount);
            }
            break;
        case kTaskColor:
            if (signals.selectedColorIndex >= 0) {
                pressedColorMask_ |= (1 << signals.selectedColorIndex);
            }
            {
                // 4色ぜんぶ選べたら達成。何色ぶん選んだかをそのまま進み具合にする
                int pressed = 0;
                for (int bit = 0; bit < kGameColorCount; ++bit) {
                    if (pressedColorMask_ & (1 << bit)) {
                        ++pressed;
                    }
                }
                task.progress = static_cast<float>(pressed) / static_cast<float>(kGameColorCount);
            }
            break;
        case kTaskChain:
            if (signals.chainCleared) {
                task.progress = 1.0f;
            }
            break;
        case kTaskClear:
            if (signals.chainCleared) {
                task.progress += 1.0f / static_cast<float>(kClearCount);
            }
            break;
        case kTaskRecover:
            // 乗っているあいだだけ進む。降りても戻さないので、何回かに分けて乗ってもよい
            if (signals.inRecoveryZone) {
                task.progress += deltaTime / kZoneHoldTime;
            }
            break;
        default:
            break;
        }

        if (task.progress >= 1.0f) {
            Complete(taskId);
        }
    }
}

void TutorialDirector::UpdateStageFlow(float deltaTime) {
    int begin = 0;
    int end = 0;
    GetStageRange(stageIndex_, begin, end);

    bool allDone = true;
    for (int taskId = begin; taskId < end; ++taskId) {
        if (!tasks_[taskId].done) {
            allDone = false;
            break;
        }
    }

    if (!allDone) {
        return;
    }

    // 埋まった瞬間にすぐ切り替えるとチェックが見えないので、少し置いてから送る
    if (clearDelay_ < 0.0f) {
        clearDelay_ = kStageClearWait;
        return;
    }

    clearDelay_ -= deltaTime;
    if (clearDelay_ > 0.0f) {
        return;
    }

    clearDelay_ = -1.0f;
    if (stageIndex_ + 1 < kStageCount) {
        ++stageIndex_;
        stageTimer_ = 0.0f;
    } else {
        isFinished_ = true;
        finishTimer_ = 0.0f;
    }
}

void TutorialDirector::Draw() {
    if (!isInitialized_) {
        return;
    }

    rects_.BeginFrame();

    const float screenWidth = static_cast<float>(WinApp::GetVirtualWidth());
    const float screenHeight = static_cast<float>(WinApp::GetVirtualHeight());

    if (isFinished_) {
        // 完了テロップ。画面中央に大きく出す。帯の高さは文字に合わせて広がる
        const float appear = std::clamp(finishTimer_ / 0.4f, 0.0f, 1.0f);
        const float centerY = screenHeight * 0.42f;
        const float bandHeight = (finishTitleSize_ + finishHintSize_) * 1.6f + 40.0f;
        rects_.Draw({screenWidth * 0.5f, centerY}, {screenWidth, bandHeight * appear},
                    {0.07f, 0.09f, 0.15f, 0.80f * appear});
        if (appear >= 1.0f) {
            const float gap = (finishTitleSize_ + finishHintSize_) * 0.45f;
            finishTitle_.DrawCentered({screenWidth * 0.5f, centerY - gap * 0.5f}, finishTitleSize_,
                                      kTitleColor);
            finishHint_.DrawCentered({screenWidth * 0.5f, centerY + gap * 0.75f}, finishHintSize_,
                                     kAccentColor);
        }
        return;
    }

    int begin = 0;
    int end = 0;
    GetStageRange(stageIndex_, begin, end);
    const int rowCount = end - begin;

    // 段が変わったときに右からすべり込ませる
    const float slide = 1.0f - std::clamp(stageTimer_ / 0.35f, 0.0f, 1.0f);
    const float panelLeft =
        screenWidth - panelMargin_.x - panelWidth_ + slide * (panelWidth_ + panelMargin_.x);

    // 見出しのぶんの高さは文字の大きさから決める
    const float headerHeight = stageTitleSize_ + 26.0f;
    // 一番下にひとこと説明を置くぶんの高さ
    const float hintRowHeight = stageHintSize_ * 2.1f;
    const float panelHeight = headerHeight + static_cast<float>(rowCount) * rowHeight_ +
                              hintRowHeight + kPanelPadding;
    const float panelTop = panelMargin_.y;
    const float panelCenterX = panelLeft + panelWidth_ * 0.5f;
    const float panelCenterY = panelTop + panelHeight * 0.5f;

    // 背景の板と、左端の色帯
    rects_.Draw({panelCenterX, panelCenterY}, {panelWidth_, panelHeight}, kPanelColor);
    rects_.Draw({panelLeft + panelWidth_ - 4.0f, panelCenterY}, {8.0f, panelHeight}, kAccentColor);

    // 見出し
    stageTitles_[stageIndex_].DrawLeft(panelLeft + 30.0f, panelTop + headerHeight * 0.55f,
                                       stageTitleSize_, kTitleColor);

    // 行の中身をどこから並べるかは、図の大きさに合わせてずらす
    const float glyphCenterX = panelLeft + kPanelPadding + glyphSize_ * 0.5f;
    const float labelLeft = glyphCenterX + glyphSize_ * 0.5f + 26.0f;

    for (int row = 0; row < rowCount && row < kMaxRowsPerStage; ++row) {
        const int taskId = begin + row;
        const Task &task = tasks_[taskId];
        const float rowCenterY = panelTop + headerHeight + rowHeight_ * (row + 0.5f);

        // コントローラーの図。画像の縦横比を保って収める
        const Vector2 glyphBase = glyphs_[taskId].GetBaseSize();
        const float glyphWidth =
            (glyphBase.y > 0.0f) ? glyphSize_ * (glyphBase.x / glyphBase.y) : glyphSize_;
        glyphs_[taskId].Draw({glyphCenterX, rowCenterY}, {glyphWidth, glyphSize_}, kWhite);

        // やること
        const Vector4 labelColor = task.done ? kLabelDoneColor : kLabelColor;
        taskLabels_[taskId].DrawLeft(labelLeft, rowCenterY - taskLabelSize_ * 0.28f, taskLabelSize_,
                                     labelColor);

        // 途中経過のバー。押しっぱなし系はどれだけ進んだか見えたほうが親切
        if (!task.done && task.progress > 0.0f) {
            const float barLeft = labelLeft;
            const float barWidth =
                (std::max)(40.0f, panelWidth_ - (labelLeft - panelLeft) - checkSize_ - 46.0f);
            const float barY = rowCenterY + taskLabelSize_ * 0.72f;
            rects_.Draw({barLeft + barWidth * 0.5f, barY}, {barWidth, 8.0f}, kBarBackColor);
            const float filled = barWidth * std::clamp(task.progress, 0.0f, 1.0f);
            rects_.Draw({barLeft + filled * 0.5f, barY}, {filled, 8.0f}, kAccentColor);
        }

        // チェックボックス。付いた瞬間だけ少し大きく弾ませる
        const float pop = 1.0f + task.checkAnim * 0.45f;
        const float boxSize = checkSize_ * pop;
        const Vector2 boxCenter = {panelLeft + panelWidth_ - kPanelPadding - checkSize_ * 0.5f,
                                   rowCenterY};
        if (task.done) {
            checkOn_[row].Draw(boxCenter, {boxSize, boxSize}, kWhite);
        } else {
            checkOff_[row].Draw(boxCenter, {boxSize, boxSize}, kWhite);
        }
    }

    // 段のひとこと説明。やることの下に、区切り線を挟んで置く。
    // 文が長いと板からはみ出すので、入りきらないぶんは文字を縮めて幅に合わせる
    {
        const float innerWidth = panelWidth_ - kPanelPadding * 2.0f;
        float hintHeight = stageHintSize_;
        const float hintWidth = stageHints_[stageIndex_].WidthAt(hintHeight);
        if (hintWidth > innerWidth && hintWidth > 0.0f) {
            hintHeight *= innerWidth / hintWidth;
        }

        const float rowsBottom =
            panelTop + headerHeight + static_cast<float>(rowCount) * rowHeight_;
        rects_.Draw({panelCenterX, rowsBottom + 2.0f}, {innerWidth, 2.0f}, kBarBackColor);
        stageHints_[stageIndex_].DrawLeft(panelLeft + kPanelPadding,
                                          rowsBottom + hintRowHeight * 0.5f, hintHeight,
                                          kAccentColor);
    }
}

void TutorialDirector::DrawImGui() {
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader("チュートリアル", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    static const char *kStageNames[kStageCount] = {"うごかしてみよう", "うってみよう",
                                                  "てきを けずろう", "たまを ほきゅうしよう"};
    ImGui::Text("いまの段: %s", isFinished_ ? "完了" : kStageNames[stageIndex_]);

    int begin = 0;
    int end = 0;
    GetStageRange(stageIndex_, begin, end);
    for (int taskId = 0; taskId < kTaskCount; ++taskId) {
        const Task &task = tasks_[taskId];
        const bool inStage = (taskId >= begin && taskId < end);
        ImGui::TextColored(task.done ? ImVec4{0.5f, 0.9f, 0.6f, 1.0f}
                                     : (inStage ? ImVec4{1.0f, 1.0f, 1.0f, 1.0f}
                                                : ImVec4{0.5f, 0.5f, 0.5f, 1.0f}),
                           "%s %d: %.0f%%", task.done ? "[x]" : "[ ]", taskId, task.progress * 100.0f);
    }

    // 動作確認のときに毎回全部やるのは大変なので、段を飛ばせるようにしておく
    if (ImGui::Button("この段をとばす")) {
        for (int taskId = begin; taskId < end; ++taskId) {
            Complete(taskId);
        }
    }
#endif // USE_IMGUI
}
