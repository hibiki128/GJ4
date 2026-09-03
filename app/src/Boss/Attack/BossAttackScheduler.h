#pragma once
#include "src/Boss/Attack/IBossAttack.h"
#include <memory>
#include <vector>

/// <summary>
/// 攻撃の選択と間隔を管理する。
/// 「どれくらいの頻度で・どの攻撃を出すか」だけを受け持ち、攻撃そのものの中身は持たない。
/// 間隔は外から与える（露出度に応じたスケーリングはボス側で計算して SetInterval する）。
/// </summary>
class BossAttackScheduler {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>攻撃を追加する（所有権を受け取る）</summary>
    /// <param name="attack">追加する攻撃</param>
    void AddAttack(std::unique_ptr<IBossAttack> attack);

    /// <summary>クールダウンを初期状態に戻す</summary>
    void Reset();

    /// <summary>クールダウンを進め、攻撃を始めてよいかを返す</summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    /// <returns>bool: 攻撃可能なら true</returns>
    bool TickCoolDown(float deltaTime);

    /// <summary>次に出す攻撃を選ぶ（直前と同じ攻撃は避ける）</summary>
    /// <returns>IBossAttack*: 選ばれた攻撃（未登録なら nullptr）</returns>
    IBossAttack *PickNext();

    /// <summary>攻撃が終わったことを通知する（クールダウン開始）</summary>
    void NotifyAttackFinished();

    /// ===================================================
    /// getter / setter
    /// ===================================================

    /// <summary>攻撃と攻撃の間隔（秒）を設定する</summary>
    void SetInterval(float seconds);
    float GetInterval() const { return interval_; }
    float GetRemainingCoolDown() const { return coolDown_; }

    size_t GetAttackCount() const { return attacks_.size(); }
    IBossAttack *GetAttack(size_t index);

    /// <summary>すぐ攻撃できる状態にする（デバッグ用）</summary>
    void ForceReady() { coolDown_ = 0.0f; }

private:
    /// ===================================================
    /// private variables
    /// ===================================================

    std::vector<std::unique_ptr<IBossAttack>> attacks_{}; // 登録された攻撃（所有）
    float interval_ = 4.5f;                               // 攻撃と攻撃の間隔（秒）
    float coolDown_ = 0.0f;                               // 次の攻撃までの残り時間（秒）
    int lastIndex_ = -1;                                  // 直前に出した攻撃
};
