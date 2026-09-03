#include "BossParameters.h"
#include "BossColorPalette.h"
#include "data/DataHandler.h"
#include "debug/log/Logger.h"

using json = nlohmann::json;

namespace {

/// <summary>JSONオブジェクトから型付きで値を取り出す（欠けていれば既定値）</summary>
template <typename T>
T JsonValue(const json &object, const char *key, const T &defaultValue) {
    if (object.is_object() && object.contains(key)) {
        try {
            return object.at(key).get<T>();
        } catch (const json::exception &) {
            // 型が違う場合は既定値で続行する（データ不備でクラッシュさせない）
        }
    }
    return defaultValue;
}

} // namespace

void BossParameters::Load(const std::string &bossId) {
    bossId_ = bossId;

    Hagine::DataHandler data("Boss", bossId_);

    colorSeed_ = data.Load<uint32_t>("colorSeed", colorSeed_);

    // --- 使用色（識別子の配列）---
    std::vector<std::string> colorIds;
    for (Color color : usedColors_) {
        colorIds.emplace_back(BossColorPalette::GetIdText(color));
    }
    colorIds = data.Load<std::vector<std::string>>("colors", colorIds);

    std::vector<Color> parsed;
    for (const std::string &id : colorIds) {
        Color color{};
        if (BossColorPalette::TryParse(id, color)) {
            parsed.push_back(color);
        } else {
            Hagine::Logger::Error("BossParameters: 未知の色ID \"" + id + "\" (" + bossId_ + ")");
        }
    }
    if (!parsed.empty()) {
        usedColors_ = parsed;
    }

    // --- 殻（FCC格子） ---
    const json shell = data.Load<json>("shell", json::object());
    shell_.subdivision = JsonValue(shell, "subdivision", shell_.subdivision);
    shell_.shellRadius = JsonValue(shell, "shellRadius", shell_.shellRadius);
    shell_.sphereRadius = JsonValue(shell, "sphereRadius", shell_.sphereRadius);
    shell_.innerLayers = JsonValue(shell, "innerLayers", shell_.innerLayers);
    shell_.outerLayers = JsonValue(shell, "outerLayers", shell_.outerLayers);
    shell_.coreScale = JsonValue(shell, "coreScale", shell_.coreScale);
    shell_.extraCapacity = JsonValue(shell, "extraCapacity", shell_.extraCapacity);

    // --- 連鎖マッチ ---
    const json chain = data.Load<json>("chain", json::object());
    chain_.minMatch = JsonValue(chain, "minMatch", chain_.minMatch);
    chain_.staggerBase = JsonValue(chain, "staggerBase", chain_.staggerBase);
    chain_.staggerPerPart = JsonValue(chain, "staggerPerPart", chain_.staggerPerPart);
    chain_.maxInitialCluster = JsonValue(chain, "maxInitialCluster", chain_.maxInitialCluster);

    // --- ソフトロックオン ---
    const json lockOn = data.Load<json>("lockOn", json::object());
    lockOn_.maxAngleDegrees = JsonValue(lockOn, "maxAngleDegrees", lockOn_.maxAngleDegrees);
    lockOn_.maxDistance = JsonValue(lockOn, "maxDistance", lockOn_.maxDistance);
    lockOn_.requireFacing = JsonValue(lockOn, "requireFacing", lockOn_.requireFacing);

    // --- 戦闘全体 ---
    const json battle = data.Load<json>("battle", json::object());
    battle_.arenaRadius = JsonValue(battle, "arenaRadius", battle_.arenaRadius);
    battle_.idleSpinSpeed = JsonValue(battle, "idleSpinSpeed", battle_.idleSpinSpeed);

    // --- 登場演出 ---
    const json appear = data.Load<json>("appear", json::object());
    appear_.gatherTime = JsonValue(appear, "gatherTime", appear_.gatherTime);
    appear_.settleTime = JsonValue(appear, "settleTime", appear_.settleTime);
    appear_.expandTime = JsonValue(appear, "expandTime", appear_.expandTime);
    appear_.gatherRadius = JsonValue(appear, "gatherRadius", appear_.gatherRadius);
    appear_.gatherSpinSpeed = JsonValue(appear, "gatherSpinSpeed", appear_.gatherSpinSpeed);
    appear_.startScale = JsonValue(appear, "startScale", appear_.startScale);
    appear_.arriveScale = JsonValue(appear, "arriveScale", appear_.arriveScale);
    appear_.spawnSpread = JsonValue(appear, "spawnSpread", appear_.spawnSpread);

    // --- 露出度スケーリング ---
    const json exposure = data.Load<json>("exposure", json::object());
    exposure_.attackIntervalAtZero = JsonValue(exposure, "attackIntervalAtZero", exposure_.attackIntervalAtZero);
    exposure_.attackIntervalAtFull = JsonValue(exposure, "attackIntervalAtFull", exposure_.attackIntervalAtFull);
    exposure_.spinDashSpeedScaleAtFull = JsonValue(exposure, "spinDashSpeedScaleAtFull", exposure_.spinDashSpeedScaleAtFull);
    exposure_.spinTelegraphScaleAtFull = JsonValue(exposure, "spinTelegraphScaleAtFull", exposure_.spinTelegraphScaleAtFull);
    exposure_.slamCountAtZero = JsonValue(exposure, "slamCountAtZero", exposure_.slamCountAtZero);
    exposure_.slamCountAtFull = JsonValue(exposure, "slamCountAtFull", exposure_.slamCountAtFull);
    exposure_.slamAimScaleAtFull = JsonValue(exposure, "slamAimScaleAtFull", exposure_.slamAimScaleAtFull);

    // --- 攻撃 ---
    const json attacks = data.Load<json>("attacks", json::object());

    const json spin = JsonValue(attacks, "spin", json::object());
    spin_.telegraphTime = JsonValue(spin, "telegraphTime", spin_.telegraphTime);
    spin_.telegraphSpinSpeed = JsonValue(spin, "telegraphSpinSpeed", spin_.telegraphSpinSpeed);
    spin_.dashSpeed = JsonValue(spin, "dashSpeed", spin_.dashSpeed);
    spin_.dashTime = JsonValue(spin, "dashTime", spin_.dashTime);
    spin_.recoverTime = JsonValue(spin, "recoverTime", spin_.recoverTime);
    spin_.damage = JsonValue(spin, "damage", spin_.damage);
    spin_.contactMargin = JsonValue(spin, "contactMargin", spin_.contactMargin);

    const json slam = JsonValue(attacks, "slam", json::object());
    slam_.riseTime = JsonValue(slam, "riseTime", slam_.riseTime);
    slam_.riseHeight = JsonValue(slam, "riseHeight", slam_.riseHeight);
    slam_.aimTime = JsonValue(slam, "aimTime", slam_.aimTime);
    slam_.fallTime = JsonValue(slam, "fallTime", slam_.fallTime);
    slam_.impactTime = JsonValue(slam, "impactTime", slam_.impactTime);
    slam_.impactRadius = JsonValue(slam, "impactRadius", slam_.impactRadius);
    slam_.damage = JsonValue(slam, "damage", slam_.damage);
    slam_.recoverTime = JsonValue(slam, "recoverTime", slam_.recoverTime);
}

void BossParameters::Save() const {
    Hagine::DataHandler data("Boss", bossId_);

    data.Save("colorSeed", colorSeed_);

    std::vector<std::string> colorIds;
    for (Color color : usedColors_) {
        colorIds.emplace_back(BossColorPalette::GetIdText(color));
    }
    data.Save("colors", colorIds);

    json shell = json::object();
    shell["subdivision"] = shell_.subdivision;
    shell["shellRadius"] = shell_.shellRadius;
    shell["sphereRadius"] = shell_.sphereRadius;
    shell["innerLayers"] = shell_.innerLayers;
    shell["outerLayers"] = shell_.outerLayers;
    shell["coreScale"] = shell_.coreScale;
    shell["extraCapacity"] = shell_.extraCapacity;
    data.Save("shell", shell);

    json chain = json::object();
    chain["minMatch"] = chain_.minMatch;
    chain["staggerBase"] = chain_.staggerBase;
    chain["staggerPerPart"] = chain_.staggerPerPart;
    chain["maxInitialCluster"] = chain_.maxInitialCluster;
    data.Save("chain", chain);

    json lockOn = json::object();
    lockOn["maxAngleDegrees"] = lockOn_.maxAngleDegrees;
    lockOn["maxDistance"] = lockOn_.maxDistance;
    lockOn["requireFacing"] = lockOn_.requireFacing;
    data.Save("lockOn", lockOn);

    json battle = json::object();
    battle["arenaRadius"] = battle_.arenaRadius;
    battle["idleSpinSpeed"] = battle_.idleSpinSpeed;
    data.Save("battle", battle);

    json appear = json::object();
    appear["gatherTime"] = appear_.gatherTime;
    appear["settleTime"] = appear_.settleTime;
    appear["expandTime"] = appear_.expandTime;
    appear["gatherRadius"] = appear_.gatherRadius;
    appear["gatherSpinSpeed"] = appear_.gatherSpinSpeed;
    appear["startScale"] = appear_.startScale;
    appear["arriveScale"] = appear_.arriveScale;
    appear["spawnSpread"] = appear_.spawnSpread;
    data.Save("appear", appear);

    json exposure = json::object();
    exposure["attackIntervalAtZero"] = exposure_.attackIntervalAtZero;
    exposure["attackIntervalAtFull"] = exposure_.attackIntervalAtFull;
    exposure["spinDashSpeedScaleAtFull"] = exposure_.spinDashSpeedScaleAtFull;
    exposure["spinTelegraphScaleAtFull"] = exposure_.spinTelegraphScaleAtFull;
    exposure["slamCountAtZero"] = exposure_.slamCountAtZero;
    exposure["slamCountAtFull"] = exposure_.slamCountAtFull;
    exposure["slamAimScaleAtFull"] = exposure_.slamAimScaleAtFull;
    data.Save("exposure", exposure);

    json spin = json::object();
    spin["telegraphTime"] = spin_.telegraphTime;
    spin["telegraphSpinSpeed"] = spin_.telegraphSpinSpeed;
    spin["dashSpeed"] = spin_.dashSpeed;
    spin["dashTime"] = spin_.dashTime;
    spin["recoverTime"] = spin_.recoverTime;
    spin["damage"] = spin_.damage;
    spin["contactMargin"] = spin_.contactMargin;

    json slam = json::object();
    slam["riseTime"] = slam_.riseTime;
    slam["riseHeight"] = slam_.riseHeight;
    slam["aimTime"] = slam_.aimTime;
    slam["fallTime"] = slam_.fallTime;
    slam["impactTime"] = slam_.impactTime;
    slam["impactRadius"] = slam_.impactRadius;
    slam["damage"] = slam_.damage;
    slam["recoverTime"] = slam_.recoverTime;

    json attacks = json::object();
    attacks["spin"] = spin;
    attacks["slam"] = slam;
    data.Save("attacks", attacks);
}
