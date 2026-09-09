#include "FieldSurround.h"
#include "MyMath.h"
#include "frame/Frame.h"
#include "model/material/Material.h"
#include "object/base/BaseObject.h"
#include "object/base/BaseObjectManager.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>
#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

using namespace Hagine;

namespace {

/// <summary>柱の名前の接頭辞。JSONのファイル名にもなる</summary>
constexpr const char *kNamePrefix = "FieldBlock_";

/// <summary>柱の本数（保存したJSONと数が合わなくなるので、増やすと前の柱が余る）</summary>
constexpr int kBlockCount = 96;

/// <summary>色をそのまま出すための白テクスチャ（床やボスと同じもの）</summary>
constexpr const char *kWhiteTexturePath = "debug/white1x1.png";

/// <summary>床と同じ法線マップ。要望どおり全部の柱に掛ける</summary>
constexpr const char *kNormalMapPath = "NormalMap/hex_normal_2048.png";

/// <summary>背景を塞ぐ球の名前</summary>
constexpr const char *kSkyDomeName = "FieldSkyDome";

constexpr float kToRadian = std::numbers::pi_v<float> / 180.0f;
constexpr float kTwoPi = std::numbers::pi_v<float> * 2.0f;

} // namespace

std::string FieldSurround::MakeName(int index) {
    // 3桁の0埋め。ファイル名で並べたときに順番が崩れないようにする
    std::string number = std::to_string(index);
    while (number.size() < 3) {
        number.insert(number.begin(), '0');
    }
    return std::string(kNamePrefix) + number;
}

void FieldSurround::Init(float fieldRadius, BaseObjectManager *objectManager,
                         const std::string &sceneName) {
    fieldRadius_ = fieldRadius;
    pObjectManager_ = objectManager;
    // 柱はそのシーンのオブジェクトとして保存する。シーンの読み込みが拾ってくれる場所に置くこと
    saveFolder_ = "SceneData/" + sceneName + "/ObjectDatas";
    palette_.LoadMaster();

    // --- 調整パラメータ ---
    // 並べ方は「作り直す」を押すまで効かない。柱はシーンのオブジェクトなので、
    // 触るたびに並べ直すとエディタで直した位置まで毎回消えてしまう
    params_.Register("RingCount", &ringCount_, {1.0f, 1.0f, 6.0f});
    params_.Register("InnerMargin", &innerMargin_, {0.5f, 0.0f, 60.0f});
    params_.Register("RingSpacing", &ringSpacing_, {0.5f, 2.0f, 80.0f});
    params_.Register("FillInner", &fillInner_, {0.01f, 0.2f, 2.0f});
    params_.Register("FillOuter", &fillOuter_, {0.01f, 0.2f, 2.0f});
    params_.Register("HeightMin", &heightMin_, {0.5f, 1.0f, 200.0f});
    params_.Register("HeightMax", &heightMax_, {0.5f, 1.0f, 200.0f});
    params_.Register("HeightGrowth", &heightGrowth_, {0.05f, 1.0f, 5.0f});
    params_.Register("BlockDepth", &blockDepth_, {0.5f, 1.0f, 40.0f});
    params_.Register("SinkDepth", &sinkDepth_, {0.5f, 0.0f, 40.0f});
    params_.Register("RadiusJitter", &radiusJitter_, {0.1f, 0.0f, 20.0f});
    params_.Register("YawJitter", &yawJitter_, {0.5f, 0.0f, 90.0f});
    params_.Register("ColorScale", &colorScale_, {0.01f, 0.0f, 2.0f});
    params_.Register("Seed", &seed_, {1.0f, 0.0f, 100000.0f});

    // 揺れと見た目はその場で効く（毎フレーム読み直しているだけなので作り直しが要らない）
    params_.Register("BobAmount", &bobAmount_, {0.05f, 0.0f, 10.0f});
    params_.Register("BobPeriodMin", &bobPeriodMin_, {0.1f, 0.5f, 30.0f});
    params_.Register("BobPeriodMax", &bobPeriodMax_, {0.1f, 0.5f, 30.0f});

    GameParamHub::Options materialOptions{};
    materialOptions.speed = 0.1f;
    materialOptions.min = 0.0f;
    materialOptions.max = 32.0f;
    materialOptions.onChange = [this] {
        for (const Block &block : blocks_) {
            ApplyMaterial(block.object);
        }
    };
    params_.Register("NormalStrength", &normalStrength_, materialOptions);
    params_.Register("UvScale", &uvScale_, materialOptions);

    GameParamHub::Options domeOptions{};
    domeOptions.speed = 5.0f;
    domeOptions.min = 50.0f;
    domeOptions.max = 900.0f;
    domeOptions.onChange = [this] { ApplySkyDome(); };
    params_.Register("DomeRadius", &domeRadius_, domeOptions);

    GameParamHub::Options domeColorOptions{};
    domeColorOptions.speed = 0.01f;
    domeColorOptions.isColor = true;
    domeColorOptions.onChange = [this] { ApplySkyDome(); };
    params_.Register("DomeColor", &domeColor_, domeColorOptions);

    // 背景を塞ぐ球は柱と関係なく、必ず1枚用意する
    SetupSkyDome();

    // 保存済みの柱があればそれを使う。ここで作り直すと、エディタで詰めた配置が消える
    if (AdoptSavedBlocks()) {
        return;
    }

    // 初回だけ。作ってそのままJSONへ落とすので、次の起動からは読み込みの側で並ぶ
    Generate();
    SaveBlocks();
}

void FieldSurround::SetupSkyDome() {
    if (!pObjectManager_) {
        return;
    }

    // シーンに保存済みならそれを使う（エディタで色や大きさを直した結果が残る）
    pSkyDome_ = pObjectManager_->GetObjectByName(kSkyDomeName);
    const bool isNew = (pSkyDome_ == nullptr);
    if (isNew) {
        pSkyDome_ = pObjectManager_->CreatePrimitiveObject(PrimitiveType::Sphere, kSkyDomeName);
    }
    if (!pSkyDome_) {
        return;
    }

    pSkyDome_->SetFolderPath(saveFolder_);
    pSkyDome_->SetIsScene(true);
    pSkyDome_->SetShouldSave(true);
    // ギズモで掴めると、画面いっぱいの球なので他のオブジェクトが選べなくなる
    pSkyDome_->SetGizmoSelectable(false);

    if (isNew) {
        ApplySkyDome();
        pSkyDome_->SceneSaveToJson();
    }
}

void FieldSurround::ApplySkyDome() {
    if (!pSkyDome_) {
        return;
    }

    WorldTransform *transform = pSkyDome_->GetWorldTransform();

    // X だけ符号を反転させて球を裏返す。こうすると表と裏が入れ替わり、
    // 内側（＝カメラのいる側）の面が描かれるようになる
    transform->translation_ = Vector3{0.0f, 0.0f, 0.0f};
    transform->scale_ = Vector3{-domeRadius_, domeRadius_, domeRadius_};
    transform->quaternionRotation_ = Quaternion::IdentityQuaternion();
    transform->UpdateMatrix();

    // 光は当てない。当てると裏返したぶん陰影が反転して、まだら模様になる
    pSkyDome_->GetLighting() = false;
    pSkyDome_->SetTexture(kWhiteTexturePath);
    pSkyDome_->SetColor(domeColor_, 0);

    if (Material *material = pSkyDome_->GetMaterial(0)) {
        // 輪郭線は裏返した面に出ると画面を覆ってしまうので切る
        material->SetEnableToon(false);
    }
}

bool FieldSurround::AdoptSavedBlocks() {
    if (!pObjectManager_) {
        return false;
    }

    blocks_.clear();

    // 名前で引く。シーンに何本あるか分からないので、途切れるまで順に探す
    for (int index = 0; index < kBlockCount; ++index) {
        BaseObject *object = pObjectManager_->GetObjectByName(MakeName(index));
        if (!object) {
            continue;
        }

        Block block{};
        block.object = object;
        AssignBobbing(block, index);
        blocks_.push_back(block);
    }

    return !blocks_.empty();
}

void FieldSurround::Generate() {
    if (!pObjectManager_) {
        return;
    }

    // 実行中にオブジェクトを捨てると、前フレームのGPUコマンドが参照している
    // リソースを解放してしまう。本数は固定にして、作るのは足りないぶんだけにする
    blocks_.clear();
    blocks_.reserve(kBlockCount);
    for (int index = 0; index < kBlockCount; ++index) {
        const std::string name = MakeName(index);
        BaseObject *object = pObjectManager_->GetObjectByName(name);
        if (!object) {
            object = pObjectManager_->CreatePrimitiveObject(PrimitiveType::Cube, name);
        }
        if (!object) {
            continue;
        }

        // シーンのオブジェクトとして保存させる。ここを設定しないと
        // 既定の SceneData/Title/ObjectData へ書きにいってしまう
        object->SetFolderPath(saveFolder_);
        object->SetIsScene(true);
        object->SetShouldSave(true);

        Block block{};
        block.object = object;
        AssignBobbing(block, index);
        blocks_.push_back(block);
    }

    if (blocks_.empty()) {
        return;
    }

    std::mt19937 random(static_cast<uint32_t>(seed_));
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::uniform_real_distribution<float> signed01(-1.0f, 1.0f);

    const int ringCount = (std::max)(1, ringCount_);
    const int perRing = (std::max)(1, static_cast<int>(blocks_.size()) / ringCount);
    const float lowHeight = (std::min)(heightMin_, heightMax_);
    const float highHeight = (std::max)(heightMin_, heightMax_);

    for (int index = 0; index < static_cast<int>(blocks_.size()); ++index) {
        BaseObject *object = blocks_[static_cast<size_t>(index)].object;

        // 余ったぶんはいちばん外の輪へ足す（本数が輪で割り切れないとき）
        const int ring = (std::min)(ringCount - 1, index / perRing);
        const int slot = index - ring * perRing;
        const int slotCount = (ring == ringCount - 1)
                                  ? (static_cast<int>(blocks_.size()) - ring * perRing)
                                  : perRing;

        const float radius = fieldRadius_ + innerMargin_ + ringSpacing_ * static_cast<float>(ring);

        // 外の輪ほど詰まらせる。いちばん外は 1.0 を超えて重ねて、隙間を無くす
        const float ringRatio =
            (ringCount <= 1) ? 1.0f : static_cast<float>(ring) / static_cast<float>(ringCount - 1);
        const float fill = fillInner_ + (fillOuter_ - fillInner_) * ringRatio;

        // 1本ぶんの幅は「その輪の円周 ÷ 本数」を基準にする。
        // こうすると外の輪でも内の輪でも、同じ詰まり具合で並ぶ
        const float arcStep = kTwoPi * radius / static_cast<float>((std::max)(1, slotCount));
        const float width = arcStep * fill;

        // 輪ごとに半区画ずらす。内側の隙間の真後ろに、必ず外側の柱が来るようにする
        const float angle = (static_cast<float>(slot) + 0.5f * static_cast<float>(ring)) *
                            (kTwoPi / static_cast<float>((std::max)(1, slotCount)));

        const float placedRadius = radius + signed01(random) * radiusJitter_;
        const float height = (lowHeight + (highHeight - lowHeight) * unit(random)) *
                             std::pow((std::max)(1.0f, heightGrowth_), static_cast<float>(ring));

        WorldTransform *transform = object->GetWorldTransform();

        // Cube プリミティブは ±1 の大きさなので、スケールは求める寸法の半分にする
        transform->scale_ = Vector3{width * 0.5f, height * 0.5f, blockDepth_ * 0.5f};

        // 足元を地面より下へ沈める。揺れても柱の下に隙間が空かない
        transform->translation_ = Vector3{std::cos(angle) * placedRadius,
                                          height * 0.5f - sinkDepth_,
                                          std::sin(angle) * placedRadius};

        // 面をフィールドの中心へ向ける。少しばらつかせて、並びの機械っぽさを消す
        const float yaw = -angle + signed01(random) * yawJitter_ * kToRadian;
        transform->quaternionRotation_ =
            Quaternion::FromAxisAngle(Vector3{0.0f, 1.0f, 0.0f}, yaw);
        transform->UpdateMatrix();

        // 色はゲームの4色から順ぐりに。輪ごとに1つずらしてから配るので、
        // 内と外で同じ色が重ならない
        const Color color = BossColorPalette::FromIndex((index + ring) % kGameColorCount);
        Vector4 rgba = palette_.GetRgba(color);
        rgba.x *= colorScale_;
        rgba.y *= colorScale_;
        rgba.z *= colorScale_;
        object->SetColor(rgba, 0);

        ApplyMaterial(object);
    }
}

void FieldSurround::ApplyMaterial(BaseObject *object) const {
    if (!object) {
        return;
    }

    // 色をそのまま出したいので、既定の uvChecker から白テクスチャへ差し替える
    object->SetTexture(kWhiteTexturePath);

    Material *material = object->GetMaterial(0);
    if (!material) {
        return;
    }

    // 床（plane）と同じ設定にそろえる。同じ世界の一部に見せたいので、
    // 法線マップもトゥーンも床の値に合わせてある
    material->SetNormalMap(kNormalMapPath);
    material->SetNormalStrength(normalStrength_);
    material->SetEnableToon(true);
    material->SetUVSize(Vector2{uvScale_, uvScale_});
}

void FieldSurround::AssignBobbing(Block &block, int index) const {
    // 乱数を持ち回さず、通し番号から決める。何度作り直しても同じ揺れ方になり、
    // 隣どうしが揃わない程度にはばらける
    const float ratio = static_cast<float>(index % 17) / 17.0f;
    const float minPeriod = (std::max)(0.5f, (std::min)(bobPeriodMin_, bobPeriodMax_));
    const float maxPeriod = (std::max)(minPeriod, (std::max)(bobPeriodMin_, bobPeriodMax_));

    block.bobPhase = static_cast<float>(index) * 0.7f;
    block.bobSpeed = kTwoPi / (minPeriod + (maxPeriod - minPeriod) * ratio);
    block.bobScale = 0.45f + 0.55f * static_cast<float>(index % 7) / 7.0f;
}

void FieldSurround::SaveBlocks() {
    for (const Block &block : blocks_) {
        if (!block.object) {
            continue;
        }
        block.object->SetFolderPath(saveFolder_);
        block.object->SceneSaveToJson();
    }
}

void FieldSurround::AppendObstacles(std::vector<BaseObject *> &obstacles) const {
    obstacles.reserve(obstacles.size() + blocks_.size());
    for (const Block &block : blocks_) {
        if (block.object) {
            obstacles.push_back(block.object);
        }
    }
}

void FieldSurround::Update() {
    motionTime_ += Frame::DeltaTime();

    // 揺れは描画オフセットで出す。transform を動かすと、保存したときに
    // 揺れたぶんだけ座標がずれた状態で書き込まれてしまう
    for (const Block &block : blocks_) {
        if (!block.object) {
            continue;
        }
        const float bob =
            std::sin(motionTime_ * block.bobSpeed + block.bobPhase) * bobAmount_ * block.bobScale;
        block.object->SetOffset(Vector3{0.0f, bob, 0.0f});
    }
}

void FieldSurround::DrawImGui() {
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader("フィールドの外周（飾りの柱）")) {
        return;
    }

    ImGui::TextDisabled("柱 %d 本 ／ 半径 %.1f の外側を %d 重で囲んでいる", GetBlockCount(),
                        fieldRadius_, (std::max)(1, ringCount_));
    ImGui::TextDisabled("並べ方の調整は ゲームパラメータ の FieldSurround から行う");

    if (ImGui::Button("並べ直す")) {
        Generate();
    }
    ImGui::SameLine();
    if (ImGui::Button("JSONへ保存")) {
        SaveBlocks();
    }
    ImGui::TextDisabled("並べ方を変えたら「並べ直す」→「JSONへ保存」の順に押す");
    ImGui::TextDisabled("（柱は SceneData/GameScene/ObjectDatas に1本ずつ保存されている）");

    ImGui::Separator();
    ImGui::TextDisabled("背景を塞ぐ球: %s（半径 %.0f）", pSkyDome_ ? "あり" : "なし", domeRadius_);
    ImGui::TextDisabled("見上げたときに柱の頭より上へ抜ける空は、この球が受け持っている");
#endif // USE_IMGUI
}
