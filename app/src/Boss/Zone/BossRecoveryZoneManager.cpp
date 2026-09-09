#include "BossRecoveryZoneManager.h"
#include "src/Interface/IAmmoRecoverySink.h"
#include "src/Interface/IFieldBounds.h"
#include "src/Interface/ITargetLocator.h"
#include "Random.h"
#include "camera/projection/ViewProjection.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

using namespace Hagine;

void BossRecoveryZoneManager::Init(const std::string &namePrefix, BossRecoveryZoneParams *params) {
    namePrefix_ = namePrefix;
    pParams_ = params;
}

void BossRecoveryZoneManager::NotifyAttackFinished(const Vector3 &bossPosition,
                                                   const BossColorPalette &palette) {
    if (!pParams_ || !pParams_->enabled) {
        return;
    }

    const std::vector<Color> &usedColors = palette.GetUsedColors();
    if (usedColors.empty()) {
        return;
    }

    // 色は毎回ランダム。ただし直前と同じ色が続くと「待てば同じ色が来る」だけになるので、
    // 2色以上あるときは引き直して必ず変える
    int colorIndex = Random::Range(0, static_cast<int>(usedColors.size()) - 1);
    if (usedColors.size() > 1 && colorIndex == lastColorIndex_) {
        colorIndex = (colorIndex + 1 + Random::Range(0, static_cast<int>(usedColors.size()) - 2)) %
                     static_cast<int>(usedColors.size());
    }
    lastColorIndex_ = colorIndex;

    BossRecoveryZone *zone = Acquire();
    if (!zone) {
        return;
    }
    zone->Spawn(bossPosition, PickLanding(bossPosition), usedColors[static_cast<size_t>(colorIndex)],
                palette, *pParams_);
    ++spawnCount_;

    // 増やしたぶん、あふれた古いものを畳む
    TrimOldest();
}

void BossRecoveryZoneManager::Update(float deltaTime) {
    if (!pParams_) {
        return;
    }
    for (const std::unique_ptr<BossRecoveryZone> &zone : zones_) {
        zone->Update(deltaTime, *pParams_, pTargetLocator_, pAmmoSink_);
    }
}

void BossRecoveryZoneManager::Draw(const ViewProjection &viewProjection) {
    // 円盤はオブジェクトマネージャーに載せていないので、ここから明示的に描く
    for (const std::unique_ptr<BossRecoveryZone> &zone : zones_) {
        zone->Draw(viewProjection);
    }
}

void BossRecoveryZoneManager::ClearAll() {
    for (const std::unique_ptr<BossRecoveryZone> &zone : zones_) {
        zone->Clear();
    }
    lastColorIndex_ = -1;
}

BossRecoveryZone *BossRecoveryZoneManager::Acquire() {
    for (const std::unique_ptr<BossRecoveryZone> &zone : zones_) {
        if (!zone->IsAlive()) {
            return zone.get();
        }
    }

    // 空きが無ければ増やすだけ。使い終わっても捨てない
    auto created = std::make_unique<BossRecoveryZone>();
    created->Init(namePrefix_ + std::to_string(zones_.size()));
    BossRecoveryZone *zone = created.get();
    zones_.push_back(std::move(created));
    return zone;
}

Vector3 BossRecoveryZoneManager::PickLanding(const Vector3 &bossPosition) const {
    const float minDistance = (std::max)(0.0f, pParams_->spawnDistanceMin);
    const float maxDistance = (std::max)(minDistance, pParams_->spawnDistanceMax);

    // ボスから見て適当な向き・距離へ落とす。何度か引き直して、
    // フィールドの内側に収まる場所を探す（角のほうへ飛んだときの取りこぼしよけ）
    Vector3 landing = bossPosition;
    for (int retry = 0; retry < 8; ++retry) {
        const float angle = Random::Range(0.0f, std::numbers::pi_v<float> * 2.0f);
        const float distance = Random::Range(minDistance, maxDistance);
        landing = Vector3{bossPosition.x + std::cos(angle) * distance, 0.0f,
                          bossPosition.z + std::sin(angle) * distance};

        if (!pFieldBounds_) {
            break;
        }
        // 縁ぎりぎりに出すと輪が半分はみ出すので、半径ぶん内側に入っているかで見る
        const float margin = pParams_->radius + pParams_->fieldMargin;
        const Vector3 toCenter = Vector3{landing.x, 0.0f, landing.z};
        const Vector3 offsets[4] = {Vector3{margin, 0.0f, 0.0f}, Vector3{-margin, 0.0f, 0.0f},
                                    Vector3{0.0f, 0.0f, margin}, Vector3{0.0f, 0.0f, -margin}};
        bool inside = true;
        for (const Vector3 &offset : offsets) {
            if (!pFieldBounds_->Contains(toCenter + offset)) {
                inside = false;
                break;
            }
        }
        if (inside) {
            break;
        }
    }

    // それでも収まらなければ、最後に押し戻して確実に内側へ入れる
    if (pFieldBounds_) {
        pFieldBounds_->ClampToField(landing);
    }
    landing.y = 0.0f;
    return landing;
}

int BossRecoveryZoneManager::CountOpen() const {
    int count = 0;
    for (const std::unique_ptr<BossRecoveryZone> &zone : zones_) {
        if (zone->IsAlive() && !zone->IsClosing()) {
            ++count;
        }
    }
    return count;
}

void BossRecoveryZoneManager::TrimOldest() {
    const int limit = (std::max)(1, pParams_->maxCount);
    while (CountOpen() > limit) {
        // いちばん長く出ているものから畳む。新しく出たほうを残すことで、
        // 直近の攻撃で決まった色がすぐ消えないようにする
        BossRecoveryZone *oldest = nullptr;
        for (const std::unique_ptr<BossRecoveryZone> &zone : zones_) {
            if (!zone->IsAlive() || zone->IsClosing()) {
                continue;
            }
            if (!oldest || zone->GetAge() > oldest->GetAge()) {
                oldest = zone.get();
            }
        }
        if (!oldest) {
            break;
        }
        oldest->BeginClose(*pParams_);
    }
}

void BossRecoveryZoneManager::DrawImGui(const Vector3 &bossPosition, const BossColorPalette &palette,
                                        const std::function<void()> &onSave) {
#ifdef USE_IMGUI
    if (!pParams_) {
        return;
    }
    if (!ImGui::CollapsingHeader("残弾の回復エリア")) {
        return;
    }
    ImGui::Indent();

    BossRecoveryZoneParams &params = *pParams_;

    ImGui::TextWrapped("ボスがひと続きの攻撃を終えるたびに1つ出ます。色は毎回ランダムで、"
                       "乗っているあいだその色の弾だけが早く戻ります。");
    ImGui::Checkbox("エリアを出す", &params.enabled);
    ImGui::SameLine();
    if (ImGui::Button("いますぐ1つ出す")) {
        NotifyAttackFinished(bossPosition, palette);
    }
    ImGui::SameLine();
    if (ImGui::Button("すべて畳む")) {
        ClearAll();
    }

    ImGui::Text("出ている数: %d / %d（これまでに %d 個）", CountOpen(), (std::max)(1, params.maxCount),
                spawnCount_);
    for (const std::unique_ptr<BossRecoveryZone> &zone : zones_) {
        if (!zone->IsAlive()) {
            continue;
        }
        const Vector4 rgba = palette.GetRgba(zone->GetColor());
        ImGui::TextColored(ImVec4{rgba.x, rgba.y, rgba.z, 1.0f}, "  %-7s %s%s",
                           BossColorPalette::GetIdText(zone->GetColor()), zone->GetPhaseName(),
                           zone->IsOccupied() ? "（乗っている）" : "");
    }

    ImGui::SeparatorText("効き方");
    ImGui::DragFloat("エリアの半径", &params.radius, 0.1f, 0.5f, 60.0f);
    ImGui::DragFloat("回復の倍率", &params.regenScale, 0.1f, 1.0f, 40.0f);
    ImGui::TextDisabled("素の回復速度（Player/Ammo の RegenPerSecond）に掛かります");
    ImGui::SliderInt("同時に出せる数", &params.maxCount, 1, 6);
    ImGui::Checkbox("満タンになったら畳む", &params.closeWhenFull);

    ImGui::SeparatorText("出る場所");
    ImGui::DragFloat("ボスからの距離(近)", &params.spawnDistanceMin, 0.5f, 0.0f, 200.0f);
    ImGui::DragFloat("ボスからの距離(遠)", &params.spawnDistanceMax, 0.5f, 0.0f, 200.0f);
    ImGui::DragFloat("フィールドの縁からの余白", &params.fieldMargin, 0.1f, 0.0f, 40.0f);

    ImGui::SeparatorText("時間");
    ImGui::DragFloat("飛び出して着地するまで", &params.popTime, 0.01f, 0.05f, 5.0f);
    ImGui::DragFloat("飛び出す弧の高さ", &params.popArcHeight, 0.2f, 0.0f, 60.0f);
    ImGui::DragFloat("輪が開くまで", &params.openTime, 0.01f, 0.05f, 5.0f);
    ImGui::DragFloat("開いている時間", &params.activeTime, 0.1f, 0.5f, 120.0f);
    ImGui::DragFloat("閉じるまで", &params.closeTime, 0.01f, 0.05f, 5.0f);

    ImGui::SeparatorText("見た目");
    ImGui::DragFloat("外枠の高さ", &params.ringHeight, 0.005f, 0.0f, 1.0f);
    ImGui::DragFloat("塗りの高さ", &params.fillHeight, 0.005f, 0.0f, 1.0f);
    ImGui::TextDisabled("同じ高さだと面が取り合って地面がちらつきます");
    ImGui::SliderFloat("塗りが脈打つ大きさ", &params.fillPulseAmount, 0.0f, 0.5f);
    ImGui::DragFloat("脈打つ速さ", &params.fillPulseSpeed, 0.05f, 0.0f, 20.0f);
    ImGui::DragFloat("粒を置く間隔(秒)", &params.auraInterval, 0.005f, 0.01f, 1.0f);
    ImGui::TextDisabled("粒の見た目は「パーティクル設定」→「ボスの土煙」の "
                        "「回復エリア: …」で調整します");

    // 調整はこの窓、保存はボスの窓、では押し忘れるので、ここからも保存できるようにしておく
    ImGui::SeparatorText("保存");
    if (onSave) {
        if (ImGui::Button("この値を保存")) {
            onSave();
        }
        ImGui::SameLine();
    }
    ImGui::TextDisabled("保存先: Assets/jsons/Boss/<ボスID>.json の \"recoveryZone\"");

    ImGui::Unindent();
#endif // USE_IMGUI
}
