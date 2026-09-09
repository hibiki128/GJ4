#pragma once
#include "object/base/BaseObject.h"
#include "type/Vector3.h"
#include "type/Vector4.h"
#include <functional>
#include <memory>
#include <string>

/// <summary>
/// 回復アイテムの調整値。
/// HealItemManager が1つだけ持ち、アイテムはポインタで見る。
/// 実行中に触っても全部のアイテムへ同時に効かせたいので、値を配らずに参照させている
/// </summary>
struct HealItemParams {
    float sealRadius = 1.2f;   // 膜の半径（黄色い弾を当てる的の大きさ）
    float coreRadius = 0.5f;   // ハートの大きさ（モデルの半径にかかる倍率）
    int sealHitPoints = 3;     // 膜を割るのに必要な弾数（1発ごとに薄くなる）
    float hitFlashTime = 0.12f;   // 膜に当たった瞬間に白く光っている時間（秒）
    float highlightScale = 1.08f; // ロックオン中に膜を大きく見せる倍率
    float breakTime = 0.25f;   // 膜が弾けて消え切るまでの時間（秒）
    float pickupRadius = 1.2f; // ハートの表面から、これだけ近づけば拾える
    float lifeTime = 30.0f;    // 出してから消えるまでの時間（秒）。0以下なら消えない
    float bobHeight = 0.25f;   // 膜の中でハートが上下に揺れる幅
    float bobSpeed = 2.0f;     // 上下に揺れる速さ（ラジアン/秒）
    float spinSpeed = 45.0f;   // ハートがその場で回る速さ（度/秒）
    float spawnHeight = 1.0f;  // 落とし主の位置から、どれだけ上へ出すか
    int healAmount = 1;        // 拾ったときに回復する量

    // 膜の色。黄色い弾で割るものなので黄色にそろえる（色味はボスの色マスタから配れる）
    Hagine::Vector4 sealRgba = {0.95f, 0.85f, 0.30f, 0.8f};
    // ハートの色
    Hagine::Vector4 coreRgba = {0.95f, 0.30f, 0.40f, 1.00f};
};

/// <summary>プレイヤーの現在位置を取得する関数（取れなければ false）</summary>
using HealItemPositionGetter = std::function<bool(Hagine::Vector3 &)>;

/// <summary>
/// 拾われたときに呼ばれる関数。実際に回復したときだけ true を返す。
/// 満タンで効かなかった場合に false を返せば、アイテムは消えずに残る
/// </summary>
using HealItemPickupHandler = std::function<bool()>;

/// <summary>
/// アイテムが外へ問い合わせる口。
/// パラメータと同じく HealItemManager が1つだけ持ち、アイテムはポインタで見る。
/// 配線がいつ来ても（Init の前でも後でも）効くようにするため、値を配らない
/// </summary>
struct HealItemHooks {
    HealItemPositionGetter playerPositionGetter{};
    HealItemPickupHandler pickupHandler{};
};

/// <summary>
/// 回復アイテム1個。
/// BaseObjectManager に「非所有登録」して使うので、Update / Draw はマネージャー側から呼ばれる
/// （ゲーム側から自前で呼ぶと二重更新になるので注意）。
///
/// 生成・破棄はせずプールで使い回す。BaseObject の生成はJSON探索を伴って重いうえ、
/// Player や弾の更新は BaseObjectManager::Update のループの中から呼ばれているため、
/// そこで登録・解除するとマネージャーが回している最中のコンテナを書き換えることになる。
///
/// 見た目は2つのオブジェクトでできている。
/// このクラス自身が中身のハート（拾う対象）で、それを包む黄色い膜は seal_ が持つ球。
/// 膜は定位置に留まり、ハートだけがその中で浮いて回る。膜が動かないので狙いも安定する。
///
/// 膜の当たり判定はコライダーを使わず「前フレームの位置→現在位置の線分」で行う。
/// 弾は1フレームで膜の直径以上進むため、重なり判定ではすり抜けてしまうのに対し、
/// 線分なら確実に当たる（PlayerBullet と同じ考え方）
/// </summary>
class HealItem final : public Hagine::BaseObject {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>アイテムが今どの状態か</summary>
    enum class State {
        None,   // 待機（プールの空き）
        Sealed, // 膜に包まれている（黄色い弾で割れる）
        Free    // 膜が割れて拾える
    };

    /// <summary>
    /// アイテムを生成する（待機状態で始まる）。
    /// 包む膜もここで一緒に作って登録する
    /// </summary>
    /// <param name="objectName">オブジェクト名（マネージャーのキーになるので一意にする）</param>
    /// <param name="params">調整値（マネージャーが持つものを参照する）</param>
    /// <param name="hooks">外への問い合わせ口（同上）</param>
    void InitItem(const std::string &objectName, const HealItemParams *params,
                  const HealItemHooks *hooks);

    /// <summary>膜つきの状態で出す（待機中のアイテムを有効化する）</summary>
    /// <param name="position">出す位置（ワールド。上へずらすのは呼び出し側の仕事）</param>
    void Spawn(const Hagine::Vector3 &position);

    /// <summary>待機状態へ戻す（プールへ返却する）</summary>
    void Deactivate();

    void Update() override;

    /// <summary>
    /// 膜と線分の交差を調べる（副作用は起こさない）。
    /// 弾の着弾にも、照準・ロックオンの問い合わせにも同じものを使うので、
    /// 「照準では当たる表示なのに弾は素通りする」というズレが出ない。
    /// 色の判定は呼び出し側（マネージャー）が済ませている
    /// </summary>
    /// <param name="from">線分の始点（ワールド）</param>
    /// <param name="to">線分の終点（ワールド）</param>
    /// <param name="outDistance">始点から交点までの距離（手前の膜を選ぶのに使う）</param>
    /// <param name="outPoint">交点（ワールド）</param>
    /// <returns>bool: 膜つきの状態で当たっていれば true</returns>
    bool RaycastSeal(const Hagine::Vector3 &from, const Hagine::Vector3 &to, float &outDistance,
                     Hagine::Vector3 &outPoint) const;

    /// <summary>
    /// 膜に1発当たったことにする。
    /// 残りが尽きたところで割れて、拾える状態になる
    /// </summary>
    void ApplySealHit();

    /// <summary>ロックオン中の強調表示を切り替える（膜つきのときだけ見た目に出る）</summary>
    void SetHighlight(bool highlight) { isHighlighted_ = highlight; }

    /// <summary>更新を止める・再開する（ポーズ中に配る）</summary>
    void SetPaused(bool paused) { isPaused_ = paused; }

    /// ===================================================
    /// getter
    /// ===================================================

    /// <summary>出ている最中かどうか（false ならプールの空き）</summary>
    bool IsActive() const { return state_ != State::None; }

    State GetState() const { return state_; }

    /// <summary>
    /// 膜の中心（ワールド）。撃つ側から見た的の中心はここで、
    /// ロックオンもエイムアシストもこの一点へ寄せる。中で揺れるハートとは違って動かない
    /// </summary>
    const Hagine::Vector3 &GetSealCenter() const { return basePosition_; }

    /// <summary>膜を割るのに、あと何発いるか</summary>
    int GetSealHp() const { return sealHp_; }

private:
    /// ===================================================
    /// private method
    /// ===================================================

    /// <summary>膜を割って拾える状態にする</summary>
    void BreakSeal();

    /// <summary>プレイヤーが届いていれば拾わせる</summary>
    /// <returns>bool: 拾われた（＝実際に回復した）なら true</returns>
    bool TryPickup();

    /// <summary>膜が弾けてからの進み具合（0＝割れた瞬間、1＝消え切った。膜つきのあいだは 0）</summary>
    float BurstProgress() const;

    /// <summary>ハートと膜の大きさ・色を今の状態に合わせる（調整値を実行中に触っても効く）</summary>
    void ApplyVisual();

    /// ===================================================
    /// private variables
    /// ===================================================

    // 調整値と外への口。実体はマネージャーが持つ
    const HealItemParams *params_ = nullptr;
    const HealItemHooks *hooks_ = nullptr;

    // ハートを包む黄色い膜。撃たれるのはこちらで、割れると消える
    std::unique_ptr<Hagine::BaseObject> seal_;

    State state_ = State::None;
    Hagine::Vector3 basePosition_{}; // 膜の中心（出した位置。揺れの基準でもある）
    int sealHp_ = 0;                 // 膜を割るのに、あと何発いるか
    float lifeTimer_ = 0.0f;         // 消えるまでの残り時間（秒）
    float breakTimer_ = 0.0f;        // 膜が弾け切るまでの残り時間（秒）
    float hitFlashTimer_ = 0.0f;     // 膜に当たって白く光っている残り時間（秒）
    float bobPhase_ = 0.0f;          // 上下の揺れの位相（ラジアン）
    float spinYaw_ = 0.0f;           // ハートの向き（ラジアン）
    bool isHighlighted_ = false;     // ロックオンで強調表示中か
    bool isPaused_ = false;          // 更新を止めているか（ポーズ中）
};
