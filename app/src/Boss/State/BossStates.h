#pragma once
#include "src/Boss/State/BossState.h"

/// <summary>
/// 待機。緩やかに自転しながら次の攻撃までの間隔を待つ。
/// 自転は死角対策も兼ねていて、裏側のパーツが少しずつ正面へ回ってくる。
/// </summary>
class BossStateIdle final : public IBossState {
public:
    BossStateId GetId() const override { return BossStateId::Idle; }
    const char *GetName() const override { return "待機"; }
    void Update(Boss &boss, float deltaTime) override;
};

/// <summary>
/// 攻撃中。実際の動きは IBossAttack 側が持ち、この状態は進行と終了判定だけを見る。
/// </summary>
class BossStateAttack final : public IBossState {
public:
    BossStateId GetId() const override { return BossStateId::Attack; }
    const char *GetName() const override { return "攻撃"; }
    void Enter(Boss &boss) override;
    void Update(Boss &boss, float deltaTime) override;
    void Exit(Boss &boss) override;
};

/// <summary>
/// 怯み。連鎖破壊で発生し、進行中の攻撃を中断する。
/// </summary>
class BossStateStagger final : public IBossState {
public:
    BossStateId GetId() const override { return BossStateId::Stagger; }
    const char *GetName() const override { return "怯み"; }
    void Enter(Boss &boss) override;
    void Update(Boss &boss, float deltaTime) override;
    void Exit(Boss &boss) override;
};

/// <summary>
/// 撃破。以降は何もしない。
/// </summary>
class BossStateDead final : public IBossState {
public:
    BossStateId GetId() const override { return BossStateId::Dead; }
    const char *GetName() const override { return "撃破"; }
    void Enter(Boss &boss) override;
    void Update(Boss &boss, float deltaTime) override;
};
