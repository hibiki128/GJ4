#include "Field.h"
#include "line/LineRenderer.h"
#include <algorithm>
#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

using namespace Hagine;

namespace {

/// <summary>外周の輪の分割数（LineRenderer 側の上限は128）</summary>
constexpr uint32_t kRingSegments = 48;

} // namespace

void Field::Init(const std::string &name) {
    // 形の持ち主。中心は毎回ここから引かせるので、center_ を動かせば判定も線も一緒に動く
    collider_ = std::make_unique<CylinderCollider>();
    collider_->SetName(name);
    collider_->SetOwnerName(name);
    collider_->SetPositionGetter([this] { return center_; });
    collider_->SetInward(true); // 外へ出たら内側へ押し戻す（フィールドの壁）

    // 大きさの保存先は GameParamHub 側だけにする。コライダーにも保存の仕組みが
    // あるが、両方から読むと起動のたびにどちらが勝つか分からなくなる
    GameParamHub::Options shapeOptions{};
    shapeOptions.speed = 0.5f;
    shapeOptions.min = 1.0f;
    shapeOptions.max = 500.0f;
    shapeOptions.onChange = [this] { ApplyShape(); };

    params_.Register("Center", &center_, {0.5f});
    params_.Register("Radius", &radius_, shapeOptions);
    params_.Register("Height", &height_, shapeOptions);
    params_.Register("LineColor", &lineColor_, {0.01f, 0.0f, 1.0f, true});
    params_.Register("RingCount", &ringCount_, {1.0f, 2.0f, 16.0f});
    params_.Register("LineVisible", &isLineVisible_);

    // 登録時に保存済みの値が書き戻されるが、そのときは onChange が呼ばれない。
    // 復元された半径・高さをここでコライダーへ渡しておく
    ApplyShape();
}

void Field::ApplyShape() {
    radius_ = (std::max)(0.1f, radius_);
    height_ = (std::max)(0.1f, height_);

    if (collider_) {
        collider_->SetRadius(radius_);
        collider_->SetHeight(height_);
    }
}

void Field::ClampToField(Vector3 &position, Vector3 &velocity) const {
    if (!collider_) {
        return;
    }
    // 押し戻しの計算はコライダーが持っている（判定はXZの円だけで、高さは見ない）
    collider_->Clamp(position, velocity);
}

bool Field::Contains(const Vector3 &position) const {
    const float dx = position.x - center_.x;
    const float dz = position.z - center_.z;
    return (dx * dx + dz * dz) <= (radius_ * radius_);
}

void Field::DrawLine() const {
    if (!isLineVisible_) {
        return;
    }

    LineRenderer *pLine = LineRenderer::GetInstance();

    // center_.y を底、そこから height_ ぶん上を天井として描く。
    // AddCylinder は中心を渡す決まりなので、半分だけ持ち上げた位置を渡す
    const float halfHeight = height_ * 0.5f;
    const Vector3 drawCenter = {center_.x, center_.y + halfHeight, center_.z};
    pLine->AddCylinder(drawCenter, radius_, halfHeight, lineColor_, kRingSegments);

    // 上下の輪だけだと壁の傾きが読めないので、間にも輪を足す。
    // AddCylinder が上下2枚を描いているので、ここで描くのはその間だけ
    const int ringCount = std::clamp(ringCount_, 2, 16);
    const Vector3 axisU = {radius_, 0.0f, 0.0f};
    const Vector3 axisV = {0.0f, 0.0f, radius_};
    for (int index = 1; index < ringCount - 1; ++index) {
        const float ratio = static_cast<float>(index) / static_cast<float>(ringCount - 1);
        const Vector3 ringCenter = {center_.x, center_.y + height_ * ratio, center_.z};
        pLine->AddCircle(ringCenter, axisU, axisV, lineColor_, kRingSegments);
    }
}

void Field::DrawImGui() {
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader("フィールド（行動範囲）")) {
        return;
    }

    ImGui::Checkbox("外周の線を表示", &isLineVisible_);
    ImGui::TextDisabled("中心 (%.1f, %.1f, %.1f) / 半径 %.1f", center_.x, center_.y, center_.z, radius_);
    ImGui::TextDisabled("大きさの調整はゲームパラメータの Field から行う");
#endif // USE_IMGUI
}
