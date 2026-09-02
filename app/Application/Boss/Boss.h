#pragma once
#include "Application/Boss/Attack/BossAttackScheduler.h"
#include "Application/Boss/Data/BossColorPalette.h"
#include "Application/Boss/Data/BossParameters.h"
#include "Application/Boss/Part/BossPartGraph.h"
#include "Application/Boss/State/BossStateMachine.h"
#include "Application/Interface/IBossTargetQuery.h"
#include "Application/Interface/IColorProvider.h"
#include "Application/Interface/IDamageable.h"
#include "Application/Interface/ITargetLocator.h"
#include "object/base/BaseObject.h"
#include <string>

/// <summary>コライダーのタグ（ゲーム側で ColliderTagManager へ登録する）</summary>
inline constexpr const char *kBossTag = "Boss";
inline constexpr const char *kBossPartTag = "BossPart";
inline constexpr const char *kPlayerBulletTag = "PlayerBullet";

/// <summary>
/// 色付きパーツで覆われた球状のボス。
///
/// ・見た目: 内側のコア球（このオブジェクト自身）＋表面を覆うパーツ群（BossPartGraph）
/// ・被弾  : IBossTargetQuery 経由で命中を受け取り、連鎖マッチ判定を行う
/// ・連携  : プレイヤーの具象クラスには依存せず、ITargetLocator / IColorProvider だけを見る
/// </summary>
class Boss final : public Hagine::BaseObject, public IDamageable, public IBossTargetQuery {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    Boss() = default;
    ~Boss() override;

    /// <summary>初期化（データ読み込み → コア生成 → パーツ生成）</summary>
    /// <param name="objectName">オブジェクト名</param>
    void Init(const std::string objectName) override;

    /// <summary>更新（怯みタイマ・デバッグ描画）</summary>
    void Update() override;

    /// <summary>描画（コア＋パーツ）</summary>
    /// <param name="viewProjection">ビュープロジェクション</param>
    void Draw(const Hagine::ViewProjection &viewProjection) override;

    /// <summary>インスペクタ表示（基底のUIにボス固有の情報を足す）</summary>
    void DrawImGui() override;

    /// ===================================================
    /// IDamageable
    /// ===================================================

    void ApplyDamage(const DamageInfo &info) override;
    float GetHp() const override { return hp_; }
    bool IsDead() const override { return hp_ <= 0.0f; }

    /// ===================================================
    /// IBossTargetQuery
    /// ===================================================

    bool FindLockOnTarget(const LockOnRequest &request, LockOnResult &out) override;
    ChainHitResult ReportHit(int partIndex, Color shotColor) override;
    ChainHitResult ReportHitByCollider(const Hagine::ColliderBase *hitCollider, Color shotColor) override;
    int FindPartIndex(const Hagine::ColliderBase *collider) const override;

    /// ===================================================
    /// 連携（シーンから配線する）
    /// ===================================================

    /// <summary>狙う相手（プレイヤー）の位置提供元を設定する</summary>
    void SetTargetLocator(ITargetLocator *locator) { pTargetLocator_ = locator; }

    /// <summary>相手が選んでいる色の提供元を設定する</summary>
    void SetColorProvider(IColorProvider *provider) { pColorProvider_ = provider; }

    /// <summary>
    /// ボスの攻撃を当てる相手を設定する。
    /// プレイヤー側が IDamageable を実装したら渡す。未接続でも攻撃自体は成立し、
    /// 命中したことはデバッグ通知で確認できる
    /// </summary>
    void SetTargetDamageSink(IDamageable *sink) { pTargetDamageSink_ = sink; }

    /// <summary>位置と色をまとめて配線する（FunctionalPlayerBridge など両方を実装した相手用）</summary>
    template <typename T>
    void SetPlayerBridge(T *bridge) {
        pTargetLocator_ = static_cast<ITargetLocator *>(bridge);
        pColorProvider_ = static_cast<IColorProvider *>(bridge);
    }

    /// ===================================================
    /// getter
    /// ===================================================

    /// <summary>削れたパーツの割合（素の露出度 0〜1）</summary>
    float GetExposure() const { return graph_.GetExposure(); }

    /// <summary>
    /// 攻撃のスケーリングに使う露出度（0〜1）。
    /// 既定では「連鎖で壊せるパーツの総数」を分母に正規化するため、
    /// 壊せる分をすべて壊した時点で 1.0 になる
    /// </summary>
    float GetNormalizedExposure() const;

    /// <summary>指定色の生存パーツ数（色残量ミニマップ用）</summary>
    int CountAliveParts(Color color) const { return graph_.CountAlive(color); }

    /// <summary>怯み中か</summary>
    bool IsStaggered() const { return staggerTimer_ > 0.0f; }

    float GetMaxHp() const { return parameters_.GetMaxHp(); }
    const BossParameters &GetParameters() const { return parameters_; }
    const BossColorPalette &GetPalette() const { return palette_; }
    BossPartGraph &GetPartGraph() { return graph_; }

    /// <summary>ロックオン中のパーツを強調表示する（-1で解除）</summary>
    void SetLockOnHighlight(int partIndex) { graph_.SetHighlightedPart(partIndex); }

    /// <summary>読み込むボスデータのID（jsons/Boss/[id].json）。Init より前に呼ぶこと</summary>
    void SetBossId(const std::string &bossId) { bossId_ = bossId; }

    /// <summary>パーツ・HPを初期状態へ戻す（デバッグ・リトライ用）</summary>
    void ResetBoss();

    /// <summary>
    /// 半径・パーツの大きさ・厚みの変更を反映する（軽い。パーツは作り直さない）。
    /// 分割数が変わっている場合だけ RebuildParts() へ回す
    /// </summary>
    void ApplyLayoutChanges();

    /// <summary>
    /// パーツを作り直す（分割数の変更用）。壊れたパーツは全て復活する
    /// </summary>
    void RebuildParts();

    /// ===================================================
    /// 状態・攻撃（BossState / IBossAttack から使う操作）
    /// ===================================================

    /// <summary>状態の変更を要求する（切り替えは次の更新の先頭）</summary>
    void RequestState(BossStateId id) { stateMachine_.Request(id); }

    /// <summary>待機中の緩やかな自転を進める（死角対策）</summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    void AddIdleSpin(float deltaTime);

    /// <summary>攻撃までのクールダウンを進め、開始してよいかを返す</summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    /// <returns>bool: 攻撃を始めてよいなら true</returns>
    bool TickAttackCoolDown(float deltaTime);

    /// <summary>次の攻撃を選んで開始する</summary>
    void StartScheduledAttack();

    /// <summary>進行中の攻撃を更新する</summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    /// <returns>bool: まだ続いていれば true</returns>
    bool UpdateCurrentAttack(float deltaTime);

    /// <summary>進行中の攻撃を終える（未完なら中断扱い）。次のクールダウンを開始する</summary>
    void EndCurrentAttack();

    /// <summary>怯み中の揺れを更新する（描画だけを揺らし、当たり判定は動かさない）</summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    void UpdateStaggerShake(float deltaTime);

    /// <summary>怯み中の揺れを解除する</summary>
    void ClearStaggerShake();

    /// <summary>自転を加える（度）</summary>
    /// <param name="degrees">加算する角度（度）</param>
    void AddSpin(float degrees);

    /// <summary>相手が指定の球の中にいるか</summary>
    /// <param name="center">球の中心</param>
    /// <param name="radius">球の半径</param>
    /// <returns>bool: 入っていれば true</returns>
    bool IsTargetWithin(const Hagine::Vector3 &center, float radius) const;

    /// <summary>相手へダメージを与える（受け口が未接続なら通知だけ出す）</summary>
    /// <param name="amount">ダメージ量</param>
    /// <param name="impactPoint">着弾位置</param>
    /// <returns>bool: 受け口へ渡せたら true</returns>
    bool DealDamageToTarget(float amount, const Hagine::Vector3 &impactPoint);

    /// ===================================================
    /// 位置・向き
    /// ===================================================

    Hagine::Vector3 GetBossPosition() const { return transform_->translation_; }
    void SetBossPosition(const Hagine::Vector3 &position);
    const Hagine::Vector3 &GetHomePosition() const { return homePosition_; }
    float GetBodyRadius() const { return parameters_.Layout().radius; }

    /// <summary>現在の状態名（デバッグUI用）</summary>
    const char *GetStateName() const { return stateMachine_.GetCurrentName(); }

private:
    /// ===================================================
    /// private method
    /// ===================================================

    /// <summary>コライダーのタグをエンジンへ登録する</summary>
    static void RegisterGameTags();

    /// <summary>実行時調整用にパラメータを GameParamHub へ登録する</summary>
    void RegisterTuningParameters();

    /// <summary>状態と攻撃を組み立てる</summary>
    void SetupStatesAndAttacks();

    /// <summary>アリーナの外へ出ないよう位置を丸める</summary>
    void ClampToArena();

    /// <summary>露出度に応じて攻撃頻度を更新する（激しさは各攻撃が開始時に算出する）</summary>
    void UpdateExposureScaling();

    /// <summary>コア（本体の球）の大きさと接地高さを半径へ追従させる</summary>
    void ApplyCoreLayout();

    /// <summary>攻撃の文脈を作る</summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    BossAttackContext MakeAttackContext(float deltaTime);

    /// ===================================================
    /// private variables
    /// ===================================================

    std::string bossId_ = "Boss01"; // 読み込むボスデータのID
    BossParameters parameters_{};   // ボスごとのデータ（JSON）
    BossColorPalette palette_{};  // 色マスタ＋使用色サブセット
    BossPartGraph graph_{};       // パーツ群と隣接グラフ

    float hp_ = 0.0f;            // 残りHP
    float staggerTimer_ = 0.0f;  // 怯み残り時間（秒）

    ITargetLocator *pTargetLocator_ = nullptr;   // 狙う相手（非所有）
    IColorProvider *pColorProvider_ = nullptr;   // 相手の選択色（非所有）
    IDamageable *pTargetDamageSink_ = nullptr;   // 攻撃の当て先（非所有・未接続可）

    BossStateMachine stateMachine_{};            // 待機／攻撃／怯み／撃破
    BossAttackScheduler scheduler_{};            // 攻撃の選択と間隔（攻撃の所有者）
    IBossAttack *pCurrentAttack_ = nullptr;      // 進行中の攻撃（所有は scheduler_）

    Hagine::Vector3 homePosition_{};             // 初期位置（アリーナ中心・着地高さの基準）
    int builtSubdivision_ = -1;                  // 現在のパーツを組んだときの分割数
    float spinAngle_ = 0.0f;                     // 自転の累積角（ラジアン）
    float staggerShakeTime_ = 0.0f;              // 怯み揺れの経過時間

    bool drawGraphDebug_ = false; // 隣接グラフのデバッグ描画
    std::string paramOwnerLabel_; // GameParamHub の登録ラベル
};
