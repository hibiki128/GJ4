#pragma once

namespace Hagine {
class ViewProjection;
}

class Boss;
class BossSpider;
class ITargetLocator;

/// <summary>
/// 攻撃の更新に必要な情報一式
/// </summary>
struct BossAttackContext {
    Boss *boss = nullptr;             // 攻撃するボス（球体形態。蜘蛛の攻撃では nullptr）
    BossSpider *spider = nullptr;     // 攻撃する蜘蛛（第2形態。球体形態の攻撃では nullptr）
    ITargetLocator *target = nullptr; // 狙う相手（未接続なら nullptr）
    float deltaTime = 0.0f;           // 経過時間（秒）
    float exposure = 0.0f;            // 露出度 0〜1（激しさのスケーリングに使う）
};

/// <summary>
/// ボスの攻撃1種類分のインターフェース。
/// 色とは独立した、当たり判定だけのシンプルな攻撃を想定している。
/// </summary>
class IBossAttack {
public:
    virtual ~IBossAttack() = default;

    /// <summary>表示名（デバッグUI用）</summary>
    virtual const char *GetName() const = 0;

    /// <summary>攻撃を開始する</summary>
    /// <param name="context">攻撃の文脈</param>
    virtual void Start(const BossAttackContext &context) = 0;

    /// <summary>毎フレームの更新</summary>
    /// <param name="context">攻撃の文脈</param>
    virtual void Update(const BossAttackContext &context) = 0;

    /// <summary>一連の動作が終わったか</summary>
    virtual bool IsFinished() const = 0;

    /// <summary>怯みなどで中断する（後始末をしてすぐ終了状態にする）</summary>
    /// <param name="context">攻撃の文脈</param>
    virtual void Cancel(const BossAttackContext &context) = 0;

    /// <summary>攻撃中だけ出す表示物（予告マーカーなど）を描画する</summary>
    /// <param name="viewProjection">ビュープロジェクション</param>
    virtual void Draw(const Hagine::ViewProjection &viewProjection) { (void)viewProjection; }

    /// <summary>現在の段階名（デバッグUI用）</summary>
    virtual const char *GetPhaseName() const = 0;
};
