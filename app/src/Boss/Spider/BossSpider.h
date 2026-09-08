#pragma once
#include "src/Boss/Data/BossColorPalette.h"
#include "src/Boss/Data/BossParameters.h"
#include "src/Boss/Spider/BossSpiderLeg.h"
#include "src/Boss/Attack/IBossAttack.h"
#include "src/Interface/IBossTargetQuery.h"
#include "src/Interface/ITargetLocator.h"
#include "object/base/BaseObject.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

/// <summary>
/// 第2形態の蜘蛛。球体形態の中心にあった黒い球をそのまま胴として使い、
/// そこから色付きの球が連なった脚を8本生やす。
///
/// 歩行は決め打ちのアニメーションではなく、足の置き場所から毎フレーム作る:
///   ・足は地面に貼り付いたままで、胴が離れると踏み替える
///   ・隣り合う脚は同時に浮かせない（結果として対角の脚が交互に出る）
///   ・胴は足の平均位置に乗り、歩調に合わせて上下・左右に揺れる
/// これで「一定の周期で動いていないのに、なぜか噛み合っている」蜘蛛らしい歩き方になる。
/// </summary>
class BossSpider final : public Hagine::BaseObject, public IBossTargetQuery {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>胴と脚を組み立てる（生成直後は非表示）</summary>
    /// <param name="objectName">オブジェクト名</param>
    void Init(const std::string objectName) override;

    /// <summary>更新（歩行・足の踏み替え）</summary>
    void Update() override;

    /// <summary>描画（胴＋脚）</summary>
    void Draw(const Hagine::ViewProjection &viewProjection) override;

    /// <summary>インスペクタ表示</summary>
    void DrawImGui() override;

    /// <summary>
    /// 蜘蛛固有の項目だけを描く（基底のトランスフォーム等は含めない）。
    /// オブジェクトを選択しなくても触れるよう、シーンの「オブジェクト設定」窓からも呼ぶ
    /// </summary>
    void DrawGameplayImGui();

    /// <summary>
    /// 球体形態のコアを引き継いで変形を始める。
    /// 引き継いだ位置・大きさからそのまま始まるので、見た目は1つのコアが
    /// 「浮き上がる → 脚が生える → 着地する」と変わっていくように見える
    /// </summary>
    /// <param name="corePosition">球体形態のコアの位置</param>
    /// <param name="coreRadius">球体形態のコアの半径（0以下なら胴の半径から始める）</param>
    void Awaken(const Hagine::Vector3 &corePosition, float coreRadius);

    /// <summary>引っ込める（非表示にして動きも止める）</summary>
    void Hide();

    /// <summary>
    /// 引き継ぐ元（球体形態のコア）の現在値を教えておく。
    /// デバッグの「変形を再生」でも本番と同じ位置・大きさから始められるよう、
    /// シーンから毎フレーム渡してもらう
    /// </summary>
    /// <param name="position">コアの位置</param>
    /// <param name="radius">コアの半径</param>
    void SetCoreHandoff(const Hagine::Vector3 &position, float radius) {
        handoffPosition_ = position;
        handoffRadius_ = radius;
    }

    /// <summary>変形が始まっているか（出現していれば true）</summary>
    bool IsActive() const { return phase_ != Phase::Hidden; }

    /// <summary>登場の演出中か（黒帯とカメラ寄せを出す合図）</summary>
    bool IsIntroCinematic() const { return phase_ == Phase::Collapse || phase_ == Phase::Transform; }

    /// <summary>変形を終えて戦闘できる状態か</summary>
    bool IsBattleReady() const { return phase_ == Phase::Active; }

    /// <summary>撃破演出中か（黒帯とカメラ寄せを出す合図）</summary>
    bool IsDefeated() const { return phase_ == Phase::Defeated; }

    /// <summary>撃破演出が終わったか（コアが落ちきって間も置いた）</summary>
    bool IsDefeatFinished() const { return isDefeatFinished_; }

    /// <summary>まだ残っている脚の本数（0で撃破）</summary>
    int GetAliveLegCount() const;

    /// <summary>撃破演出を始める（脚が全部無くなったとき・デバッグボタン）</summary>
    void BeginDefeat();

    /// <summary>いまの段階の名前（デバッグUI用）</summary>
    const char *GetPhaseName() const;

    /// <summary>更新を止める／再開する（調整用）。止めているあいだも描画は続く</summary>
    /// <param name="paused">止めるなら true</param>
    void SetPaused(bool paused) { isPaused_ = paused; }

    /// <summary>更新を止めているか</summary>
    bool IsPaused() const { return isPaused_; }

    /// <summary>歩いて向かう相手を設定する（未設定ならその場で足踏みする）</summary>
    void SetTargetLocator(ITargetLocator *locator) { pTargetLocator_ = locator; }

    /// <summary>色パレットを差し替える（球体形態と同じ色にそろえるため）</summary>
    void SetPalette(const BossColorPalette &palette) { palette_ = palette; }

    /// <summary>
    /// どのボスデータから読み書きするかを決める（Init より前に呼ぶこと）。
    /// 球体形態と同じ jsons/Boss/&lt;bossId&gt;.json の "spider" を使う
    /// </summary>
    /// <param name="bossId">ボス識別子（例: "Boss01"）</param>
    void SetBossId(const std::string &bossId) { bossId_ = bossId; }

    /// <summary>JSONから読み直して組み直す</summary>
    void LoadParameters();

    /// <summary>いまの値をJSONへ書き出す（同じファイルの他の項目は消えない）</summary>
    void SaveParameters() const;


    /// ===================================================
    /// IBossTargetQuery（撃つ側からの窓口）
    /// ===================================================

    /// <summary>
    /// 弾の移動線分を渡して着弾を判定する。
    /// 当たった脚の先へ球を継ぎ足して脚を伸ばし、
    /// 先端に同色が規定数そろえばまとめて消す（基本の脚と関節は消えない）
    /// </summary>
    /// <param name="worldStart">線分の始点（前フレームの弾の位置）</param>
    /// <param name="worldEnd">線分の終点（現在の弾の位置）</param>
    /// <param name="color">弾の色</param>
    /// <returns>BulletHitResult: 当たったか・付着したか・消えたか</returns>
    BulletHitResult RaycastAttach(const Hagine::Vector3 &worldStart,
                                  const Hagine::Vector3 &worldEnd, Color color) override;

    /// <summary>
    /// 線分が最初に当たる点を返すだけの問い合わせ（付着も消去もしない）。
    /// 照準の射線から着弾地点を求めるのに使う
    /// </summary>
    /// <param name="worldStart">線分の始点（ワールド）</param>
    /// <param name="worldEnd">線分の終点（ワールド）</param>
    /// <param name="color">撃とうとしている色（色違いの飛翔弾はすり抜ける）</param>
    /// <param name="outPoint">最初に当たった点（ワールド）</param>
    /// <returns>bool: 当たれば true</returns>
    bool RaycastPoint(const Hagine::Vector3 &worldStart, const Hagine::Vector3 &worldEnd, Color color,
                      Hagine::Vector3 &outPoint) override;

    /// <summary>ソフトロックオンの対象（脚の球）を探す</summary>
    /// <param name="request">問い合わせ内容</param>
    /// <param name="out">見つかった対象</param>
    bool FindLockOnTarget(const LockOnRequest &request, LockOnResult &out) override;

    /// <summary>脚の球の現在のワールド座標を取得する（飛翔中の弾の追尾に使う）</summary>
    /// <param name="cell">対象（x=脚の番号 / y=付け根から数えた並び順）</param>
    /// <param name="out">ワールド座標</param>
    bool TryGetTargetPosition(const ShellCell &cell, Hagine::Vector3 &out) override;

    /// <summary>ロックオンの許容範囲（球体形態と同じ値をシーンから受け取っている）</summary>
    LockOnRange GetLockOnRange() const override {
        return LockOnRange{lockOn_.maxAngleDegrees, lockOn_.maxDistance};
    }

    /// <summary>
    /// 脚には強調表示の仕組みが無いので何もしない。
    /// 撃つ側は相手が球体形態か蜘蛛かを気にせず呼べる
    /// </summary>
    void SetLockOnHighlight(const ShellCell &, bool) override {}

    /// <summary>色の表示RGBAを取得する</summary>
    Hagine::Vector4 GetColorRgba(Color color) const override { return palette_.GetRgba(color); }

    /// <summary>この蜘蛛が使っている色</summary>
    const std::vector<Color> &GetUsedColors() const override { return palette_.GetUsedColors(); }

    /// <summary>
    /// 連鎖・演出・ロックオンの設定を渡す（球体形態と同じ値をそろえるため、シーンから配線する）
    /// </summary>
    /// <param name="chain">連鎖マッチの設定</param>
    /// <param name="effect">吸着・消滅の演出設定</param>
    /// <param name="lockOn">ソフトロックオンの設定</param>
    void SetBattleParams(const BossChainParams &chain, const BossEffectParams &effect,
                         const BossLockOnParams &lockOn) {
        chain_ = chain;
        effect_ = effect;
        lockOn_ = lockOn;
    }


    /// ===================================================
    /// 攻撃から蜘蛛を動かすための口
    /// ===================================================

    /// <summary>相手に当たったことを外へ知らせる関数の型</summary>
    /// <param name="center">当たり判定の中心（ワールド）</param>
    /// <param name="radius">有効半径</param>
    /// <param name="damage">ダメージ量</param>
    using HitCallback = std::function<void(const Hagine::Vector3 &center, float radius, float damage)>;

    /// <summary>
    /// 攻撃が当たったときの通知先を設定する。
    /// プレイヤーへのダメージはこちらから触らないので、受け側で処理してもらう
    /// </summary>
    void SetHitCallback(HitCallback callback) { hitCallback_ = std::move(callback); }

    /// <summary>攻撃の当たりを通知する（通知先が未設定なら何もしない）</summary>
    void ReportHit(const Hagine::Vector3 &center, float radius, float damage);

    /// <summary>胴の位置（揺れを含まない基準の位置）</summary>
    const Hagine::Vector3 &GetBodyPosition() const { return bodyPosition_; }

    /// <summary>胴の位置を直接置く（攻撃が動きを受け持つあいだに使う）</summary>
    void SetBodyPosition(const Hagine::Vector3 &position) { bodyPosition_ = position; }

    /// <summary>胴の向き（ラジアン）</summary>
    float GetBodyYaw() const { return bodyYaw_; }

    /// <summary>胴の向きを足す（回転攻撃で使う）</summary>
    void AddBodyYaw(float radians) { bodyYaw_ += radians; }

    /// <summary>指定の位置のほうへ即座に向き直る</summary>
    void FaceTowards(const Hagine::Vector3 &worldPoint);

    /// <summary>足を着いて立っているときの胴の高さ</summary>
    float GetStandHeight() const { return standHeight_; }

    /// <summary>
    /// 脚を胴の下へ畳む度合いを、時間をかけて変える（0で接地・1で真下）。
    /// 一気に切り替えると足がワープするので、必ず時間をかけること
    /// </summary>
    /// <param name="tuck">目標の畳み具合</param>
    /// <param name="duration">かける時間（秒）</param>
    void SetLegTuck(float tuck, float duration);

    /// <summary>
    /// 脚の折り具合を時間をかけて変える（1で膝を曲げた通常姿勢・0で真横に伸び切る）
    /// </summary>
    /// <param name="bend">目標の折り具合</param>
    /// <param name="duration">かける時間（秒）</param>
    void SetLegBend(float bend, float duration);

    /// <summary>いま脚の先が届く半径（広げた脚の攻撃範囲）</summary>
    float GetFootReach() const;

    /// <summary>足を今の胴のまわりへ置き直す（変形直後など、補間が要らないときだけ）</summary>
    void ReplantFeet();

    /// <summary>色つきの弾を1発撃つ</summary>
    /// <param name="direction">飛ばす向き（正規化していなくてよい）</param>
    /// <param name="params">弾のパラメータ</param>
    void FireBullet(const Hagine::Vector3 &direction, const BossSpiderShootParams &params);

    BossSpiderParams &GetParameters() { return parameters_; }

private:
    /// ===================================================
    /// private method
    /// ===================================================

    /// <summary>
    /// 脚を構成し直す（本数や球数を変えたとき）。
    /// 脚も球も破棄せず「増やす・隠す」だけで済ませる。実行中に D3D12 リソースを
    /// 解放すると、前フレームのGPUコマンドが参照中で落ちるため
    /// </summary>
    void RebuildLegs();

    /// <summary>進みたい向きを求める（相手が居なければゼロ＝その場で足踏み）</summary>
    Hagine::Vector3 CalcDesiredDirection() const;

    /// <summary>胴の位置と向きを進める</summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    /// <returns>Vector3: このフレームの進行方向（正規化済み。止まっていればゼロ）</returns>
    Hagine::Vector3 UpdateBodyMove(float deltaTime);

    /// <summary>脚の踏み替えを更新する</summary>
    void UpdateLegs(const Hagine::Vector3 &moveDirection, float deltaTime);

    /// <summary>足の平均位置と歩調から胴を上下・左右に揺らす</summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    /// <param name="isMoving">歩いているか（止まっているときは揺らさない）</param>
    /// <returns>Vector3: 揺れを含んだ、実際に描く胴の位置</returns>
    /// <param name="controlHeight">胴の高さをこちらで決めてよいか（攻撃中は攻撃が決めるので false）</param>
    Hagine::Vector3 UpdateBodyPosture(float deltaTime, bool isMoving, bool controlHeight);

    /// <summary>胴の最終的な位置から脚の球を並べ直す</summary>
    /// <param name="bodyPosition">揺れを含んだ胴の位置</param>
    /// <param name="growth">脚が何割生えているか（1で生えきり）</param>
    /// <param name="bend">姿勢の混ぜ具合（1で膝を曲げた通常姿勢）</param>
    void PlaceLegs(const Hagine::Vector3 &bodyPosition, float growth = 1.0f, float bend = 1.0f);

    /// <summary>変形（浮上→脚が生える→着地）を1フレーム進める</summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    void UpdateTransform(float deltaTime);

    /// <summary>撃破演出（ふらつきながら地面へ落ちる）を1フレーム進める</summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    void UpdateDefeat(float deltaTime);

    /// <summary>変形の前ぶり（ふらつきながら地面へ落ちる）を1フレーム進める</summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    void UpdateCollapse(float deltaTime);

    /// <summary>飛んでいる弾1発ぶん</summary>
    struct SpiderBullet {
        BossSphere *sphere = nullptr;  // 見た目（bulletPool_ が所有）
        Hagine::Vector3 position{};    // 現在位置（ワールド）
        Hagine::Vector3 velocity{};    // 速度
        Color color = Color::RED;      // 弾の色（同じ色を当てられると消える）
        float radius = 1.0f;           // 当たり判定と見た目の半径
        float life = 0.0f;             // 残り寿命（秒）
        float damage = 0.0f;           // 命中したときのダメージ
        float speed = 0.0f;            // 速さ（追尾で向きを変えても速さは保つ）
        float homingRate = 0.0f;       // 相手を追う強さ（0で追わない）
        float homingLeft = 0.0f;       // あと何秒追いかけるか
        bool active = false;           // 飛んでいるか
    };

    /// <summary>攻撃を選ぶ・始める・進める</summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    /// <returns>Vector3: このフレームの進行方向（脚の踏み替えに渡す）</returns>
    Hagine::Vector3 UpdateAttack(float deltaTime);

    /// <summary>距離と確率から次の攻撃を選ぶ</summary>
    IBossAttack *PickAttack();

    /// <summary>飛んでいる弾を進める</summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    void UpdateBullets(float deltaTime);

    /// <summary>
    /// 線分に当たっている飛翔弾を探す（同じ色のものだけが当たる）。
    /// 消す処理は呼び出し側が行うので、ここでは添字を返すだけにしてある
    /// </summary>
    /// <param name="outBulletIndex">当たった弾の添字（bullets_ の並び）</param>
    /// <param name="outPoint">当たった弾の位置</param>
    /// <returns>bool: 当たれば true</returns>
    bool FindBulletHit(const Hagine::Vector3 &worldStart, const Hagine::Vector3 &worldEnd, Color color,
                       int &outBulletIndex, Hagine::Vector3 &outPoint) const;

    /// <summary>線分がいちばん手前で当たった脚を探す（脚は色に関係なく弾を止める）</summary>
    /// <param name="outLegIndex">当たった脚の番号</param>
    /// <param name="outSphereIndex">当たった球の並び順（付け根から数えた番号）</param>
    /// <param name="outPoint">着弾位置</param>
    /// <returns>bool: 当たれば true</returns>
    bool FindLegHit(const Hagine::Vector3 &worldStart, const Hagine::Vector3 &worldEnd, int &outLegIndex,
                    int &outSphereIndex, Hagine::Vector3 &outPoint) const;

    /// <summary>相手までの水平距離（相手がいなければ負の値）</summary>
    float CalcTargetDistance() const;

    /// <summary>脚の姿勢（畳み・折り）の補間を1フレームぶん進める</summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    void UpdateLegPosture(float deltaTime);

    /// <summary>変形を最後まで飛ばして戦闘できる状態にする（デバッグ用）</summary>
    void SkipTransform();

    /// <summary>いま足が置かれている高さの平均（胴をどこに乗せるかの基準）</summary>
    float CalcFootAverageHeight() const;

    /// <summary>変形を始めてから、脚が生え始めるまでの時間（秒）</summary>
    float CalcGrowStartTime() const;

    /// <summary>変形を始めてから、関節が折れ始めるまでの時間（秒）</summary>
    float CalcBendStartTime() const;

    /// <summary>変形にかかる合計時間（秒）。3つの動きが重なるぶん、単純な和より短い</summary>
    float CalcTransformDuration() const;

    /// ===================================================
    /// private variables
    /// ===================================================

    /// <summary>球体形態から蜘蛛になるまでの段階</summary>
    enum class Phase {
        Hidden,    // 未出現
        Collapse,  // 変形の前ぶり（コアがふらつきながら地面へ落ちる）
        Transform, // 変形中（浮き上がり・脚が生える・関節が折れる が重なって進む）
        Active,    // 変形完了（歩き回る）
        Defeated,  // 撃破（ふらつきながらコアが地面へ落ちる）
    };

    BossSpiderParams parameters_{}; // 見た目と歩行のパラメータ
    BossColorPalette palette_{};    // 脚の色

    /// <summary>脚の上限（プールの上限。ここまでは作られたら使い回す）</summary>
    static constexpr int kMaxLegCount = 16;

    std::vector<std::unique_ptr<BossSpiderLeg>> legs_{}; // 脚（所有・使い回す）
    int activeLegCount_ = 0;                             // 実際に使っている脚の本数

    BossChainParams chain_{};   // 連鎖マッチの設定（球体形態と共通）
    BossEffectParams effect_{};  // 吸着・消滅の演出設定
    BossLockOnParams lockOn_{}; // ソフトロックオンの設定（球体形態と共通）


    // --- 攻撃 ---
    /// <summary>attacks_ の並び順（PickAttack から引くのに使う）</summary>
    static constexpr size_t kAttackLeap = 0;  // 跳ねまわる
    static constexpr size_t kAttackShoot = 1; // 弾を撃つ
    static constexpr size_t kAttackWhirl = 2; // 回転して接近

    std::vector<std::unique_ptr<IBossAttack>> attacks_{}; // 使える攻撃（所有）
    IBossAttack *pCurrentAttack_ = nullptr;               // 進行中の攻撃（非所有）
    float attackCoolDown_ = 0.0f;                         // 次の攻撃までの残り時間（秒）
    HitCallback hitCallback_{};                           // 当たりの通知先（未設定なら通知しない）

    // --- 弾 ---
    std::vector<std::unique_ptr<BossSphere>> bulletPool_{}; // 弾の球（所有・増やすだけ）
    std::vector<SpiderBullet> bullets_{};                   // 弾の状態（プールと同じ並び）

    ITargetLocator *pTargetLocator_ = nullptr; // 歩いて向かう相手（非所有）

    Hagine::Vector3 bodyPosition_{}; // 胴の位置（足の平均から高さを決める前の基準）
    float bodyYaw_ = 0.0f;           // 胴の向き（ラジアン）
    float walkPhase_ = 0.0f;         // 歩調（胴の揺れに使う）
    Phase phase_ = Phase::Hidden;    // 変形の段階
    float transformTime_ = 0.0f;     // 変形を始めてからの経過時間（秒）
    float startRadius_ = 0.0f;       // 引き継いだコアの半径
    float startHeight_ = 0.0f;       // 引き継いだコアの高さ
    float standHeight_ = 0.0f;       // 足を着いて立ったときの胴の高さ

    Hagine::Vector3 handoffPosition_{}; // 引き継ぐ元のコアの位置（シーンが毎フレーム更新）
    float handoffRadius_ = 0.0f;        // 引き継ぐ元のコアの半径
    // 脚の姿勢は必ず時間をかけて変える（一気に変えると足がワープする）
    float legTuck_ = 0.0f;           // いまの畳み具合
    float legTuckFrom_ = 0.0f;       // 変え始めたときの畳み具合
    float legTuckTarget_ = 0.0f;     // 目標の畳み具合
    float legTuckTimer_ = 0.0f;      // 経過時間（秒）
    float legTuckDuration_ = 0.2f;   // かける時間（秒）

    float legBend_ = 1.0f;           // いまの折り具合（1で通常・0で真横）
    float legBendFrom_ = 1.0f;       // 変え始めたときの折り具合
    float legBendTarget_ = 1.0f;     // 目標の折り具合
    float legBendTimer_ = 0.0f;      // 経過時間（秒）
    float legBendDuration_ = 0.5f;   // かける時間（秒）

    float defeatTime_ = 0.0f;        // 撃破演出の経過時間（秒）
    float defeatStartHeight_ = 0.0f; // 落ち始めたときのコアの高さ
    float collapseTime_ = 0.0f;      // 変形前ぶりの経過時間（秒）
    bool isDefeatFinished_ = false;  // 撃破演出が終わったか
    bool isPaused_ = false;          // 更新を止めているか（調整用）
    std::string legNamePrefix_;      // 脚の球の名前の接頭辞
    std::string bossId_ = "Boss01";  // パラメータの読み書き先（球体形態と同じファイル）
};
