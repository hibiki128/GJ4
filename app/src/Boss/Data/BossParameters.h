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
/// 登場演出のパラメータ。
/// 「高速回転しながら周囲から球を集める」→「回転が収まる」→「球が膨らんで定位置の大きさになる」
/// の3段階で、各段階をイージングで繋いでカクつかないようにする
/// </summary>
struct BossAppearParams {
    float gatherTime = 1.8f;  // 球が集まってくるまでの時間（秒）
    float settleTime = 0.45f; // 回転が通常速度まで落ちるまでの時間（秒）
    float expandTime = 0.65f; // 球が膨らみ切るまでの時間（秒）

    float gatherRadius = 22.0f;     // どれだけ遠くから集まってくるか
    float gatherSpinSpeed = 900.0f; // 集束中の自転速度（度/秒）

    float startScale = 0.10f;  // 飛んでくる間の球の大きさ（最終サイズに対する倍率）
    float arriveScale = 0.35f; // 到着した瞬間の大きさ（ここから膨らむ）
    float spawnSpread = 0.6f;  // 球ごとの到着タイミングのばらつき（秒。0で一斉に到着）
};

/// <summary>
/// 弾が吸着するとき・球が消えるときの演出パラメータ
/// </summary>
struct BossEffectParams {
    // --- 吸着（弾が殻へ張り付く）---
    float attachTime = 0.18f;       // 着弾点から定位置へ吸い寄せられるまでの時間（秒）
    float attachStartScale = 0.5f;  // 吸着し始めの大きさ（最終サイズに対する倍率）

    // --- 消滅（同色がそろって消える）---
    float vanishTime = 0.3f;    // 消え切るまでの時間（秒）
    float vanishDrift = 0.7f;   // 消えながら外へ押し出される距離
    float vanishSpread = 0.04f; // 塊の中で消える順番の時間差（秒。0で一斉に消える）
};

/// <summary>
/// 第2形態（蜘蛛）の見た目と歩行のパラメータ。
/// 胴は球体形態の中心と同じ黒い球で、そこから色付きの球が連なった脚が生える
/// </summary>
struct BossSpiderParams {
    // --- 見た目 ---
    float bodyRadius = 2.1f;       // 胴（黒い球）の半径
    int legCount = 8;              // 脚の本数
    // 付け根→膝（上腿）に並べる球の数。0以下なら「球が接して連なる数」を上腿の長さから自動算出する。
    // 膝の球は上腿と下腿で共有するので、脚1本の総数は upperSphereCount + lowerSphereCount - 1 になる
    int upperSphereCount = 0;
    // 膝→足先（下腿）に並べる球の数。0以下なら自動算出
    int lowerSphereCount = 0;
    float legSphereRadius = 0.42f; // 脚の球の半径
    // 脚を胴のまわりへ配置するときの、片側（右半分・左半分）の広がり角（度）。
    // 180 なら円周に等間隔。小さくするほど真横へ寄って密集し、蜘蛛らしい並びになる
    float legSpread = 180.0f;
    // 脚の扇全体を前後へずらす角度（度）。正で前寄り、負で後ろ寄り
    float legSpreadOffset = 0.0f;
    // 脚の全長（付け根→膝→足先の折れ線の長さ。膝で二等辺三角形に折るので常にこの長さになる）。
    // 足を最も伸ばした姿勢でも届くよう、footRadius + stepTrigger + stepLead より長く取ること
    float legLength = 8.5f;
    float kneeLift = 0.8f;         // 膝の追加の持ち上げ量（大きいほど「への字」が立つ）
    float footRadius = 5.0f;       // 足を置く円の半径（胴の中心から）
    // 足の高さから胴までの高さ。球体形態のコアが座っていた高さより高くしておくと、
    // 変形のときにコアが「上がりきったまま」立てる（下がり直さないので巻き戻って見えない）
    float bodyHeight = 5.6f;

    // --- 変形（球体形態のコアから生えてくる演出）---
    // 3つの動きは少しずつ重なって進む（浮き上がりきる前に脚が生え始め、
    // 生えきる前に関節が折れ始める）ので、合計はこれらの単純な和より短い
    float riseTime = 1.4f;    // コアが立つ高さまで浮き上がる時間（秒）
    float growTime = 1.6f;    // 脚が真横へ生えきるまでの時間（秒）
    float growStagger = 0.3f; // 隣り合う脚の生え始めのずれ（0で一斉に生える）
    float landTime = 1.1f;    // 関節が折れて足が地面に着くまでの時間（秒）

    // --- 歩行 ---
    float moveSpeed = 3.5f;     // 歩く速さ
    float turnSpeed = 110.0f;   // 向き直りの速さ（度/秒）
    float stepTime = 0.26f;     // 1歩にかける時間（短いほど素早く不気味）
    float stepHeight = 1.5f;    // 足を持ち上げる高さ
    float stepTrigger = 1.5f;   // 足が定位置からこれだけ離れたら踏み替える
    float stepLead = 1.2f;      // 進行方向へ踏み越す量
    float bodyBob = 0.16f;      // 歩調に合わせた胴の上下
    float bodySway = 0.12f;     // 歩調に合わせた胴の左右
    float stopDistance = 6.0f;  // 相手にこれだけ近づいたら止まる
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
/// 蜘蛛形態のパラメータを jsons/Boss/&lt;bossId&gt;.json の "spider" から読み込む。
/// ファイルや項目が無ければ既定値のまま
/// </summary>
/// <param name="bossId">ボス識別子（例: "Boss01"）</param>
/// <param name="out">読み込み先</param>
void LoadSpiderParams(const std::string &bossId, BossSpiderParams &out);

/// <summary>
/// 蜘蛛形態のパラメータを書き戻す。
/// 同じファイルの他の項目（殻・連鎖・攻撃など）はそのまま残る
/// </summary>
/// <param name="bossId">ボス識別子</param>
/// <param name="params">保存する値</param>
void SaveSpiderParams(const std::string &bossId, const BossSpiderParams &params);

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
    BossEffectParams &Effect() { return effect_; }
    const BossEffectParams &Effect() const { return effect_; }
    BossAppearParams &Appear() { return appear_; }
    const BossAppearParams &Appear() const { return appear_; }
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
    BossAppearParams appear_{};
    BossEffectParams effect_{};
};
