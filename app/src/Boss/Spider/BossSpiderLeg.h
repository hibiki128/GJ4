#pragma once
#include "src/Boss/Data/BossColorPalette.h"
#include "src/Boss/Data/BossParameters.h"
#include "src/Boss/Shell/BossSphere.h"
#include <memory>
#include <string>
#include <vector>

namespace Hagine {
class ViewProjection;
}

/// <summary>
/// 蜘蛛の脚1本。色付きの球が連なって出来ている。
///
/// 足先は「地面に置いた点」をワールド座標で覚えておき、胴が離れたら踏み替える。
/// 胴と足先が決まれば、2本の骨（付け根→膝、膝→足先）の姿勢は
/// 三角形の解として一意に決まるので、そこへ球を等間隔に並べている。
/// 膝を持ち上げる向きへ折ることで、蜘蛛らしい「への字」の脚になる。
/// </summary>
class BossSpiderLeg {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>
    /// 脚を構成し直す。
    ///
    /// 球は決して破棄せず、足りなければ増やし、余ったら隠すだけにしている。
    /// 実行中に D3D12 リソースを解放すると、前フレームのGPUコマンドがまだ
    /// それを参照していて「使用中リソースの解放」で落ちるため
    /// （エンジンはシーン遷移時にだけ GPU 完了を待ってから破棄している）。
    /// </summary>
    /// <param name="namePrefix">球の名前の接頭辞（一意にすること）</param>
    /// <param name="legIndex">脚の番号（0〜legCount-1）</param>
    /// <param name="legCount">脚の総数</param>
    /// <param name="params">蜘蛛のパラメータ</param>
    /// <param name="palette">色パレット</param>
    void Configure(const std::string &namePrefix, int legIndex, int legCount,
                   const BossSpiderParams &params, const BossColorPalette &palette);

    /// <summary>この脚を使うかどうか（本数を減らしたときに余った脚を隠す）</summary>
    /// <param name="hidden">隠すなら true</param>
    void SetHidden(bool hidden);

    /// <summary>足先を地面の初期位置へ置く</summary>
    /// <param name="bodyPosition">胴の位置</param>
    /// <param name="bodyYaw">胴の向き（ラジアン）</param>
    /// <param name="params">蜘蛛のパラメータ</param>
    void ResetFoot(const Hagine::Vector3 &bodyPosition, float bodyYaw, const BossSpiderParams &params);

    /// <summary>
    /// 足の踏み替えと球の配置を更新する
    /// </summary>
    /// <param name="bodyPosition">胴の位置</param>
    /// <param name="bodyYaw">胴の向き（ラジアン）</param>
    /// <param name="moveDirection">進行方向（正規化済み。止まっていればゼロ）</param>
    /// <param name="params">蜘蛛のパラメータ</param>
    /// <param name="canStartStep">今このフレームに踏み出してよいか（隣の脚と同時に浮かせないための制御）</param>
    /// <param name="deltaTime">経過時間（秒）</param>
    void Update(const Hagine::Vector3 &bodyPosition, float bodyYaw, const Hagine::Vector3 &moveDirection,
                const BossSpiderParams &params, bool canStartStep, float deltaTime);

    /// <summary>脚を描画する</summary>
    void Draw(const Hagine::ViewProjection &viewProjection);

    /// ===================================================
    /// getter
    /// ===================================================

    /// <summary>足先の現在位置（ワールド）</summary>
    const Hagine::Vector3 &GetFootPosition() const { return footPosition_; }

    /// <summary>今この脚が浮いているか</summary>
    bool IsStepping() const { return isStepping_; }

    /// <summary>この脚を構成する球の数（実際に使っている数）</summary>
    int GetSphereCount() const { return activeSphereCount_; }

    /// <summary>付け根→膝に並んでいる球の数（膝を含む）</summary>
    int GetUpperSphereCount() const { return upperSphereCount_; }

    /// <summary>膝→足先に並んでいる球の数（膝を含む）</summary>
    int GetLowerSphereCount() const { return lowerSphereCount_; }

    /// <summary>
    /// 付け根→膝（上腿）に並べる球の数を決める。
    /// パラメータが0以下なら、球がちょうど接して連なる数を上腿の長さから求める
    /// </summary>
    /// <param name="params">蜘蛛のパラメータ</param>
    static int ResolveUpperSphereCount(const BossSpiderParams &params);

    /// <summary>膝→足先（下腿）に並べる球の数を決める</summary>
    /// <param name="params">蜘蛛のパラメータ</param>
    static int ResolveLowerSphereCount(const BossSpiderParams &params);

    /// <summary>脚1本に使う球の数（膝の球は上腿と下腿で共有するので合計から1つ引く）</summary>
    /// <param name="params">蜘蛛のパラメータ</param>
    static int ResolveSphereCount(const BossSpiderParams &params);

    /// <summary>
    /// 付け根→膝→足先の折れ線の全長。
    /// 上腿と下腿はこの長さを球の数の比で分け合うので、膝の位置がここから決まる
    /// </summary>
    /// <param name="params">蜘蛛のパラメータ</param>
    static float ResolvePathLength(const BossSpiderParams &params);

    /// <summary>
    /// いま並んでいる球の間隔（上腿・下腿で共通）。
    /// 球の直径より広ければ隙間が空き、狭ければ重なる
    /// </summary>
    float GetSphereSpacing() const { return sphereSpacing_; }

    /// <summary>
    /// 脚を胴のまわりのどの向きに生やすかを決める（胴の正面を0とした角度・ラジアン）。
    /// 等間隔に並べたあと、左右それぞれの真横を中心にずれを縮めることで、
    /// legSpread が小さいほど脚が左右へ密集する
    /// </summary>
    /// <param name="legIndex">脚の番号（0〜legCount-1）</param>
    /// <param name="legCount">脚の総数</param>
    /// <param name="params">蜘蛛のパラメータ</param>
    static float ResolveAzimuth(int legIndex, int legCount, const BossSpiderParams &params);

private:
    /// ===================================================
    /// private method
    /// ===================================================

    /// <summary>胴に対する足の定位置（ワールド）を求める</summary>
    Hagine::Vector3 CalcHomePosition(const Hagine::Vector3 &bodyPosition, float bodyYaw,
                                     const BossSpiderParams &params) const;

    /// <summary>脚の付け根（胴の表面）の位置を求める</summary>
    Hagine::Vector3 CalcHipPosition(const Hagine::Vector3 &bodyPosition, float bodyYaw,
                                    const BossSpiderParams &params) const;

    /// <summary>付け根・膝・足先を通るように球を並べ直す</summary>
    void PlaceSpheres(const Hagine::Vector3 &hip, const BossSpiderParams &params);

    /// <summary>球を必要数まで用意する（増やすだけ。減らすときは隠す）</summary>
    void EnsureSpheres(const std::string &namePrefix, int count, float radius);

    /// ===================================================
    /// private variables
    /// ===================================================

    std::vector<std::unique_ptr<BossSphere>> spheres_{}; // 脚を構成する球（付け根→足先の順・使い回す）
    int activeSphereCount_ = 0;                          // 実際に使っている球の数
    int upperSphereCount_ = 0;                           // うち付け根→膝の数（膝を含む）
    int lowerSphereCount_ = 0;                           // うち膝→足先の数（膝を含む）
    float sphereSpacing_ = 0.0f;                         // いまの球の間隔（上腿・下腿で共通）
    bool isHidden_ = false;                              // 本数を減らして余った脚か

    int legIndex_ = 0;      // 脚の番号
    float azimuth_ = 0.0f;  // 胴を上から見たときの、脚の向き（ラジアン）

    Hagine::Vector3 footPosition_{}; // 足先の現在位置（ワールド）
    Hagine::Vector3 stepFrom_{};     // 踏み替えの開始位置
    Hagine::Vector3 stepTo_{};       // 踏み替えの着地位置
    float stepTimer_ = 0.0f;         // 踏み替えの経過時間
    bool isStepping_ = false;        // 浮いている最中か
};
