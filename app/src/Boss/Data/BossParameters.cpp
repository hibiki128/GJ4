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

    // --- 殻の見た目（メタボール） ---
    const json metaBall = data.Load<json>("metaBall", json::object());
    metaBall_.influenceScale = JsonValue(metaBall, "influenceScale", metaBall_.influenceScale);
    metaBall_.voxelRatio = JsonValue(metaBall, "voxelRatio", metaBall_.voxelRatio);
    metaBall_.threshold = JsonValue(metaBall, "threshold", metaBall_.threshold);
    metaBall_.highlightScale = JsonValue(metaBall, "highlightScale", metaBall_.highlightScale);
    metaBall_.useGpu = JsonValue(metaBall, "useGpu", metaBall_.useGpu);
    metaBall_.wobbleAmplitude = JsonValue(metaBall, "wobbleAmplitude", metaBall_.wobbleAmplitude);
    metaBall_.wobbleSpeed = JsonValue(metaBall, "wobbleSpeed", metaBall_.wobbleSpeed);
    metaBall_.wobbleFrequency = JsonValue(metaBall, "wobbleFrequency", metaBall_.wobbleFrequency);
    metaBall_.maxTrianglesPerColor = JsonValue(metaBall, "maxTrianglesPerColor", metaBall_.maxTrianglesPerColor);

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

    // --- 吸着・消滅の演出 ---
    const json effect = data.Load<json>("effect", json::object());
    effect_.attachTime = JsonValue(effect, "attachTime", effect_.attachTime);
    effect_.attachStartScale = JsonValue(effect, "attachStartScale", effect_.attachStartScale);
    effect_.vanishTime = JsonValue(effect, "vanishTime", effect_.vanishTime);
    effect_.vanishDrift = JsonValue(effect, "vanishDrift", effect_.vanishDrift);
    effect_.vanishSpread = JsonValue(effect, "vanishSpread", effect_.vanishSpread);

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

    json metaBall = json::object();
    metaBall["influenceScale"] = metaBall_.influenceScale;
    metaBall["voxelRatio"] = metaBall_.voxelRatio;
    metaBall["threshold"] = metaBall_.threshold;
    metaBall["highlightScale"] = metaBall_.highlightScale;
    metaBall["useGpu"] = metaBall_.useGpu;
    metaBall["wobbleAmplitude"] = metaBall_.wobbleAmplitude;
    metaBall["wobbleSpeed"] = metaBall_.wobbleSpeed;
    metaBall["wobbleFrequency"] = metaBall_.wobbleFrequency;
    metaBall["maxTrianglesPerColor"] = metaBall_.maxTrianglesPerColor;
    data.Save("metaBall", metaBall);

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

    json effect = json::object();
    effect["attachTime"] = effect_.attachTime;
    effect["attachStartScale"] = effect_.attachStartScale;
    effect["vanishTime"] = effect_.vanishTime;
    effect["vanishDrift"] = effect_.vanishDrift;
    effect["vanishSpread"] = effect_.vanishSpread;
    data.Save("effect", effect);

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

void LoadSpiderParams(const std::string &bossId, BossSpiderParams &out) {
    Hagine::DataHandler data("Boss", bossId);
    const json spider = data.Load<json>("spider", json::object());

    out.bodyRadius = JsonValue(spider, "bodyRadius", out.bodyRadius);
    out.legCount = JsonValue(spider, "legCount", out.legCount);
    out.upperSphereCount = JsonValue(spider, "upperSphereCount", out.upperSphereCount);
    out.lowerSphereCount = JsonValue(spider, "lowerSphereCount", out.lowerSphereCount);
    out.legSphereRadius = JsonValue(spider, "legSphereRadius", out.legSphereRadius);
    out.maxAttachPerLeg = JsonValue(spider, "maxAttachPerLeg", out.maxAttachPerLeg);

    out.legSpread = JsonValue(spider, "legSpread", out.legSpread);
    out.legSpreadOffset = JsonValue(spider, "legSpreadOffset", out.legSpreadOffset);
    out.legLength = JsonValue(spider, "legLength", out.legLength);
    out.kneeLift = JsonValue(spider, "kneeLift", out.kneeLift);
    out.footRadius = JsonValue(spider, "footRadius", out.footRadius);
    out.bodyHeight = JsonValue(spider, "bodyHeight", out.bodyHeight);

    out.riseTime = JsonValue(spider, "riseTime", out.riseTime);
    out.growTime = JsonValue(spider, "growTime", out.growTime);
    out.growStagger = JsonValue(spider, "growStagger", out.growStagger);
    out.landTime = JsonValue(spider, "landTime", out.landTime);

    out.moveSpeed = JsonValue(spider, "moveSpeed", out.moveSpeed);
    out.turnSpeed = JsonValue(spider, "turnSpeed", out.turnSpeed);
    out.stepTime = JsonValue(spider, "stepTime", out.stepTime);
    out.stepHeight = JsonValue(spider, "stepHeight", out.stepHeight);
    out.stepTrigger = JsonValue(spider, "stepTrigger", out.stepTrigger);
    out.stepLead = JsonValue(spider, "stepLead", out.stepLead);
    out.bodyBob = JsonValue(spider, "bodyBob", out.bodyBob);
    out.bodySway = JsonValue(spider, "bodySway", out.bodySway);
    out.stopDistance = JsonValue(spider, "stopDistance", out.stopDistance);
    // --- 攻撃 ---
    const json attacks = JsonValue(spider, "attacks", json::object());
    out.attack.interval = JsonValue(attacks, "interval", out.attack.interval);
    out.attack.shootRange = JsonValue(attacks, "shootRange", out.attack.shootRange);
    out.attack.whirlChance = JsonValue(attacks, "whirlChance", out.attack.whirlChance);

    const json leap = JsonValue(attacks, "leap", json::object());
    out.attack.leap.hopCount = JsonValue(leap, "hopCount", out.attack.leap.hopCount);
    out.attack.leap.crouchTime = JsonValue(leap, "crouchTime", out.attack.leap.crouchTime);
    out.attack.leap.crouchDepth = JsonValue(leap, "crouchDepth", out.attack.leap.crouchDepth);
    out.attack.leap.riseTime = JsonValue(leap, "riseTime", out.attack.leap.riseTime);
    out.attack.leap.apexHeight = JsonValue(leap, "apexHeight", out.attack.leap.apexHeight);
    out.attack.leap.fallTime = JsonValue(leap, "fallTime", out.attack.leap.fallTime);
    out.attack.leap.impactTime = JsonValue(leap, "impactTime", out.attack.leap.impactTime);
    out.attack.leap.impactRadius = JsonValue(leap, "impactRadius", out.attack.leap.impactRadius);
    out.attack.leap.damage = JsonValue(leap, "damage", out.attack.leap.damage);
    out.attack.leap.landSpread = JsonValue(leap, "landSpread", out.attack.leap.landSpread);
    out.attack.leap.maxLeapRange = JsonValue(leap, "maxLeapRange", out.attack.leap.maxLeapRange);
    out.attack.leap.recoverTime = JsonValue(leap, "recoverTime", out.attack.leap.recoverTime);
    out.attack.leap.legTuck = JsonValue(leap, "legTuck", out.attack.leap.legTuck);
    out.attack.leap.legFoldTime = JsonValue(leap, "legFoldTime", out.attack.leap.legFoldTime);

    const json shoot = JsonValue(attacks, "shoot", json::object());
    out.attack.shoot.telegraphTime = JsonValue(shoot, "telegraphTime", out.attack.shoot.telegraphTime);
    out.attack.shoot.shotCount = JsonValue(shoot, "shotCount", out.attack.shoot.shotCount);
    out.attack.shoot.shotInterval = JsonValue(shoot, "shotInterval", out.attack.shoot.shotInterval);
    out.attack.shoot.speed = JsonValue(shoot, "speed", out.attack.shoot.speed);
    out.attack.shoot.radius = JsonValue(shoot, "radius", out.attack.shoot.radius);
    out.attack.shoot.life = JsonValue(shoot, "life", out.attack.shoot.life);
    out.attack.shoot.spreadDegrees = JsonValue(shoot, "spreadDegrees", out.attack.shoot.spreadDegrees);
    out.attack.shoot.homingRate = JsonValue(shoot, "homingRate", out.attack.shoot.homingRate);
    out.attack.shoot.homingTime = JsonValue(shoot, "homingTime", out.attack.shoot.homingTime);
    out.attack.shoot.damage = JsonValue(shoot, "damage", out.attack.shoot.damage);
    out.attack.shoot.recoverTime = JsonValue(shoot, "recoverTime", out.attack.shoot.recoverTime);

    const json whirl = JsonValue(attacks, "whirl", json::object());
    out.attack.whirl.telegraphTime = JsonValue(whirl, "telegraphTime", out.attack.whirl.telegraphTime);
    out.attack.whirl.spinTime = JsonValue(whirl, "spinTime", out.attack.whirl.spinTime);
    out.attack.whirl.spinSpeed = JsonValue(whirl, "spinSpeed", out.attack.whirl.spinSpeed);
    out.attack.whirl.spinHeight = JsonValue(whirl, "spinHeight", out.attack.whirl.spinHeight);
    out.attack.whirl.damage = JsonValue(whirl, "damage", out.attack.whirl.damage);
    out.attack.whirl.recoverTime = JsonValue(whirl, "recoverTime", out.attack.whirl.recoverTime);

}

void SaveSpiderParams(const std::string &bossId, const BossSpiderParams &params) {
    // DataHandler は生成時にファイルを読み込んでから該当キーだけを差し替えるので、
    // ここで "spider" 以外の項目（殻・連鎖・攻撃など）が消えることはない
    Hagine::DataHandler data("Boss", bossId);

    json spider = json::object();
    spider["bodyRadius"] = params.bodyRadius;
    spider["legCount"] = params.legCount;
    spider["upperSphereCount"] = params.upperSphereCount;
    spider["lowerSphereCount"] = params.lowerSphereCount;
    spider["legSphereRadius"] = params.legSphereRadius;
    spider["maxAttachPerLeg"] = params.maxAttachPerLeg;

    spider["legSpread"] = params.legSpread;
    spider["legSpreadOffset"] = params.legSpreadOffset;
    spider["legLength"] = params.legLength;
    spider["kneeLift"] = params.kneeLift;
    spider["footRadius"] = params.footRadius;
    spider["bodyHeight"] = params.bodyHeight;

    spider["riseTime"] = params.riseTime;
    spider["growTime"] = params.growTime;
    spider["growStagger"] = params.growStagger;
    spider["landTime"] = params.landTime;

    spider["moveSpeed"] = params.moveSpeed;
    spider["turnSpeed"] = params.turnSpeed;
    spider["stepTime"] = params.stepTime;
    spider["stepHeight"] = params.stepHeight;
    spider["stepTrigger"] = params.stepTrigger;
    spider["stepLead"] = params.stepLead;
    spider["bodyBob"] = params.bodyBob;
    spider["bodySway"] = params.bodySway;
    spider["stopDistance"] = params.stopDistance;
    json leap = json::object();
    leap["hopCount"] = params.attack.leap.hopCount;
    leap["crouchTime"] = params.attack.leap.crouchTime;
    leap["crouchDepth"] = params.attack.leap.crouchDepth;
    leap["riseTime"] = params.attack.leap.riseTime;
    leap["apexHeight"] = params.attack.leap.apexHeight;
    leap["fallTime"] = params.attack.leap.fallTime;
    leap["impactTime"] = params.attack.leap.impactTime;
    leap["impactRadius"] = params.attack.leap.impactRadius;
    leap["damage"] = params.attack.leap.damage;
    leap["landSpread"] = params.attack.leap.landSpread;
    leap["maxLeapRange"] = params.attack.leap.maxLeapRange;
    leap["recoverTime"] = params.attack.leap.recoverTime;
    leap["legTuck"] = params.attack.leap.legTuck;
    leap["legFoldTime"] = params.attack.leap.legFoldTime;

    json shoot = json::object();
    shoot["telegraphTime"] = params.attack.shoot.telegraphTime;
    shoot["shotCount"] = params.attack.shoot.shotCount;
    shoot["shotInterval"] = params.attack.shoot.shotInterval;
    shoot["speed"] = params.attack.shoot.speed;
    shoot["radius"] = params.attack.shoot.radius;
    shoot["life"] = params.attack.shoot.life;
    shoot["spreadDegrees"] = params.attack.shoot.spreadDegrees;
    shoot["homingRate"] = params.attack.shoot.homingRate;
    shoot["homingTime"] = params.attack.shoot.homingTime;
    shoot["damage"] = params.attack.shoot.damage;
    shoot["recoverTime"] = params.attack.shoot.recoverTime;

    json whirl = json::object();
    whirl["telegraphTime"] = params.attack.whirl.telegraphTime;
    whirl["spinTime"] = params.attack.whirl.spinTime;
    whirl["spinSpeed"] = params.attack.whirl.spinSpeed;
    whirl["spinHeight"] = params.attack.whirl.spinHeight;
    whirl["damage"] = params.attack.whirl.damage;
    whirl["recoverTime"] = params.attack.whirl.recoverTime;

    json attacks = json::object();
    attacks["interval"] = params.attack.interval;
    attacks["shootRange"] = params.attack.shootRange;
    attacks["whirlChance"] = params.attack.whirlChance;
    attacks["leap"] = leap;
    attacks["shoot"] = shoot;
    attacks["whirl"] = whirl;
    spider["attacks"] = attacks;


    data.Save("spider", spider);
}
