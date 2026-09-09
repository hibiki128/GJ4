#pragma once
#include "src/Boss/Data/BossColorPalette.h"
#include "debug/param/GameParamHub.h"
#include "type/Vector3.h"
#include "type/Vector4.h"
#include <string>
#include <vector>

namespace Hagine {
class BaseObject;
class BaseObjectManager;
} // namespace Hagine

/// <summary>
/// フィールドの外側を埋める飾りの柱。
///
/// 行動範囲（Field）の外は床も無く、そのままだと背景のクリアカラーが素通しで見える。
/// そこを、ゲームで使っている4色のキューブでぐるりと囲んで塞ぐ。
///
/// 置き方は同心の輪を何重か重ねる形にしてある:
///   ・内側の輪は間を空けて低め（隙間から奥が見えるので、奥行きが出る）
///   ・外へ行くほど背が高く、隙間なく詰まる（いちばん外がクリアカラーを塞ぐ壁になる）
///   ・輪ごとに半周ぶん角度をずらすので、内側の隙間の真後ろには必ず外側の柱が来る
///
/// 柱そのものはシーンのオブジェクト（BaseObject）として作り、
/// SceneData/&lt;シーン名&gt;/ObjectDatas へ1個ずつJSONで保存する。つまり一度作れば
/// 次からはシーン読み込みがそのまま並べ直してくれるので、このクラスは
/// 「見つけた柱を揺らすだけ」になる。位置や色をエディタで直した場合もそれが残る。
///
/// 上下の揺れは transform ではなく描画オフセット（SetOffset）で出している。
/// 保存されるのは transform だけなので、揺れている途中に保存しても
/// 座標がじりじりずれていかない。
///
/// 柱だけでは上を向いたときの空を塞ぎきれない（見上げの限界まで振ると、
/// 向こう岸の柱の頭より上が抜ける）。塞ぐには 200 を超える高さが要って
/// 絵として無理があるので、外側に球を1枚かぶせて背景そのものを差し替えている。
/// 球は裏返して（スケールの符号を1軸だけ反転して）内側から見えるようにし、
/// ライティングもトゥーンも切ってある。1回の描画で済むので負荷は増えない。
/// </summary>
class FieldSurround {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>
    /// 調整パラメータを登録し、柱を用意する（シーンの初期化から1回だけ）。
    /// 保存済みの柱があればそれを引き取り、無ければその場で作ってJSONへ保存する
    /// </summary>
    /// <param name="fieldRadius">行動範囲の半径（この外側へ置く）</param>
    /// <param name="objectManager">柱の登録先。柱の所有者にもなる</param>
    /// <param name="sceneName">保存先のシーン名（SceneData/&lt;ここ&gt;/ObjectDatas へ書く）</param>
    void Init(float fieldRadius, Hagine::BaseObjectManager *objectManager,
              const std::string &sceneName);

    /// <summary>
    /// 行動範囲を持たないシーン（クリア・ゲームオーバー）が使う既定の半径。
    /// ゲームシーンの Field と同じ値にしてあるので、どの画面でも同じ広さの
    /// 場所に立っているように見える
    /// </summary>
    static constexpr float kDefaultFieldRadius = 50.0f;

    /// <summary>上下の揺れを進める（オブジェクトの更新より前に毎フレーム）</summary>
    void Update();

    /// <summary>作り直しと保存のUI（シーンの「オブジェクト設定」窓から呼ぶ）</summary>
    void DrawImGui();

    /// <summary>
    /// 柱を、カメラの遮蔽物リストへ足す。
    /// カメラが柱の中へ下がると裏面が抜けて背景が見えてしまうので、
    /// 追従カメラに「ここから先へは下がらない」と教えるために使う
    /// </summary>
    /// <param name="obstacles">足す先</param>
    void AppendObstacles(std::vector<Hagine::BaseObject *> &obstacles) const;

    /// <summary>並べてある柱の数</summary>
    int GetBlockCount() const { return static_cast<int>(blocks_.size()); }

private:
    /// ===================================================
    /// private method
    /// ===================================================

    /// <summary>
    /// 1本ぶんの柱。実体はオブジェクトマネージャーが持っているので、ここは非所有で見るだけ
    /// </summary>
    struct Block {
        Hagine::BaseObject *object = nullptr; // 柱の実体（非所有）
        float bobPhase = 0.0f;                // 揺れの位相（本ごとにずらす）
        float bobSpeed = 0.0f;                // 揺れの速さ（ラジアン/秒）
        float bobScale = 1.0f;                // 揺れの大きさの個体差（0〜1）
    };

    /// <summary>シーンに読み込まれている柱を探して引き取る</summary>
    /// <returns>bool: 1本でも見つかれば true</returns>
    bool AdoptSavedBlocks();

    /// <summary>柱を作り直す（足りなければ作り、余ったぶんは隠す）</summary>
    void Generate();

    /// <summary>いまの柱をすべて SceneData/GameScene/ObjectDatas へ保存する</summary>
    void SaveBlocks();

    /// <summary>見た目（テクスチャ・法線マップ・トゥーン）をそろえる</summary>
    /// <param name="object">対象の柱</param>
    void ApplyMaterial(Hagine::BaseObject *object) const;

    /// <summary>揺れ方を本ごとに散らす（見つけた順で決まるので毎回同じになる）</summary>
    /// <param name="block">対象の柱</param>
    /// <param name="index">通し番号</param>
    void AssignBobbing(Block &block, int index) const;

    /// <summary>柱の名前（&lt;接頭辞&gt;000 形式。JSONのファイル名にもなる）</summary>
    static std::string MakeName(int index);

    /// <summary>背景を塞ぐ球を用意する（無ければ作り、あれば引き取る）</summary>
    void SetupSkyDome();

    /// <summary>球の大きさと色を反映する</summary>
    void ApplySkyDome();

    /// ===================================================
    /// private variables
    /// ===================================================

    Hagine::BaseObjectManager *pObjectManager_ = nullptr; // 柱の持ち主（非所有）
    std::string saveFolder_{};                            // 柱の保存先（シーンごとに違う）
    BossColorPalette palette_{};                          // 色マスタ（4色）
    std::vector<Block> blocks_{};                         // 並べてある柱
    Hagine::BaseObject *pSkyDome_ = nullptr;              // 背景を塞ぐ球（非所有）
    float fieldRadius_ = 50.0f;                           // 行動範囲の半径
    float motionTime_ = 0.0f;                             // 揺れの経過時間（秒）

    // --- 調整パラメータ（並べ方。触ったら「作り直す」で反映）---
    int ringCount_ = 3;          // 輪の重ね数（外へ行くほど高く・詰まる）
    // 内側の輪は、カメラが引ける限界（ボスを収めるときの最大距離）より外へ置く。
    // 内側に置くとカメラが柱の中へ入り、裏面が抜けて背景が丸見えになる
    float innerMargin_ = 35.0f;  // 行動範囲のふちから内側の輪までの距離
    float ringSpacing_ = 16.0f;  // 輪と輪の間隔
    float fillInner_ = 0.80f;    // 内側の輪の詰まり具合（1.0で隣とちょうど接する）
    float fillOuter_ = 1.15f;    // いちばん外の輪の詰まり具合（1より大きいと重なって隙間が消える）
    float heightMin_ = 20.0f;    // 内側の輪の柱の高さ（低いほう）
    float heightMax_ = 36.0f;    // 内側の輪の柱の高さ（高いほう）
    float heightGrowth_ = 2.4f;  // 1つ外の輪へ移るときの高さの倍率
    float blockDepth_ = 9.0f;    // 柱の奥行き（フィールドから見た厚み）
    float sinkDepth_ = 8.0f;     // 柱を地面へ埋める深さ（揺れても足元に隙間が空かないように）
    float radiusJitter_ = 2.0f;  // 半径方向のばらつき
    float yawJitter_ = 10.0f;    // 向きのばらつき（度）
    float colorScale_ = 1.0f;    // 色の明るさ倍率（下げるとボスの殻の色と喧嘩しにくい）
    int seed_ = 20260909;        // 並べ方のシード（変えると別の並びになる）

    // --- 調整パラメータ（見た目と動き。触るとすぐ効く）---
    float bobAmount_ = 0.9f;     // 上下に揺れる大きさ
    float bobPeriodMin_ = 4.0f;  // 揺れの周期の下限（秒）
    float bobPeriodMax_ = 8.0f;  // 揺れの周期の上限（秒）
    float normalStrength_ = 8.0f; // 法線マップの強さ（床と同じ値にそろえてある）
    float uvScale_ = 2.0f;        // 法線マップの繰り返し回数

    // --- 調整パラメータ（背景を塞ぐ球）---
    float domeRadius_ = 450.0f;   // 球の大きさ（カメラの遠面より内側に収めること）
    Hagine::Vector4 domeColor_ = {0.09f, 0.11f, 0.18f, 1.0f}; // 空の色（柱が映えるよう暗めにしてある）

    // 登録した調整パラメータは破棄時にまとめて解除する（ポインタ失効を防ぐ）
    Hagine::GameParamOwner params_{"FieldSurround"};
};
