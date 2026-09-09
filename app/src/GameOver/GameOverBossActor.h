#pragma once
#include "src/Boss/Boss.h"
#include "src/Boss/Spider/BossSpider.h"
#include "src/GameOver/GameOverContext.h"
#include "debug/param/GameParamHub.h"
#include <memory>
#include <string>

namespace Hagine {
class BaseObjectManager;
}

/// <summary>
/// ゲームオーバー画面で見下ろしてくる、勝ったほうのボス。
///
/// 形態は「負けた瞬間に出ていたほう」を使う（GameOverContext から受け取る）。
/// どちらの形態も見た目を作り直さず、ゲーム中と同じ Boss / BossSpider をそのまま置く。
/// 攻撃や状態遷移は要らないので止めておき、動きはここから外づけで与える:
///
///   ・第1形態（球体）: 止めたうえで、ゆっくり回りながら浮き沈みし、
///                      殻の膨らみが上から下へ波のように流れていく
///   ・第2形態（蜘蛛）: 立たせたまま攻撃だけ止め、その場で足踏みし続ける
///
/// 蜘蛛のほうは足の踏み替えを生きたまま使っているので、胴を左右へ揺らすだけで
/// 脚が1本ずつ勝手に踏み替わる。これが一番それらしく見える。
///
/// 殻の波はワールドの上から下へ流したいので、ボスの回転の逆を掛けた向きを
/// 毎フレーム渡している。こうしないと、回るのに合わせて波の向きまで回ってしまう。
///
/// 両形態とも最初に作っておき、出すほうだけ表示する。実行中に作り直すと
/// 前フレームのGPUコマンドが参照中のリソースを解放してしまうため、
/// 確認用の切り替え（インスペクタ）も「作り直し」ではなく「表示の入れ替え」で行う。
/// </summary>
class GameOverBossActor {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>
    /// 両形態のボスを作り、オブジェクトマネージャーへ預けてから、出すほうを決める。
    /// 蜘蛛は足の位置を先に決める必要があるので、立たせる場所と向く先はここで受け取る
    /// </summary>
    /// <param name="form">最初に出す形態</param>
    /// <param name="groundPosition">立たせる場所（地面）</param>
    /// <param name="lookTarget">向く先（倒れたプレイヤー）</param>
    /// <param name="objectManager">登録先</param>
    void Init(BossFormId form, const Hagine::Vector3 &groundPosition,
              const Hagine::Vector3 &lookTarget, Hagine::BaseObjectManager *objectManager);

    /// <summary>調整パラメータをデバッグUIへ登録する（Init の後に一度だけ）</summary>
    void RegisterParams();

    /// <summary>
    /// 出す形態を入れ替える。立ち位置と向く先は先に渡しておくこと
    /// （蜘蛛はその場で足を置き直すため）
    /// </summary>
    /// <param name="form">出す形態</param>
    void SetForm(BossFormId form);

    /// <summary>
    /// 動きを進める。オブジェクトの更新より前（シーンの Update）から呼ぶこと。
    /// ここで置いた値をボス自身の更新がその場で使う
    /// </summary>
    void Update();

    /// <summary>殻の形をGPUで作り直す（第1形態を出しているときだけ意味がある）</summary>
    void DispatchCompute();

    /// <summary>
    /// 立っている場所を変える。蜘蛛は足を置き直さないと、
    /// 離れた場所へ脚だけ取り残される
    /// </summary>
    void SetGroundPosition(const Hagine::Vector3 &position);

    /// <summary>倒れている相手（プレイヤー）の場所を教える。そちらを向いて見下ろす</summary>
    void SetLookTarget(const Hagine::Vector3 &position);

    /// <summary>出している形態</summary>
    BossFormId GetForm() const { return form_; }

    /// <summary>状態の確認用UI</summary>
    void DrawImGui();

private:
    /// ===================================================
    /// private method
    /// ===================================================

    /// <summary>蜘蛛を、いまの立ち位置へ立った状態で置き直す</summary>
    void StandSpider();

    /// <summary>蜘蛛を「相手の向き＋調整ぶん」へ向け直し、足も置き直す</summary>
    void FaceSpider();

    /// <summary>第1形態（球体）の動き</summary>
    void UpdateSphereForm(float deltaTime);

    /// <summary>第2形態（蜘蛛）の動き</summary>
    void UpdateSpiderForm(float deltaTime);

    /// ===================================================
    /// private variables
    /// ===================================================

    BossFormId form_ = BossFormId::Sphere;
    std::unique_ptr<Boss> sphere_;       // 第1形態（両方作っておき、出すほうだけ表示する）
    std::unique_ptr<BossSpider> spider_; // 第2形態

    Hagine::Vector3 groundPosition_ = {0.0f, 0.0f, 0.0f}; // 立っている場所（地面）
    Hagine::Vector3 lookTarget_ = {0.0f, 0.0f, 0.0f};     // 見下ろす相手
    float motionTime_ = 0.0f;                             // 動きの経過時間（秒）

    // 向きの調整はUIから動かされる。足を置き直すのは変わったときだけでよいので、
    // 反映済みの角度を控えておいて食い違いを見る
    float appliedFacingDegrees_ = 0.0f;

    // --- 調整パラメータ（第1形態）---
    float sphereHover_ = 0.35f;      // 浮き沈みの大きさ
    float sphereHoverPeriod_ = 4.5f; // 浮き沈みの周期（秒）
    float sphereSpinSpeed_ = 8.0f;   // 回る速さ（度/秒）
    float sphereLift_ = 0.0f;        // 地面からの底上げ（見上げる画にしたいとき用）
    // 殻の伸縮。1.0 が本来の位置で、大きいほどコアから離れて殻がばらける
    float shellExpandMin_ = 0.95f;   // いちばん縮んだときの倍率
    float shellExpandMax_ = 1.65f;   // いちばん広がったときの倍率
    float shellWavePeriod_ = 4.0f;   // 波がひと巡りする時間（秒）。大きいほどゆっくり流れる
    float shellWaveCount_ = 1.0f;    // 上から下までに入る波の数（1で常にどこか1か所が膨らむ）

    // --- 調整パラメータ（第2形態）---
    // 足踏みは「胴を揺らす → 定位置から離れた脚が踏み替わる」で出す。
    // 揺れ幅は蜘蛛の stepTrigger（足が離れたら踏み替える距離）より大きく取ること
    float stepSwayRadius_ = 1.1f;  // 体重を移す幅（脚が踏み替わる元）
    float stepSwayPeriod_ = 6.0f;  // 左右をひと往復する時間（秒）
    float legStanceBend_ = 1.0f;   // 立っているときの脚の折り具合（1で通常・0で真横に伸び切る）
    float facingDegrees_ = 0.0f;   // 相手を向いた状態からの回し量（度）。正面を見せるのに使う

    // 登録した調整パラメータは破棄時にまとめて解除する（形態ごとに分ける）
    Hagine::GameParamOwner sphereParams_{"GameOver/Boss/Sphere"};
    Hagine::GameParamOwner spiderParams_{"GameOver/Boss/Spider"};
};
