#include "BossStateMachine.h"

void BossStateMachine::Register(std::unique_ptr<IBossState> state) {
    if (!state) {
        return;
    }
    const size_t index = static_cast<size_t>(state->GetId());
    if (index >= states_.size()) {
        return;
    }
    states_[index] = std::move(state);
}

void BossStateMachine::Start(Boss &boss, BossStateId id) {
    const size_t index = static_cast<size_t>(id);
    if (index >= states_.size() || !states_[index]) {
        return;
    }
    pCurrent_ = states_[index].get();
    currentId_ = id;
    elapsed_ = 0.0f;
    requested_ = BossStateId::Count;
    pCurrent_->Enter(boss);
}

void BossStateMachine::Update(Boss &boss, float deltaTime) {
    // 要求されていた遷移をここで適用する（状態の Update 中に要求されても安全）
    if (requested_ != BossStateId::Count) {
        const BossStateId next = requested_;
        requested_ = BossStateId::Count;

        const size_t index = static_cast<size_t>(next);
        if (index < states_.size() && states_[index] && next != currentId_) {
            if (pCurrent_) {
                pCurrent_->Exit(boss);
            }
            pCurrent_ = states_[index].get();
            currentId_ = next;
            elapsed_ = 0.0f;
            pCurrent_->Enter(boss);
        }
    }

    if (!pCurrent_) {
        return;
    }
    elapsed_ += deltaTime;
    pCurrent_->Update(boss, deltaTime);
}

const char *BossStateMachine::GetCurrentName() const {
    return pCurrent_ ? pCurrent_->GetName() : "None";
}
