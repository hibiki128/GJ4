#pragma once
#include "src/Character/ColorStruct.h"
#include <cstdint>
#include <string>
#include <vector>

/// <summary>
/// 殻に関するパラメータ。
/// 基本殻は icosphere の頂点をそのまま使うためハニカム状に均等配置され、
/// 弾はその外側（外向きの層）へ積み上がっていく
/// </summary>
struct BossShellParams {
    int subdivision = 1;        // 基本殻の分割回数（0→12 / 1→42 / 2→162 個）
    float shellRadius = 3.0f;   // 基本殻の半径（球の中心までの距離）
    float sphereRadius = 0.0f;  // 球1個の半径（0以下なら隣同士が接する大きさを自動算出）
    // 内側へ付着を許す層数。層の間隔は球の直径なので、球が大きい構成（分割0〜1）では
    // 内側の層がコア球に埋もれて見えなくなる。既定は0（分割2以上なら1にできる）
    int innerLayers = 0;
    int outerLayers = 2;        // 外側へ付着を許す層数（弾が盛り上がる）
    float coreScale = 1.0f;     // コア球の大きさ（基本殻の内側に接する大きさに対する倍率）
    int extraCapacity = 0;      // 付着ぶんの球プール（0なら層数から自動算出）
};

/// <summary>
/// 殻の見た目（メタボール）に関するパラメータ。
///
/// 殻は球1個ずつを描くのではなく、同じ色の球の密度場をまとめて三角形化した
/// 1枚のメッシュとして描かれる。隣り合う同色の球は自然に融合してくっつく。
/// </summary>
struct BossMetaBallParams {
    // 影響半径 = 球の半径 × これ。しきい値0.5では「単体の見た目の半径 = 影響半径の半分」に
    // なるので、2.0 のとき見た目が球の半径とちょうど一致する。大きいほど太って強く繋がる
    float influenceScale = 2.0f;
    // 格子セル1辺の長さ = 球の半径 × これ。小さいほど滑らかだが生成が重い
    // （球42個・3色で 0.5→約1.3ms / 0.3→約3.5ms / 0.15→約22ms）
    float voxelRatio = 0.3f;
    float threshold = 0.5f;       // 等値面のしきい値。大きいほど痩せて繋がりにくくなる
    float highlightScale = 1.15f; // ロックオン強調で重ねる球の大きさ倍率（融合面から少し出す）

    // 生成をGPU（コンピュートシェーダー）で行うか。
    // GPU なら毎フレーム作り直しても CPU 時間を使わないので、脈動のような動く表現ができる。
    // false にすると CPU 生成に戻り、球が増減した色だけを作り直す（動かせないが確実に動く）
    bool useGpu = true;

    // 脈動（GPU生成のときだけ効く）。影響半径を時間で伸び縮みさせる
    float wobbleAmplitude = 0.0f; // 振幅（影響半径に対する割合）。0 で静止
    float wobbleSpeed = 3.0f;     // 速さ（ラジアン/秒）
    float wobbleFrequency = 1.0f; // 位置による位相のばらけ具合。大きいほど細かくうねる

    // GPU生成で1色あたりに出せる三角形の数の上限（頂点バッファの確保に使う）。
    // 既定値は分割2・セル細かさ0.15 でも足りる程度に取ってある
    int maxTrianglesPerColor = 30000;
};

/// <summary>
/// 連鎖マッチに関するパラメータ
/// </summary>
struct BossChainParams {
    int minMatch = 3;             // 破壊に必要な同色連結数
    float staggerBase = 0.6f;     // 連鎖成立時の基礎怯み時間（秒）
    float staggerPerPart = 0.15f; // 最低数を超えた1つごとの怯み加算（秒）
    // 初期配色で許す同色の塊の最大数（0以下で無制限）。
    // 弾を付着させて塊を育てる方式では、最初から minMatch 個そろっていると
    // 1発当てただけで消えてしまうため、既定は「消去に必要な数 - 1」にしている
    int maxInitialCluster = 2;
};

/// <summary>
/// 戦闘全体の挙動に関するパラメータ
/// </summary>
struct BossBattleParams {
    float arenaRadius = 25.0f;   // 初期位置を中心に、ボスが動ける範囲
    float idleSpinSpeed = 18.0f; // 待機中の自転速度（度/秒）。死角のパーツを見せるための緩やかな回転
};

/// <summary>
/// 露出度（削れたパーツの割合）に応じたスケーリング係数。
/// ボスごとにデータ化して難易度カーブを作る。
/// 各値は「露出度0のとき」と「露出度1のとき」の対で持ち、あいだは線形補間する。
/// </summary>
struct BossExposureParams {

    float attackIntervalAtZero = 6.0f;    // 露出度0のときの攻撃間隔（秒）
    float attackIntervalAtFull = 2.2f;    // 露出度1のときの攻撃間隔（秒）

    float spinDashSpeedScaleAtFull = 1.6f;  // 突進速度の倍率（露出度1）
    float spinTelegraphScaleAtFull = 0.6f;  // 突進の予兆時間の倍率（露出度1。短いほど厳しい）

    int slamCountAtZero = 1;                // 露出度0のときの落下回数
    int slamCountAtFull = 5;                // 露出度1のときの落下回数
    float slamAimScaleAtFull = 0.55f;       // 落下の狙い時間の倍率（露出度1）
};

/// <summary>
/// 攻撃1: 回転＆突進のパラメータ
/// </summary>
struct BossSpinAttackParams {
    float telegraphTime = 1.2f;        // 予兆（その場で回転を上げる）時間
    float telegraphSpinSpeed = 540.0f; // 予兆終盤の自転速度（度/秒）
    float dashSpeed = 22.0f;           // 突進速度
    float dashTime = 0.8f;             // 突進時間
    float recoverTime = 0.8f;          // 突進後の硬直
    float damage = 15.0f;              // 接触ダメージ
    float contactMargin = 1.0f;        // 本体半径への上乗せ（当たりの甘さ）
};

/// <summary>
/// 攻撃2: 飛び上がり→頭上落下のパラメータ
/// </summary>
struct BossSlamAttackParams {
    float riseTime = 0.8f;     // 飛び上がりにかける時間
    float riseHeight = 10.0f;  // 飛び上がる高さ
    float aimTime = 0.5f;      // 落下地点を追尾する時間（この間に逃げる）
    float fallTime = 0.35f;    // 落下時間
    float impactTime = 0.35f;  // 着弾後の静止時間
    float impactRadius = 4.5f; // 着弾の有効半径
    float damage = 20.0f;      // 着弾ダメージ
    float recoverTime = 0.8f;  // 最後の着弾後の硬直
};

/// <summary>
/// ソフトロックオンに関するパラメータ
/// </summary>
struct BossLockOnParams {
    float maxAngleDegrees = 20.0f; // 照準からの許容角度
    float maxDistance = 60.0f;     // 有効距離
    bool requireFacing = true;     // 射手側を向いている面だけを対象にするか
};

/// <summary>
/// ボス1体分のデータ（jsons/Boss/&lt;bossId&gt;.json）。
/// レベルデザインでの難易度調整は、コードではなくこのJSONで行う。
/// </summary>
class BossParameters {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>ボスデータを読み込む（ファイルが無ければ既定値のまま）</summary>
    /// <param name="bossId">ボス識別子（例: "Boss01"）</param>
    void Load(const std::string &bossId);

    /// <summary>現在の値をJSONへ書き戻す</summary>
    void Save() const;

    /// ===================================================
    /// getter / setter
    /// ===================================================

    const std::string &GetBossId() const { return bossId_; }


    uint32_t GetColorSeed() const { return colorSeed_; }
    void SetColorSeed(uint32_t seed) { colorSeed_ = seed; }

    const std::vector<Color> &GetUsedColors() const { return usedColors_; }
    void SetUsedColors(const std::vector<Color> &colors) { usedColors_ = colors; }

    // 実行時調整（GameParamHub）へポインタを渡すため非constで返す
    BossShellParams &Shell() { return shell_; }
    const BossShellParams &Shell() const { return shell_; }
    BossMetaBallParams &MetaBall() { return metaBall_; }
    const BossMetaBallParams &MetaBall() const { return metaBall_; }
    BossChainParams &Chain() { return chain_; }
    const BossChainParams &Chain() const { return chain_; }
    BossLockOnParams &LockOn() { return lockOn_; }
    const BossLockOnParams &LockOn() const { return lockOn_; }
    BossBattleParams &Battle() { return battle_; }
    const BossBattleParams &Battle() const { return battle_; }
    BossSpinAttackParams &Spin() { return spin_; }
    const BossSpinAttackParams &Spin() const { return spin_; }
    BossSlamAttackParams &Slam() { return slam_; }
    const BossSlamAttackParams &Slam() const { return slam_; }
    BossExposureParams &Exposure() { return exposure_; }
    const BossExposureParams &Exposure() const { return exposure_; }

private:
    /// ===================================================
    /// private variables
    /// ===================================================

    std::string bossId_ = "Boss01";
    uint32_t colorSeed_ = 20260902; // 0 なら実行ごとにランダム
    std::vector<Color> usedColors_ = {Color::RED, Color::BLUE, Color::GREEN};

    BossShellParams shell_{};
    BossMetaBallParams metaBall_{};
    BossChainParams chain_{};
    BossLockOnParams lockOn_{};
    BossBattleParams battle_{};
    BossSpinAttackParams spin_{};
    BossSlamAttackParams slam_{};
    BossExposureParams exposure_{};
};
