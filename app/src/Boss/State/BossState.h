#pragma once

class Boss;

/// <summary>
/// ボスの状態種別
/// </summary>
enum class BossStateId {
    Idle,    // 待機（緩やかに自転しながら次の攻撃を待つ）
    Attack,  // 攻撃中
    Stagger, // 怯み（連鎖破壊で発生。攻撃は中断される）
    Dead,    // 撃破
    Count
};

/// <summary>
/// ボスの状態1つ分のインターフェース。
/// エンジンに汎用ステートマシンが無いため、ゲーム層に最小限のものを用意している。
/// </summary>
class IBossState {
public:
    virtual ~IBossState() = default;

    /// <summary>状態の種別</summary>
    virtual BossStateId GetId() const = 0;

    /// <summary>表示名（デバッグUI用）</summary>
    virtual const char *GetName() const = 0;

    /// <summary>この状態に入ったとき</summary>
    /// <param name="boss">対象のボス</param>
    virtual void Enter(Boss &boss) { (void)boss; }

    /// <summary>毎フレームの更新</summary>
    /// <param name="boss">対象のボス</param>
    /// <param name="deltaTime">経過時間（秒）</param>
    virtual void Update(Boss &boss, float deltaTime) = 0;

    /// <summary>この状態から出るとき</summary>
    /// <param name="boss">対象のボス</param>
    virtual void Exit(Boss &boss) { (void)boss; }
};
