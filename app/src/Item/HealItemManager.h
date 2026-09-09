#pragma once
#include "src/Character/ColorStruct.h"
#include "src/Interface/IShootableTargetQuery.h"
#include "src/Item/HealItem.h"
#include "type/Vector3.h"
#include "type/Vector4.h"
#include <memory>
#include <string>
#include <vector>

/// <summary>
/// 回復アイテムをまとめて受け持つ。
///
/// 敵が落とす想定なので、出すのは Spawn(位置) の1行だけで済むようにしてある。
/// どこからでも1行で呼べる必要があるため、BossParticles / PlayerParticles と同じく
/// シングルトンにしている。
///
/// アイテムは Init で決まった数だけ作り、BaseObjectManager へ「非所有登録」しておく。
/// 実体はこのクラスが unique_ptr で持ったまま、更新と描画だけをエンジンに任せる形で、
/// 出すときは待機中のものを使い回す（PlayerBulletManager と同じプール方式）。
///
/// 撃つ側から見ると、膜はボスの球と並ぶ「的」の1つなので、IShootableTargetQuery を実装して
/// 着弾・照準・ソフトロックオンの問い合わせを受ける。撃つ側はアイテムの正体を知らない。
///
/// ※ 非所有登録はシーンを切り替えても外れない（外れるのはアイテムを破棄したとき）。
///    シーンの初期化のたびに Init() を呼ぶこと。前のシーンのぶんは作り直しで捨てられる
/// </summary>
class HealItemManager final : public IShootableTargetQuery {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    static HealItemManager *GetInstance();

    /// <summary>
    /// アイテムのプールを作り直してオブジェクトマネージャーに登録する
    /// （シーンの初期化から毎回呼ぶ）
    /// </summary>
    /// <param name="baseName">名前のもと（マネージャーのキーになるので他と被らない名前にする）</param>
    void Init(const std::string &baseName = "HealItem");

    /// <summary>
    /// アイテムを片付ける（シーンの終了処理から呼ぶ）。
    ///
    /// 非所有登録はシーンを切り替えても外れないので、ここで捨てておかないと、
    /// 出したまま次のシーンへ移ったアイテムがゲームオーバー画面などに残ってしまう
    /// </summary>
    void Finalize();

    /// <summary>
    /// 回復アイテムを1つ出す。敵が落とすときはこれを呼ぶだけでよい。
    /// 出てくるのは膜に包まれた状態で、黄色い弾を当てるまで拾えない
    /// </summary>
    /// <param name="position">落とし主の位置（ワールド）。少し上へ出すのはこちらで行う</param>
    /// <returns>bool: 出せれば true（プールに空きが無ければ false）</returns>
    bool Spawn(const Hagine::Vector3 &position);

    /// ===================================================
    /// IShootableTargetQuery（撃つ側からの問い合わせ）
    /// ===================================================

    /// <summary>
    /// 弾の移動線分を渡して膜へ1発ぶんを当てる。
    /// 割れるのは規定の弾数を当ててからで、当たった弾は割れる前でも消える。
    /// 反応するのは黄色い弾だけで、他の色は素通りする
    /// </summary>
    bool RaycastHit(const Hagine::Vector3 &worldStart, const Hagine::Vector3 &worldEnd, Color color,
                    float bulletRadius) override;

    /// <summary>線分が最初に当たる膜の点を返す（副作用なし。照準とレティクルが使う）</summary>
    bool RaycastPoint(const Hagine::Vector3 &worldStart, const Hagine::Vector3 &worldEnd, Color color,
                      float bulletRadius, AimHit &outHit) override;

    /// <summary>ソフトロックオンの対象になる膜を探す（黄色を撃っているときだけ対象になる）</summary>
    bool FindLockOnTarget(const LockOnRequest &request, ShootableLockOnResult &out) override;

    /// <summary>ロックオン中の膜を強調表示する（指定以外の強調はここで解除される）</summary>
    void SetLockOnHighlight(int targetId, bool valid) override;

    /// ===================================================
    /// 配線（シーン側で行う）
    /// ===================================================

    /// <summary>
    /// 拾い手の位置の取得元を渡す。
    /// Player の型を知らないままにしておきたいので、関数越しにだけ触る
    /// </summary>
    void SetPlayerPositionGetter(HealItemPositionGetter getter) {
        hooks_.playerPositionGetter = std::move(getter);
    }

    /// <summary>
    /// 拾われたときの処理を渡す（実際に回復したら true を返すこと）。
    /// 満タンで false を返せば、アイテムは消えずにその場へ残る
    /// </summary>
    void SetPickupHandler(HealItemPickupHandler handler) {
        hooks_.pickupHandler = std::move(handler);
    }

    /// <summary>
    /// 膜の色味を渡す（ボスの色マスタの黄色に合わせるため）。
    /// 透け具合はこちらの調整値のままにする
    /// </summary>
    void SetSealColor(const Hagine::Vector4 &rgba);

    /// <summary>
    /// 更新を止める・再開する（ポーズ中に毎フレーム渡す）。
    /// アイテムの更新は BaseObjectManager が回しているので、シーンが return するだけでは止まらない
    /// </summary>
    void SetPaused(bool paused);

    /// <summary>調整値（実行中に触ると出ているアイテムにもその場で効く）</summary>
    HealItemParams &GetParams() { return params_; }
    const HealItemParams &GetParams() const { return params_; }

    /// <summary>いま出ているアイテムの数（デバッグ表示用）</summary>
    size_t GetActiveCount() const;

private:
    /// ===================================================
    /// private method
    /// ===================================================

    HealItemManager() = default;
    ~HealItemManager() override = default;
    HealItemManager(const HealItemManager &) = delete;
    HealItemManager &operator=(const HealItemManager &) = delete;

    /// <summary>
    /// 線分が最初に当たる膜を探す。着弾も照準も同じここを通るので、
    /// 「照準では当たる表示なのに弾は素通りする」というズレが出ない
    /// </summary>
    /// <param name="from">線分の始点（ワールド）</param>
    /// <param name="to">線分の終点（ワールド）</param>
    /// <param name="color">撃っている色（黄色でなければ誰にも当たらない）</param>
    /// <param name="bulletRadius">弾の半径（膜の半径に足して判定する）</param>
    /// <param name="outPoint">交点（ワールド）。不要なら nullptr</param>
    /// <returns>HealItem*: 手前で当たった膜（無ければ nullptr）</returns>
    HealItem *FindNearestSeal(const Hagine::Vector3 &from, const Hagine::Vector3 &to, Color color,
                              float bulletRadius, Hagine::Vector3 *outPoint);

    /// ===================================================
    /// private variables
    /// ===================================================

    // 同時に出せるアイテムの数
    static constexpr size_t kMaxItemCount = 8;

    std::vector<std::unique_ptr<HealItem>> items_;

    // アイテム全員が参照する調整値と、外への問い合わせ口
    HealItemParams params_{};
    HealItemHooks hooks_{};

    bool isPaused_ = false;
};
