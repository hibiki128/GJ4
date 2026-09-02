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

    maxHp_ = data.Load<float>("hp", maxHp_);
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

    // --- パーツ配置 ---
    const json layout = data.Load<json>("layout", json::object());
    layout_.subdivision = JsonValue(layout, "subdivision", layout_.subdivision);
    layout_.radius = JsonValue(layout, "radius", layout_.radius);
    layout_.partScale = JsonValue(layout, "partScale", layout_.partScale);
    layout_.partThickness = JsonValue(layout, "partThickness", layout_.partThickness);
    layout_.coreScale = JsonValue(layout, "coreScale", layout_.coreScale);

    // --- 連鎖マッチ ---
    const json chain = data.Load<json>("chain", json::object());
    chain_.minMatch = JsonValue(chain, "minMatch", chain_.minMatch);
    chain_.damagePerPart = JsonValue(chain, "damagePerPart", chain_.damagePerPart);
    chain_.chainBonus = JsonValue(chain, "chainBonus", chain_.chainBonus);
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

    // --- 露出度スケーリング ---
    const json exposure = data.Load<json>("exposure", json::object());
    exposure_.normalizeByDestroyable = JsonValue(exposure, "normalizeByDestroyable", exposure_.normalizeByDestroyable);
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

    data.Save("hp", maxHp_);
    data.Save("colorSeed", colorSeed_);

    std::vector<std::string> colorIds;
    for (Color color : usedColors_) {
        colorIds.emplace_back(BossColorPalette::GetIdText(color));
    }
    data.Save("colors", colorIds);

    json layout = json::object();
    layout["subdivision"] = layout_.subdivision;
    layout["radius"] = layout_.radius;
    layout["partScale"] = layout_.partScale;
    layout["partThickness"] = layout_.partThickness;
    layout["coreScale"] = layout_.coreScale;
    data.Save("layout", layout);

    json chain = json::object();
    chain["minMatch"] = chain_.minMatch;
    chain["damagePerPart"] = chain_.damagePerPart;
    chain["chainBonus"] = chain_.chainBonus;
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

    json exposure = json::object();
    exposure["normalizeByDestroyable"] = exposure_.normalizeByDestroyable;
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
