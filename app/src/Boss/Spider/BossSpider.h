#pragma once
#include "src/Boss/Data/BossColorPalette.h"
#include "src/Boss/Data/BossParameters.h"
#include "src/Boss/Spider/BossSpiderLeg.h"
#include "src/Interface/ITargetLocator.h"
#include "object/base/BaseObject.h"
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
class BossSpider final : public Hagine::BaseObject {
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

    /// <summary>変形が始まっているか（出現していれば true）</summary>
    bool IsActive() const { return phase_ != Phase::Hidden; }

    /// <summary>変形を終えて戦闘できる状態か</summary>
    bool IsBattleReady() const { return phase_ == Phase::Active; }

    /// <summary>いまの段階の名前（デバッグUI用）</summary>
    const char *GetPhaseName() const;

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
    Hagine::Vector3 UpdateBodyPosture(float deltaTime, bool isMoving);

    /// <summary>胴の最終的な位置から脚の球を並べ直す</summary>
    /// <param name="bodyPosition">揺れを含んだ胴の位置</param>
    /// <param name="growth">脚が何割生えているか（1で生えきり）</param>
    /// <param name="bend">姿勢の混ぜ具合（1で膝を曲げた通常姿勢）</param>
    void PlaceLegs(const Hagine::Vector3 &bodyPosition, float growth = 1.0f, float bend = 1.0f);

    /// <summary>変形（浮上→脚が生える→着地）を1フレーム進める</summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    void UpdateTransform(float deltaTime);

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
        Transform, // 変形中（浮き上がり・脚が生える・関節が折れる が重なって進む）
        Active,    // 変形完了（歩き回る）
    };

    BossSpiderParams parameters_{}; // 見た目と歩行のパラメータ
    BossColorPalette palette_{};    // 脚の色

    /// <summary>脚の上限（プールの上限。ここまでは作られたら使い回す）</summary>
    static constexpr int kMaxLegCount = 16;

    std::vector<std::unique_ptr<BossSpiderLeg>> legs_{}; // 脚（所有・使い回す）
    int activeLegCount_ = 0;                             // 実際に使っている脚の本数

    ITargetLocator *pTargetLocator_ = nullptr; // 歩いて向かう相手（非所有）

    Hagine::Vector3 bodyPosition_{}; // 胴の位置（足の平均から高さを決める前の基準）
    float bodyYaw_ = 0.0f;           // 胴の向き（ラジアン）
    float walkPhase_ = 0.0f;         // 歩調（胴の揺れに使う）
    Phase phase_ = Phase::Hidden;    // 変形の段階
    float transformTime_ = 0.0f;     // 変形を始めてからの経過時間（秒）
    float startRadius_ = 0.0f;       // 引き継いだコアの半径
    float startHeight_ = 0.0f;       // 引き継いだコアの高さ
    float standHeight_ = 0.0f;       // 足を着いて立ったときの胴の高さ
    std::string legNamePrefix_;      // 脚の球の名前の接頭辞
    std::string bossId_ = "Boss01";  // パラメータの読み書き先（球体形態と同じファイル）
};
