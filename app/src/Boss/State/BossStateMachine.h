#pragma once
#include "src/Boss/State/BossState.h"
#include <array>
#include <memory>

/// <summary>
/// ボス用の軽量ステートマシン。
/// 遷移要求は即時に適用せず次の更新の先頭で処理するため、
/// 状態の Update 中から自分自身の遷移を要求しても安全。
/// </summary>
class BossStateMachine {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>状態を登録する（同じ種別は上書き）</summary>
    /// <param name="state">登録する状態</param>
    void Register(std::unique_ptr<IBossState> state);

    /// <summary>初期状態を設定する（Enter を呼ぶ）</summary>
    /// <param name="boss">対象のボス</param>
    /// <param name="id">初期状態</param>
    void Start(Boss &boss, BossStateId id);

    /// <summary>状態の変更を要求する（実際の切り替えは次の Update 先頭）</summary>
    /// <param name="id">遷移先</param>
    void Request(BossStateId id) { requested_ = id; }

    /// <summary>更新（要求された遷移の適用 → 現在状態の更新）</summary>
    /// <param name="boss">対象のボス</param>
    /// <param name="deltaTime">経過時間（秒）</param>
    void Update(Boss &boss, float deltaTime);

    /// ===================================================
    /// getter
    /// ===================================================

    BossStateId GetCurrentId() const { return currentId_; }
    const char *GetCurrentName() const;

    /// <summary>現在の状態に入ってからの経過時間（秒）</summary>
    float GetElapsed() const { return elapsed_; }

private:
    /// ===================================================
    /// private variables
    /// ===================================================

    std::array<std::unique_ptr<IBossState>, static_cast<size_t>(BossStateId::Count)> states_{};
    IBossState *pCurrent_ = nullptr;
    BossStateId currentId_ = BossStateId::Idle;
    BossStateId requested_ = BossStateId::Count; // Count = 要求なし
    float elapsed_ = 0.0f;
};
