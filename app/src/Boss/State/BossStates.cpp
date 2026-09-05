#include "BossStates.h"
#include "src/Boss/Boss.h"

/// ===================================================
/// 登場
/// ===================================================

void BossStateAppear::Enter(Boss &boss) {
    boss.BeginAppear();
}

void BossStateAppear::Update(Boss &boss, float deltaTime) {
    // 演出が終わったら通常の待機へ
    if (!boss.UpdateAppear(deltaTime)) {
        boss.RequestState(BossStateId::Idle);
    }
}

void BossStateAppear::Exit(Boss &boss) {
    boss.EndAppear();
}

/// ===================================================
/// 待機
/// ===================================================

void BossStateIdle::Update(Boss &boss, float deltaTime) {
    boss.AddIdleSpin(deltaTime);

    if (boss.TickAttackCoolDown(deltaTime)) {
        boss.RequestState(BossStateId::Attack);
    }
}

/// ===================================================
/// 攻撃
/// ===================================================

void BossStateAttack::Enter(Boss &boss) {
    boss.StartScheduledAttack();
}

void BossStateAttack::Update(Boss &boss, float deltaTime) {
    // 攻撃が終わったら待機へ戻る
    if (!boss.UpdateCurrentAttack(deltaTime)) {
        boss.RequestState(BossStateId::Idle);
    }
}

void BossStateAttack::Exit(Boss &boss) {
    // 通常終了・怯みによる中断のどちらでもここを通る
    boss.EndCurrentAttack();
}

/// ===================================================
/// 怯み
/// ===================================================

void BossStateStagger::Enter(Boss &boss) {
    (void)boss;
}

void BossStateStagger::Update(Boss &boss, float deltaTime) {
    boss.UpdateStaggerShake(deltaTime);

    if (!boss.IsStaggered()) {
        boss.RequestState(BossStateId::Idle);
    }
}

void BossStateStagger::Exit(Boss &boss) {
    boss.ClearStaggerShake();
}

/// ===================================================
/// 撃破
/// ===================================================

void BossStateDead::Enter(Boss &boss) {
    boss.ClearStaggerShake();
}

void BossStateDead::Update(Boss &boss, float deltaTime) {
    (void)boss;
    (void)deltaTime;
}
