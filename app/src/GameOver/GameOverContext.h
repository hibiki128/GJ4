#pragma once

/// <summary>ボスの形態</summary>
enum class BossFormId {
    Sphere, // 第1形態（色付きの殻をまとった球体）
    Spider, // 第2形態（蜘蛛）
};

/// <summary>
/// 「どの形態に負けたか」だけをゲームシーンからゲームオーバー画面へ渡す入れ物。
///
/// シーンは切り替わるたびに作り直されるので、値をまたいで持ち越す場所がどこかに要る。
/// ゲームオーバー画面がゲームシーンの中身（ボスの状態）を直接覗きにいくと
/// シーン同士が繋がってしまうので、間にこの1枚を挟んでいる。
/// </summary>
class GameOverContext {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    static GameOverContext *GetInstance() {
        static GameOverContext instance;
        return &instance;
    }

    /// <summary>負けた瞬間の形態を控える（ゲームシーンが呼ぶ）</summary>
    void SetBossForm(BossFormId form) { bossForm_ = form; }

    /// <summary>控えてある形態を取り出す（ゲームオーバー画面が呼ぶ）</summary>
    BossFormId GetBossForm() const { return bossForm_; }

    /// <summary>形態の名前（デバッグ表示・パラメータのグループ名に使う）</summary>
    static const char *GetFormName(BossFormId form) {
        return (form == BossFormId::Spider) ? "Spider" : "Sphere";
    }

private:
    GameOverContext() = default;
    ~GameOverContext() = default;
    GameOverContext(const GameOverContext &) = delete;
    GameOverContext &operator=(const GameOverContext &) = delete;

    // ゲームシーンを通らずに直接ゲームオーバー画面を開いたときは第1形態にしておく
    BossFormId bossForm_ = BossFormId::Sphere;
};
