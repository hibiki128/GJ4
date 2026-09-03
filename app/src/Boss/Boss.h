#pragma once
#include "src/Boss/Attack/BossAttackScheduler.h"
#include "src/Boss/Data/BossColorPalette.h"
#include "src/Boss/Data/BossParameters.h"
#include "src/Boss/Shell/BossSphereCluster.h"
#include "src/Boss/State/BossStateMachine.h"
#include "src/Interface/IBossTargetQuery.h"
#include "src/Interface/IColorProvider.h"
#include "src/Interface/IDamageable.h"
#include "src/Interface/ITargetLocator.h"
#include "object/base/BaseObject.h"
#include <string>

/// <summary>コライダーのタグ（ゲーム側で ColliderTagManager へ登録する）</summary>
inline constexpr const char *kBossTag = "Boss";
inline constexpr const char *kBossPartTag = "BossPart";
inline constexpr const char *kPlayerBulletTag = "PlayerBullet";

/// <summary>
/// 色付きパーツで覆われた球状のボス。
///
/// ・見た目: 内側のコア球（このオブジェクト自身）＋ハニカム状に並んだ球の殻（BossSphereCluster）
/// ・被弾  : IBossTargetQuery 経由で弾の線分を受け取り、付着と同色消去を行う
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

    /// <summary>
    /// 攻撃を受けたときの処理。
    /// このボスはHPを削って倒すのではなく「殻の球をすべて破壊する」のが撃破条件なので、
    /// ここでは怯みだけを適用する（球を減らすのは同色消去そのもの）
    /// </summary>
    void ApplyDamage(const DamageInfo &info) override;

    /// <summary>残っている殻の球の数（＝このボスにとってのHP）</summary>
    float GetHp() const override { return static_cast<float>(cluster_.GetOccupiedCount()); }

    /// <summary>撃破されたか（中心のコアを除く色付きの球がすべて無くなったか）</summary>
    bool IsDead() const override { return cluster_.GetOccupiedCount() <= 0; }

    /// ===================================================
    /// IBossTargetQuery
    /// ===================================================

    bool FindLockOnTarget(const LockOnRequest &request, LockOnResult &out) override;
    BulletHitResult RaycastAttach(const Hagine::Vector3 &worldStart, const Hagine::Vector3 &worldEnd, Color color) override;
    bool TryGetTargetPosition(const ShellCell &cell, Hagine::Vector3 &out) override;

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

    /// <summary>削れた球の割合（露出度 0〜1）。初期の球数が基準</summary>
    float GetExposure() const { return cluster_.GetExposure(); }

    /// <summary>
    /// 攻撃のスケーリングに使う露出度（0〜1）。
    /// 弾を付着させて塊を育てられるので、削り切れば 1.0 に到達する
    /// </summary>
    float GetNormalizedExposure() const;

    /// <summary>指定色の球の数（色残量ミニマップ用）</summary>
    int CountAliveParts(Color color) const { return cluster_.CountAlive(color); }

    /// <summary>怯み中か</summary>
    bool IsStaggered() const { return staggerTimer_ > 0.0f; }

    /// <summary>初期状態の球の数（＝最大HP相当）</summary>
    float GetMaxHp() const { return static_cast<float>(cluster_.GetInitialCount()); }
    const BossParameters &GetParameters() const { return parameters_; }
    const BossColorPalette &GetPalette() const { return palette_; }
    BossSphereCluster &GetCluster() { return cluster_; }

    /// <summary>ロックオン中の球を強調表示する（valid=false で解除）</summary>
    void SetLockOnHighlight(const ShellCell &cell, bool valid) { cluster_.SetHighlightedCell(cell, valid); }

    /// <summary>読み込むボスデータのID（jsons/Boss/[id].json）。Init より前に呼ぶこと</summary>
    void SetBossId(const std::string &bossId) { bossId_ = bossId; }

    /// <summary>殻・HPを初期状態へ戻す（デバッグ・リトライ用）</summary>
    void ResetBoss();

    /// <summary>
    /// 球の大きさの変更を反映する（軽い。球は作り直さない）。
    /// 帯そのものを変えた場合は RebuildShell() を使うこと
    /// </summary>
    void ApplyShellChanges();

    /// <summary>
    /// 殻を作り直す（帯や球の半径を変えたとき用）。消えた球はすべて復活する
    /// </summary>
    void RebuildShell();

    /// ===================================================
    /// 状態・攻撃（BossState / IBossAttack から使う操作）
    /// ===================================================

    /// <summary>状態の変更を要求する（切り替えは次の更新の先頭）</summary>
    void RequestState(BossStateId id) { stateMachine_.Request(id); }

    /// ===================================================
    /// 登場演出
    /// ===================================================

    /// <summary>登場演出を最初から再生する</summary>
    void BeginAppear();

    /// <summary>
    /// 登場演出を1フレーム進める
    /// </summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    /// <returns>bool: まだ演出中なら true</returns>
    bool UpdateAppear(float deltaTime);

    /// <summary>登場演出を終了して通常状態にする</summary>
    void EndAppear();

    /// <summary>登場演出の最中か（この間は被弾もロックオンも受け付けない）</summary>
    bool IsAppearing() const { return stateMachine_.GetCurrentId() == BossStateId::Appear; }

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
    /// <summary>見た目の外周半径（基本殻の球の表面まで）。接地高さや接触判定に使う</summary>
    float GetBodyRadius() const {
        return parameters_.Shell().shellRadius + cluster_.GetSphereRadius();
    }

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
    BossSphereCluster cluster_{}; // 殻を構成する球の集合

    float staggerTimer_ = 0.0f;  // 怯み残り時間（秒）

    ITargetLocator *pTargetLocator_ = nullptr;   // 狙う相手（非所有）
    IColorProvider *pColorProvider_ = nullptr;   // 相手の選択色（非所有）
    IDamageable *pTargetDamageSink_ = nullptr;   // 攻撃の当て先（非所有・未接続可）

    BossStateMachine stateMachine_{};            // 待機／攻撃／怯み／撃破
    BossAttackScheduler scheduler_{};            // 攻撃の選択と間隔（攻撃の所有者）
    IBossAttack *pCurrentAttack_ = nullptr;      // 進行中の攻撃（所有は scheduler_）

    Hagine::Vector3 homePosition_{};             // 初期位置（アリーナ中心・着地高さの基準）
    float spinAngle_ = 0.0f;                     // 自転の累積角（ラジアン）
    float staggerShakeTime_ = 0.0f;              // 怯み揺れの経過時間
    float appearTime_ = 0.0f;                    // 登場演出の経過時間

    bool drawGraphDebug_ = false; // 隣接グラフのデバッグ描画
    std::string paramOwnerLabel_; // GameParamHub の登録ラベル
};
