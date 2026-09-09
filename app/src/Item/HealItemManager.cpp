#include "HealItemManager.h"
#include "3d/Object/Base/BaseObjectManager.h"
#include <algorithm>
#include <cmath>
#include <numbers>

using namespace Hagine;

HealItemManager *HealItemManager::GetInstance() {
    static HealItemManager instance;
    return &instance;
}

void HealItemManager::Init(const std::string &baseName) {
    // 前のシーンのぶんを先に捨てる。
    // 非所有登録はシーンを切り替えても外れないので、ここで破棄しないと
    // 前のシーンのアイテムが次のシーンでも更新・描画され続けてしまう
    // （登録解除は BaseObject のデストラクタが自動でやってくれる）
    items_.clear();
    items_.reserve(kMaxItemCount);

    isPaused_ = false;

    for (size_t i = 0; i < kMaxItemCount; ++i) {
        auto item = std::make_unique<HealItem>();

        // 名前が BaseObjectManager のキーになるので、必ず一意にする
        item->InitItem(baseName + "_" + std::to_string(i), &params_, &hooks_);

        // 実体はこのクラスが持ったまま、参照だけマネージャーに渡す。
        // これでアイテムの Update / Draw はマネージャーが回してくれる
        BaseObjectManager::GetInstance()->RegisterExternal(item.get());

        items_.push_back(std::move(item));
    }
}

void HealItemManager::Finalize() {
    // 破棄すれば BaseObject のデストラクタが登録解除までやってくれる
    items_.clear();
    isPaused_ = false;
}

bool HealItemManager::Spawn(const Vector3 &position) {
    // 待機中のアイテムを探して出す
    for (auto &item : items_) {
        if (item->IsActive()) {
            continue;
        }

        // 落とし主の中心から出すと地面や相手の体に埋まるので、少し上へずらす
        item->Spawn(position + Vector3{0.0f, params_.spawnHeight, 0.0f});
        item->SetPaused(isPaused_);
        return true;
    }

    // 空きが無い場合は出さない（同時に出せる数の上限）
    return false;
}

HealItem *HealItemManager::FindNearestSeal(const Vector3 &from, const Vector3 &to, Color color,
                                           float bulletRadius, Vector3 *outPoint) {
    // 膜に反応するのは黄色い弾だけ。他の色は素通りさせる（弾も照準も止めない）
    if (color != Color::YELLOW) {
        return nullptr;
    }

    HealItem *nearest = nullptr;
    float nearestDistance = 0.0f;
    Vector3 nearestPoint{};

    // 数個しか無いので総当たりでよい
    for (auto &item : items_) {
        float distance = 0.0f;
        Vector3 point{};
        if (!item->RaycastSeal(from, to, bulletRadius, distance, point)) {
            continue;
        }
        if (nearest && distance >= nearestDistance) {
            continue; // すでに手前で当たっているものがある
        }

        nearest = item.get();
        nearestDistance = distance;
        nearestPoint = point;
    }

    if (nearest && outPoint) {
        *outPoint = nearestPoint;
    }
    return nearest;
}

bool HealItemManager::RaycastHit(const Vector3 &worldStart, const Vector3 &worldEnd, Color color,
                                 float bulletRadius) {
    HealItem *item = FindNearestSeal(worldStart, worldEnd, color, bulletRadius, nullptr);
    if (!item) {
        return false;
    }

    // まだ割れなくても、当たった弾はここで役目を終える
    item->ApplySealHit();
    return true;
}

bool HealItemManager::RaycastPoint(const Vector3 &worldStart, const Vector3 &worldEnd, Color color,
                                   float bulletRadius, AimHit &outHit) {
    Vector3 point{};
    HealItem *item = FindNearestSeal(worldStart, worldEnd, color, bulletRadius, &point);
    if (!item) {
        return false;
    }

    outHit.point = point;
    // エイムアシストの寄せ先は膜の真ん中。縁をかすっているときほど大きく寄る
    outHit.center = item->GetSealCenter();
    return true;
}

bool HealItemManager::FindLockOnTarget(const LockOnRequest &request, ShootableLockOnResult &out) {
    out = ShootableLockOnResult{};

    // 狙えるのは黄色を撃っているときだけ。膜は黄色い弾でしか壊せないので、
    // 他の色で構えているあいだに強調表示が出ると狙える的だと誤解させてしまう
    if (request.color != Color::YELLOW || request.aimDirection.LengthSq() <= 0.0f) {
        return false;
    }

    const Vector3 aimDirection = request.aimDirection.Normalize();
    const float maxAngleCos =
        std::cos(request.maxAngleDegrees * std::numbers::pi_v<float> / 180.0f);

    // 照準に一番近い（なす角の小さい）ものを選ぶ。ボスの殻と同じ選び方
    float bestCos = -2.0f;
    for (size_t i = 0; i < items_.size(); ++i) {
        const HealItem &item = *items_[i];
        if (item.GetState() != HealItem::State::Sealed) {
            continue; // 割れた後の中身は撃つものではないので狙わせない
        }

        const Vector3 toItem = item.GetSealCenter() - request.origin;
        const float distance = toItem.Length();
        if (distance <= 0.0001f || distance > request.maxDistance) {
            continue;
        }

        const float angleCos = aimDirection.Dot(toItem / distance);
        if (angleCos < maxAngleCos || angleCos <= bestCos) {
            continue; // 許容角度から外れている / すでにもっと近いものがある
        }

        bestCos = angleCos;
        out.found = true;
        out.targetId = static_cast<int>(i);
        out.worldPosition = item.GetSealCenter();
        out.distance = distance;
        out.angleDegrees =
            std::acos(std::clamp(angleCos, -1.0f, 1.0f)) * 180.0f / std::numbers::pi_v<float>;
    }

    return out.found;
}

void HealItemManager::SetLockOnHighlight(int targetId, bool valid) {
    // 指定以外は必ず解除する。狙う先が移ったときに前の強調が残らないようにするため
    for (size_t i = 0; i < items_.size(); ++i) {
        items_[i]->SetHighlight(valid && static_cast<int>(i) == targetId);
    }
}

void HealItemManager::SetSealColor(const Vector4 &rgba) {
    // 透け具合（アルファ）はこちらの調整値のまま。色味だけを合わせる
    params_.sealRgba = Vector4{rgba.x, rgba.y, rgba.z, params_.sealRgba.w};
}

void HealItemManager::SetPaused(bool paused) {
    isPaused_ = paused;
    for (auto &item : items_) {
        item->SetPaused(paused);
    }
}

size_t HealItemManager::GetActiveCount() const {
    size_t count = 0;
    for (const auto &item : items_) {
        if (item->IsActive()) {
            ++count;
        }
    }
    return count;
}
