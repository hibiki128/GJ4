#pragma once
#include "Application/Character/ColorStruct.h"
#include <cstdint>
#include <string>
#include <vector>

/// <summary>
/// パーツ配置に関するパラメータ
/// </summary>
struct BossLayoutParams {
    int subdivision = 1;        // icosphere の分割回数（0→12個 / 1→42個 / 2→162個）
    float radius = 3.0f;        // パーツを並べる球の半径（これを変えると全体が比例して拡縮する）
    float partScale = 0.0f;     // パーツ1枚の大きさ（0以下なら平均辺長から自動算出）
    float partThickness = 0.7f; // パーツの厚み（法線方向のスケール倍率。1.0で真球、小さいほど平たい板）
    float coreScale = 0.88f;    // 内側のコア球の大きさ（半径に対する倍率）
};

/// <summary>
/// 連鎖マッチに関するパラメータ
/// </summary>
struct BossChainParams {
    int minMatch = 3;             // 破壊に必要な同色連結数
    float damagePerPart = 20.0f;  // パーツ1つあたりの基礎ダメージ
    float chainBonus = 0.25f;     // 最低数を超えた1つごとのダメージ倍率加算
    float staggerBase = 0.6f;     // 連鎖成立時の基礎怯み時間（秒）
    float staggerPerPart = 0.15f; // 最低数を超えた1つごとの怯み加算（秒）
    int maxInitialCluster = 8;    // 初期配色で許す同色の塊の最大数（0以下で無制限）
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
    // 壊せない孤立パーツを除いて正規化する。
    // 色は変化しないため一部のパーツは最後まで壊せず、素の割合では 1.0 に到達しない。
    // true にすると「壊せるパーツをすべて壊した状態」を 1.0 として扱う
    bool normalizeByDestroyable = true;

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

    float GetMaxHp() const { return maxHp_; }
    void SetMaxHp(float hp) { maxHp_ = hp; }

    uint32_t GetColorSeed() const { return colorSeed_; }
    void SetColorSeed(uint32_t seed) { colorSeed_ = seed; }

    const std::vector<Color> &GetUsedColors() const { return usedColors_; }
    void SetUsedColors(const std::vector<Color> &colors) { usedColors_ = colors; }

    // 実行時調整（GameParamHub）へポインタを渡すため非constで返す
    BossLayoutParams &Layout() { return layout_; }
    const BossLayoutParams &Layout() const { return layout_; }
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
    float maxHp_ = 1000.0f;
    uint32_t colorSeed_ = 20260902; // 0 なら実行ごとにランダム
    std::vector<Color> usedColors_ = {Color::RED, Color::BLUE, Color::GREEN};

    BossLayoutParams layout_{};
    BossChainParams chain_{};
    BossLockOnParams lockOn_{};
    BossBattleParams battle_{};
    BossSpinAttackParams spin_{};
    BossSlamAttackParams slam_{};
    BossExposureParams exposure_{};
};
