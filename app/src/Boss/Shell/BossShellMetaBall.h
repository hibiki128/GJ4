#pragma once
#include "src/Boss/Data/BossColorPalette.h"
#include "src/Boss/Data/BossParameters.h"
#include "metaball/MetaBall.h"
#include "metaball/MetaBallGpuField.h"
#include "type/Vector3.h"
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace Hagine {
class BaseObject;
class Object3d;
class ViewProjection;
class WorldTransform;
} // namespace Hagine

/// <summary>
/// 殻の見た目を作る部分。色ごとに1枚の融合メッシュを持つ。
///
/// 球を1個ずつ描く代わりに、同じ色の球の密度場をまとめて Marching Cubes で
/// 三角形化する。近い球同士は自然に繋がるので、殻は水滴のような1つの塊に見える。
/// **別の色とは密度を足さない**ので、くっつくのは同じ色同士だけになる。
///
/// メッシュはボスのローカル空間で作り、描画時にボスの位置・回転を掛ける。
/// ボスが自転してもメッシュはそのまま使える。
///
/// 生成は既定で GPU（コンピュートシェーダー）。毎フレーム作り直しても CPU 時間を使わないので、
/// 殻を脈打たせるといった「形が動き続ける」表現ができる。
/// CPU 版も残してあり、パラメータで切り替えられる（GPU が使えない場合の逃げ道）。
/// </summary>
class BossShellMetaBall {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    ~BossShellMetaBall();

    /// <summary>
    /// 色ごとの描画先を用意する（2回目以降の呼び出しでは作り直さず設定だけ入れ直す）
    /// </summary>
    /// <param name="parent">親（ボス本体）。位置と回転だけを引き継ぐ</param>
    /// <param name="palette">色パレット</param>
    /// <param name="params">メタボールのパラメータ</param>
    /// <param name="maxBallCount">1色が持ちうる球の最大数（GPU バッファの確保に使う）</param>
    void Init(Hagine::BaseObject *parent, const BossColorPalette &palette,
              const BossMetaBallParams &params, int maxBallCount);

    /// <summary>
    /// 1色ぶんの球の中心（ボスのローカル座標）を差し替える
    /// </summary>
    /// <param name="color">色</param>
    /// <param name="localPositions">その色の球の中心</param>
    /// <param name="sphereRadius">球1個の半径（影響半径とセルの大きさの基準）</param>
    void SetElements(Color color, std::vector<Hagine::Vector3> localPositions, float sphereRadius);

    /// <summary>パラメータを変える（全色を作り直す）</summary>
    void SetParams(const BossMetaBallParams &params);

    /// <summary>マテリアルの色をパレットに合わせる</summary>
    void ApplyPalette(const BossColorPalette &palette);

    /// <summary>
    /// CPU 生成のときだけ、変化のあった色のメッシュを作り直す。
    /// GPU 生成のときは何もしない（毎フレーム DispatchCompute で作り直すため）
    /// </summary>
    void Update();

    /// <summary>
    /// GPU 生成の本体。DrawSystem のコンピュートフェーズ（シャドウより前）から呼ぶ。
    /// 色ごとに 密度場 → マーチングキューブス を走らせてメッシュを書き換える
    /// </summary>
    /// <param name="deltaTime">経過時間（秒）。脈動の時間を進めるのに使う</param>
    void DispatchCompute(float deltaTime);

    /// <summary>融合した殻を色ごとに描く</summary>
    /// <param name="viewProjection">ビュープロジェクション</param>
    void Draw(const Hagine::ViewProjection &viewProjection);

    /// ===================================================
    /// getter
    /// ===================================================

    /// <summary>GPU で生成しているか</summary>
    bool IsGpuMode() const { return params_.useGpu; }

    /// <summary>全色ぶんの三角形数（CPU 生成のときだけ実数。GPU では上限を返す）</summary>
    int GetTotalTriangleCount() const;

    /// <summary>直近に CPU で作り直したときの合計時間（ミリ秒）</summary>
    float GetLastBuildMilliseconds() const { return lastBuildMilliseconds_; }

    /// <summary>GPU 生成の規模（格子の大きさなど）</summary>
    const Hagine::MetaBallGpuStats &GetGpuStats() const { return gpuField_.GetStats(); }

private:
    /// <summary>色1つぶんの融合メッシュ</summary>
    struct ColorMesh {
        std::unique_ptr<Hagine::Object3d> obj3d{};      // 融合結果を描くモデル
        std::vector<Hagine::Vector3> localPositions{};  // その色の球の中心（ローカル）
        float sphereRadius = 0.0f;                      // 球1個の半径
        bool dirty = false;                             // 次の Update で作り直すか（CPU生成のみ）
        bool hasMesh = false;                           // 描けるメッシュがあるか
        Hagine::MetaBallBuildStats stats{};             // 直近の生成結果（CPU生成のみ）
    };

    /// ===================================================
    /// private method
    /// ===================================================

    /// <summary>色ごとの描画先モデルを作る（GPU/CPU で確保の仕方が違う）</summary>
    void CreateMeshes();

    /// <summary>1色ぶんのメッシュを CPU で作り直す</summary>
    void RebuildOnCpu(int colorIndex);

    /// <summary>動的モデルを ModelManager から取り除く</summary>
    void Release();

    /// ===================================================
    /// private variables
    /// ===================================================

    std::array<ColorMesh, kGameColorCount> meshes_{};
    // パレットの色。生成方法を切り替えてモデルを作り直したときに入れ直すのに使う
    std::array<Hagine::Vector4, kGameColorCount> colors_{};
    // メッシュはローカル空間なので、描画に使う行列はボスの位置・回転だけ（スケールは継承しない）
    std::unique_ptr<Hagine::WorldTransform> transform_{};
    BossMetaBallParams params_{};
    float lastBuildMilliseconds_ = 0.0f;

    // GPU 生成の場。色ごとに出力先を切り替えて使い回す
    Hagine::MetaBallGpuField gpuField_{};
    bool gpuReady_ = false;      // GPU 側のバッファを用意済みか
    bool createdAsGpu_ = false;  // いま持っているモデルが GPU 書き込み用か
    int maxBallCount_ = 0;       // 1色が持ちうる球の最大数
    float elapsedTime_ = 0.0f;   // 脈動に使う時間

    // 生成用の作業バッファ。毎回の確保を避けるため使い回す
    std::vector<Hagine::MetaBallElement> scratch_{};
    Hagine::MetaBallBuildStats emptyStats_{};
};
