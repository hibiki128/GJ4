#include "BossAttackScheduler.h"
#include "random.h"
#include <algorithm>

void BossAttackScheduler::AddAttack(std::unique_ptr<IBossAttack> attack) {
    if (!attack) {
        return;
    }
    attacks_.push_back(std::move(attack));
}

void BossAttackScheduler::Reset() {
    // 戦闘開始直後にいきなり撃たれないよう、1回分の間隔を空ける
    coolDown_ = interval_;
    lastIndex_ = -1;
}

bool BossAttackScheduler::TickCoolDown(float deltaTime) {
    if (attacks_.empty()) {
        return false;
    }
    coolDown_ -= deltaTime;
    if (coolDown_ > 0.0f) {
        return false;
    }
    coolDown_ = 0.0f;
    return true;
}

IBossAttack *BossAttackScheduler::PickNext() {
    if (attacks_.empty()) {
        return nullptr;
    }
    if (attacks_.size() == 1) {
        lastIndex_ = 0;
        return attacks_[0].get();
    }

    // 直前と同じ攻撃が続かないよう、残りの中から選ぶ
    const int count = static_cast<int>(attacks_.size());
    int index = Hagine::Random::Range(0, count - 2);
    if (lastIndex_ >= 0 && index >= lastIndex_) {
        ++index;
    }
    index = std::clamp(index, 0, count - 1);

    lastIndex_ = index;
    return attacks_[static_cast<size_t>(index)].get();
}

void BossAttackScheduler::NotifyAttackFinished() {
    coolDown_ = interval_;
}

void BossAttackScheduler::SetInterval(float seconds) {
    interval_ = (std::max)(0.1f, seconds);
}

IBossAttack *BossAttackScheduler::GetAttack(size_t index) {
    if (index >= attacks_.size()) {
        return nullptr;
    }
    return attacks_[index].get();
}
