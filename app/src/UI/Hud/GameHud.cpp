#include "GameHud.h"
#include "WinApp.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>
#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

using namespace Hagine;

namespace {

constexpr const char *kHeartTexture = "Hud/heart.png";
constexpr const char *kChipTexture = "Hud/color_chip.png";
constexpr const char *kDpadTexture = "Tutorial/dpad.png";
constexpr const char *kRtTexture = "Tutorial/btn_rt.png";
constexpr const char *kLbTexture = "Tutorial/btn_lb.png";
constexpr const char *kRbTexture = "Tutorial/btn_rb.png";
constexpr const char *kMenuTexture = "Hud/btn_menu.png";

/// <summary>反応が収まるまでの時間（秒）。短いほど機敏に見える</summary>
constexpr float kDamageAnimTime = 0.45f;
constexpr float kColorAnimTime = 0.30f;
constexpr float kShotAnimTime = 0.16f;
constexpr float kBossHitAnimTime = 0.35f;
constexpr float kReloadTickAnimTime = 0.28f;

/// <summary>補給表示が出入りする速さ（1秒でこの割合ぶん詰める）</summary>
constexpr float kReloadShowRate = 9.0f;

/// <summary>バーの表示値が実際の値へ追いつく速さ（1秒でこの割合ぶん詰める）</summary>
constexpr float kBossBarFollowRate = 3.2f;

constexpr float kTwoPi = std::numbers::pi_v<float> * 2.0f;

/// <summary>失ったハートの色</summary>
const Vector4 kLostHeartColor = {0.18f, 0.19f, 0.24f, 0.85f};
const Vector4 kWhite = {1.0f, 1.0f, 1.0f, 1.0f};
const Vector4 kFrameColor = {0.08f, 0.08f, 0.11f, 0.95f};
const Vector4 kBarBackColor = {0.20f, 0.21f, 0.26f, 0.95f};
const Vector4 kBossBarColor = {0.90f, 0.25f, 0.28f, 1.0f};

/// <summary>1→0 へ落ちる値を、跳ねてから収まる曲線にする</summary>
/// <param name="anim">反応の残り（1で起きた瞬間・0で収まった）</param>
/// <returns>float: 0を中心にした揺れ</returns>
float Bounce(float anim) {
    if (anim <= 0.0f) {
        return 0.0f;
    }
    // 起きた直後がいちばん大きく、減衰しながら数回揺れる
    return std::sin(anim * std::numbers::pi_v<float> * 3.0f) * anim * anim;
}

} // namespace

void GameHud::Init(const BossColorPalette &palette) {
    if (isInitialized_) {
        return;
    }
    isInitialized_ = true;
    palette_ = palette;

    rects_.Initialize(kRectCapacity);

    // ハートは枚数ぶん持つ。スプライトは1フレームに1回しか描けない
    for (GameUi::UiSprite &heart : hearts_) {
        heart.Initialize(kHeartTexture);
    }
    for (GameUi::UiSprite &chip : chips_) {
        chip.Initialize(kChipTexture);
    }
    dpad_.Initialize(kDpadTexture);
    triggerRt_.Initialize(kRtTexture);
    buttonLb_.Initialize(kLbTexture);
    buttonRb_.Initialize(kRbTexture);
    buttonMenu_.Initialize(kMenuTexture);

    ammoNumber_.Create("Hud_Ammo", 4);
    reloadChip_.Initialize(kChipTexture);
    reloadNumber_.Create("Hud_ReloadAmmo", 4);
    reloadHint_.Create("Hud_ReloadHint", "かいふく", 7.0f);
    // フチは太めに。ゲーム画面の上に直接載るので、細いと背景に負けて読めなくなる
    shootHint_.Create("Hud_ShootHint", "しゃげき", 7.0f);
    pauseHint_.Create("Hud_PauseHint", "ぽーず", 7.0f);
}

void GameHud::Finalize() {
    rects_.Finalize();
    for (GameUi::UiSprite &heart : hearts_) {
        heart.Finalize();
    }
    for (GameUi::UiSprite &chip : chips_) {
        chip.Finalize();
    }
    dpad_.Finalize();
    triggerRt_.Finalize();
    buttonLb_.Finalize();
    buttonRb_.Finalize();
    buttonMenu_.Finalize();
    ammoNumber_.Finalize();
    reloadChip_.Finalize();
    reloadNumber_.Finalize();
    reloadHint_.Finalize();
    shootHint_.Finalize();
    pauseHint_.Finalize();
    isInitialized_ = false;
}

void GameHud::RegisterParams() {
    params_.Register("HeartOrigin", &heartOrigin_, {1.0f});
    params_.Register("HeartSize", &heartSize_, {0.5f, 8.0f, 200.0f});
    params_.Register("HeartSpacing", &heartSpacing_, {0.5f, 8.0f, 200.0f});

    params_.Register("BossBarCenter", &bossBarCenter_, {1.0f});
    params_.Register("BossBarWidth", &bossBarWidth_, {1.0f, 40.0f, 1700.0f});
    params_.Register("BossBarHeight", &bossBarHeight_, {0.5f, 6.0f, 120.0f});
    params_.Register("BossNameSpace", &bossNameSpace_, {0.5f, 0.0f, 200.0f});

    params_.Register("ColorPanelCenter", &colorPanelCenter_, {1.0f});
    params_.Register("ChipSize", &chipSize_, {0.5f, 8.0f, 200.0f});
    params_.Register("ChipSideScale", &chipSideScale_, {0.01f, 0.1f, 1.0f});
    params_.Register("ChipArcRadiusX", &chipArcRadiusX_, {0.5f, 8.0f, 400.0f});
    params_.Register("ChipArcRadiusY", &chipArcRadiusY_, {0.5f, 0.0f, 400.0f});
    params_.Register("ChipArcAngleDeg", &chipArcAngleDeg_, {0.5f, 10.0f, 120.0f});
    params_.Register("DpadOffsetY", &dpadOffsetY_, {0.5f, 0.0f, 300.0f});
    params_.Register("AmmoTextSize", &ammoTextSize_, {0.5f, 8.0f, 120.0f});
    params_.Register("DpadSize", &dpadSize_, {0.5f, 8.0f, 200.0f});
    params_.Register("HintSize", &hintSize_, {0.5f, 8.0f, 120.0f});
    params_.Register("CycleButtonSize", &cycleButtonSize_, {0.5f, 8.0f, 200.0f});
    params_.Register("CycleButtonGap", &cycleButtonGap_, {0.5f, 0.0f, 200.0f});

    params_.Register("ReloadOffset", &reloadOffset_, {1.0f});
    params_.Register("ReloadPanelWidth", &reloadPanelWidth_, {1.0f, 40.0f, 600.0f});
    params_.Register("ReloadPanelHeight", &reloadPanelHeight_, {0.5f, 16.0f, 300.0f});
    params_.Register("ReloadChipSize", &reloadChipSize_, {0.5f, 8.0f, 200.0f});
    params_.Register("ReloadTextSize", &reloadTextSize_, {0.5f, 8.0f, 120.0f});
    params_.Register("ReloadHintSize", &reloadHintSize_, {0.5f, 8.0f, 120.0f});

    params_.Register("MenuCenter", &menuCenter_, {1.0f});
    params_.Register("MenuSize", &menuSize_, {0.5f, 8.0f, 200.0f});
    params_.Register("MenuTextSize", &menuTextSize_, {0.5f, 8.0f, 120.0f});
    params_.Register("MenuTextGap", &menuTextGap_, {0.5f, 0.0f, 200.0f});
}

Vector4 GameHud::ColorOf(Color color, float alpha) const {
    Vector4 rgba = palette_.GetRgba(color);
    rgba.w = alpha;
    return rgba;
}

void GameHud::PlayDamaged() { damageAnim_ = 1.0f; }
void GameHud::PlayShot() { shotAnim_ = 1.0f; }
void GameHud::PlayBossDamaged() { bossHitAnim_ = 1.0f; }
void GameHud::PlayReloadTick() { reloadTickAnim_ = 1.0f; }

void GameHud::PlayColorChanged() {
    colorAnim_ = 1.0f;
    // 次の色へ進んだなら右から、戻ったなら左から流れてくるように見せる
    const int current = ToColorIndex(snapshot_.playerColor);
    const int forward = (previousColorIndex_ + 1) % kGameColorCount;
    arcSlide_ = (current == forward) ? 1.0f : -1.0f;
}

void GameHud::Update(float deltaTime, const GameHudSnapshot &snapshot) {
    const int previousHp = snapshot_.hp;
    const Color previousColor = snapshot_.playerColor;
    previousColorIndex_ = ToColorIndex(previousColor);

    snapshot_ = snapshot;
    time_ += deltaTime;

    // シーンから知らせ忘れても最低限は反応するよう、値の変化からも拾っておく
    if (snapshot_.hp < previousHp) {
        PlayDamaged();
    }
    if (snapshot_.playerColor != previousColor) {
        PlayColorChanged();
    }

    // バーの表示値を実際の値へ寄せる。
    // 第2形態は始まった瞬間に球がそろっていくので、ここが追いつく動きが
    // そのまま「だんだん増えていく」表示になる
    const float target =
        (snapshot_.bossMaxHp > 0.0f) ? std::clamp(snapshot_.bossHp / snapshot_.bossMaxHp, 0.0f, 1.0f)
                                     : 0.0f;
    const float follow = std::clamp(deltaTime * kBossBarFollowRate, 0.0f, 1.0f);
    bossRatio_ += (target - bossRatio_) * follow;

    // 反応を収めていく
    auto decay = [deltaTime](float &value, float duration) {
        if (value > 0.0f) {
            value = (std::max)(0.0f, value - deltaTime / duration);
        }
    };
    decay(damageAnim_, kDamageAnimTime);
    decay(colorAnim_, kColorAnimTime);
    decay(shotAnim_, kShotAnimTime);
    decay(bossHitAnim_, kBossHitAnimTime);
    decay(reloadTickAnim_, kReloadTickAnimTime);

    // 補給の表示は、乗った瞬間に浮き上がり、離れると引っ込む
    const float showTarget = snapshot_.reloadActive ? 1.0f : 0.0f;
    reloadShow_ += (showTarget - reloadShow_) * std::clamp(deltaTime * kReloadShowRate, 0.0f, 1.0f);

    // 1発戻るたびに弾ませる。数字だけだと増えたことに気づきにくい
    if (snapshot_.reloadActive) {
        if (previousReloadAmmo_ >= 0 && snapshot_.reloadAmmo > previousReloadAmmo_) {
            PlayReloadTick();
        }
        previousReloadAmmo_ = snapshot_.reloadAmmo;
    } else {
        previousReloadAmmo_ = -1;
    }

    // 弧の流れは色替えの反応と一緒に収まる
    arcSlide_ *= (colorAnim_ > 0.0f) ? colorAnim_ : 0.0f;
}

void GameHud::Draw() {
    if (!isInitialized_) {
        return;
    }
    rects_.BeginFrame();
    ammoNumber_.BeginFrame();
    reloadNumber_.BeginFrame();

    if (showHearts_) {
        DrawHearts();
    }
    if (showBossBar_) {
        DrawBossBar();
    }
    DrawColorPanel();
    DrawReloadPanel();
    DrawMenuButton();
}

void GameHud::DrawHearts() {
    const int maxHearts = (std::min)(kMaxHearts, (std::max)(0, snapshot_.maxHp));

    // 被弾したら全体が小刻みに揺れる
    const float shake = Bounce(damageAnim_) * 7.0f;

    for (int index = 0; index < maxHearts; ++index) {
        const bool alive = (index < snapshot_.hp);

        // 減った直後の1つは大きく弾ませる（どれが減ったのか目で追えるように）
        const bool justLost = (!alive && index == snapshot_.hp);
        const float pop = justLost ? Bounce(damageAnim_) * 0.35f : 0.0f;

        const float size = heartSize_ * (1.0f + pop);
        const Vector2 center = {heartOrigin_.x + heartSpacing_ * static_cast<float>(index) + shake,
                                heartOrigin_.y + shake * 0.5f};

        // 生きているハートはいま選んでいる色。失ったぶんは沈んだ色にする
        const Vector4 color = alive ? ColorOf(snapshot_.playerColor) : kLostHeartColor;

        // 画像の縦横比を保つ
        const Vector2 base = hearts_[index].GetBaseSize();
        const float width = (base.y > 0.0f) ? size * (base.x / base.y) : size;
        hearts_[index].Draw(center, {width, size}, color);
    }
}

void GameHud::DrawBossBar() {
    if (!snapshot_.bossVisible || snapshot_.bossMaxHp <= 0.0f) {
        return;
    }

    // 球が減ると光って揺れる
    const float shake = Bounce(bossHitAnim_) * 5.0f;
    const Vector2 center = {bossBarCenter_.x + shake, bossBarCenter_.y};

    // 枠 → 下地 → 中身 の順に重ねる
    rects_.Draw(center, {bossBarWidth_ + 10.0f, bossBarHeight_ + 10.0f}, kFrameColor);
    rects_.Draw(center, {bossBarWidth_, bossBarHeight_}, kBarBackColor);

    const float filled = bossBarWidth_ * std::clamp(bossRatio_, 0.0f, 1.0f);
    if (filled > 0.0f) {
        Vector4 barColor = kBossBarColor;
        // 減った直後だけ白く光らせる
        const float flash = bossHitAnim_ * bossHitAnim_;
        barColor.x = std::clamp(barColor.x + flash, 0.0f, 1.0f);
        barColor.y = std::clamp(barColor.y + flash, 0.0f, 1.0f);
        barColor.z = std::clamp(barColor.z + flash, 0.0f, 1.0f);

        rects_.Draw({center.x - bossBarWidth_ * 0.5f + filled * 0.5f, center.y},
                    {filled, bossBarHeight_}, barColor);
    }

    // 名前を入れるぶんの余白。まだ何も置かないが、場所だけ取っておく
    (void)bossNameSpace_;
}

void GameHud::DrawColorPanel() {
    const Vector2 arcCenter = colorPanelCenter_;
    const int currentIndex = ToColorIndex(snapshot_.playerColor);

    // 前・今・次の3色を、平たい弧に並べる。
    // 真ん中（真上）が選んでいる色で、両隣が前後の色。全部の色は出さない
    const float step = chipArcAngleDeg_ * (std::numbers::pi_v<float> / 180.0f);
    const float top = -std::numbers::pi_v<float> * 0.5f;

    Vector2 selectedCenter = {arcCenter.x, arcCenter.y - chipArcRadiusY_};

    for (int slot = 0; slot < kChipCount; ++slot) {
        const int offset = slot - 1; // -1:前 / 0:今 / +1:次
        const int colorIndex =
            ((currentIndex + offset) % kGameColorCount + kGameColorCount) % kGameColorCount;

        // 色を変えた直後は、まだ1つ前の位置にいるところから流れてくる
        const float angle = top + step * (static_cast<float>(offset) + arcSlide_);
        const Vector2 chipCenter = {arcCenter.x + std::cos(angle) * chipArcRadiusX_,
                                    arcCenter.y + std::sin(angle) * chipArcRadiusY_};

        const bool isCurrent = (offset == 0);
        if (isCurrent) {
            selectedCenter = chipCenter;
        }

        // 選んでいる色だけ大きく・くっきり。前後は小さく半透明にして脇へ引く
        const float pop = isCurrent ? Bounce(colorAnim_) * 0.28f : 0.0f;
        const float size = chipSize_ * (isCurrent ? 1.0f : chipSideScale_) * (1.0f + pop);
        const float alpha = isCurrent ? 1.0f : 0.45f;

        chips_[slot].Draw(chipCenter, {size, size}, ColorOf(FromColorIndex(colorIndex), alpha));
    }

    // 残弾は、選んでいる色のチップの上に数字だけ出す
    {
        const float pop = Bounce(shotAnim_) * 0.35f;
        const float height = ammoTextSize_ * (1.0f + pop);
        const std::string text = std::to_string(snapshot_.ammo);

        // 数字は右揃えで描かれるので、桁数ぶんの幅の半分だけ右へずらすと中央に来る。
        // 1文字の幅は UiNumber のセル比（横÷縦）とおよそ同じ
        constexpr float kDigitAspect = 0.78f / 1.32f;
        const float halfWidth = static_cast<float>(text.size()) * height * kDigitAspect * 0.5f;

        ammoNumber_.DrawRight(text, selectedCenter.x + halfWidth, selectedCenter.y, height, kWhite);
    }

    // 色送りの LB / RB。弧の外側、左上と右上に置いて回る向きを示す
    {
        const float outward = chipArcRadiusX_ + chipSize_ * 0.5f + cycleButtonGap_;
        const float lift = chipArcRadiusY_ + chipSize_ * 0.35f;

        const Vector2 lbBase = buttonLb_.GetBaseSize();
        const float lbWidth =
            (lbBase.y > 0.0f) ? cycleButtonSize_ * (lbBase.x / lbBase.y) : cycleButtonSize_;
        buttonLb_.Draw({arcCenter.x - outward, arcCenter.y - lift}, {lbWidth, cycleButtonSize_},
                       kWhite);

        const Vector2 rbBase = buttonRb_.GetBaseSize();
        const float rbWidth =
            (rbBase.y > 0.0f) ? cycleButtonSize_ * (rbBase.x / rbBase.y) : cycleButtonSize_;
        buttonRb_.Draw({arcCenter.x + outward, arcCenter.y - lift}, {rbWidth, cycleButtonSize_},
                       kWhite);
    }

    // 十字ボタンの図は弧の下。図の色の並びが、そのまま「どの方向がどの色か」の対応表になる
    const Vector2 dpadCenter = {arcCenter.x, arcCenter.y + dpadOffsetY_};
    dpad_.Draw(dpadCenter, {dpadSize_, dpadSize_}, kWhite);

    // 「RT しゃげき」は十字ボタンの右へ並べる。撃つと少し弾む
    const float shotPop = 1.0f + Bounce(shotAnim_) * 0.25f;
    const Vector2 rtBase = triggerRt_.GetBaseSize();
    const float rtHeight = dpadSize_ * 0.58f * shotPop;
    const float rtWidth = (rtBase.y > 0.0f) ? rtHeight * (rtBase.x / rtBase.y) : rtHeight;
    const Vector2 rtCenter = {dpadCenter.x + dpadSize_ * 0.5f + rtWidth * 0.5f + 24.0f,
                              dpadCenter.y};
    triggerRt_.Draw(rtCenter, {rtWidth, rtHeight}, kWhite);

    shootHint_.DrawLeft(rtCenter.x + rtWidth * 0.5f + 14.0f, dpadCenter.y, hintSize_, kWhite);
}

void GameHud::DrawReloadPanel() {
    // 出きっていないあいだも描くが、消えているときは何も出さない
    const float appear = std::clamp(reloadShow_, 0.0f, 1.0f);
    if (appear <= 0.01f) {
        return;
    }

    // 出るときは少し下から浮き上がってくる
    const Vector2 center = {colorPanelCenter_.x + reloadOffset_.x,
                            colorPanelCenter_.y + reloadOffset_.y + (1.0f - appear) * 22.0f};

    const Vector4 reloadColor = ColorOf(snapshot_.reloadColor, appear);
    // 1発戻った瞬間に大きくなる
    const float pop = Bounce(reloadTickAnim_) * 0.26f;

    // 板。明るい床の上でも沈まないよう、枠を足して濃いめに敷く。
    // 戻っている色をうっすら混ぜて、どの色の話なのか板の時点で分かるようにする
    rects_.Draw(center, {reloadPanelWidth_ + 8.0f, reloadPanelHeight_ + 8.0f},
                {kFrameColor.x, kFrameColor.y, kFrameColor.z, kFrameColor.w * appear});
    const Vector4 plateColor = {0.09f + reloadColor.x * 0.16f, 0.11f + reloadColor.y * 0.16f,
                                0.17f + reloadColor.z * 0.16f, 0.94f * appear};
    rects_.Draw(center, {reloadPanelWidth_, reloadPanelHeight_}, plateColor);

    // 戻っている色の球。板の左寄せ
    const float chipSize = reloadChipSize_ * (1.0f + pop);
    const Vector2 chipCenter = {center.x - reloadPanelWidth_ * 0.5f + 16.0f + reloadChipSize_ * 0.5f,
                                center.y - reloadPanelHeight_ * 0.10f};
    reloadChip_.Draw(chipCenter, {chipSize, chipSize}, reloadColor);

    // 残弾。桁が変わっても右端が動かないよう右揃えにする
    {
        const float height = reloadTextSize_ * (1.0f + pop);
        const Vector4 numberColor = {1.0f, 1.0f, 1.0f, appear};
        reloadNumber_.DrawRight(std::to_string(snapshot_.reloadAmmo),
                                center.x + reloadPanelWidth_ * 0.5f - 16.0f, chipCenter.y, height,
                                numberColor);
    }

    // 下に細いゲージ。数字だけだと満タンまでの距離が分からない
    {
        const float gaugeWidth = reloadPanelWidth_ - 32.0f;
        const float gaugeY = center.y + reloadPanelHeight_ * 0.5f - 12.0f;
        rects_.Draw({center.x, gaugeY}, {gaugeWidth, 8.0f}, {1.0f, 1.0f, 1.0f, 0.18f * appear});

        const float ratio =
            (snapshot_.maxAmmo > 0)
                ? std::clamp(static_cast<float>(snapshot_.reloadAmmo) /
                                 static_cast<float>(snapshot_.maxAmmo),
                             0.0f, 1.0f)
                : 0.0f;
        const float filled = gaugeWidth * ratio;
        if (filled > 0.0f) {
            // 戻った瞬間だけ白く光らせる
            const float flash = reloadTickAnim_ * reloadTickAnim_;
            const Vector4 barColor = {std::clamp(reloadColor.x + flash, 0.0f, 1.0f),
                                      std::clamp(reloadColor.y + flash, 0.0f, 1.0f),
                                      std::clamp(reloadColor.z + flash, 0.0f, 1.0f), appear};
            rects_.Draw({center.x - gaugeWidth * 0.5f + filled * 0.5f, gaugeY}, {filled, 8.0f},
                        barColor);
        }
    }

    // 「かいふく」は板の上。板の中に入れると数字と競って読みにくい
    reloadHint_.DrawCentered({center.x, center.y - reloadPanelHeight_ * 0.5f - reloadHintSize_ * 0.7f},
                             reloadHintSize_, {1.0f, 1.0f, 1.0f, appear});
}

void GameHud::DrawMenuButton() {
    // 右下。押すとポーズ画面が開くことを図と文字で示すだけで、当たり判定は持たない
    const Vector2 base = buttonMenu_.GetBaseSize();
    const float width = (base.y > 0.0f) ? menuSize_ * (base.x / base.y) : menuSize_;
    buttonMenu_.Draw(menuCenter_, {width, menuSize_}, kWhite);

    pauseHint_.DrawLeft(menuCenter_.x + width * 0.5f + menuTextGap_, menuCenter_.y, menuTextSize_,
                        kWhite);
}

void GameHud::DrawImGui() {
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader("HUD")) {
        return;
    }
    ImGui::Text("HP %d / %d   弾 %d / %d", snapshot_.hp, snapshot_.maxHp, snapshot_.ammo,
                snapshot_.maxAmmo);
    ImGui::Text("色: %s", GetColorIdText(snapshot_.playerColor));
    ImGui::Text("補給: %s（%s %d発）", snapshot_.reloadActive ? "中" : "なし",
                GetColorIdText(snapshot_.reloadColor), snapshot_.reloadAmmo);
    ImGui::Text("ボス %.0f / %.0f （表示 %.0f%%）", snapshot_.bossHp, snapshot_.bossMaxHp,
                bossRatio_ * 100.0f);
    ImGui::TextDisabled("配置と大きさは ゲームパラメータ の Hud から");

    if (ImGui::Button("被弾の反応")) {
        PlayDamaged();
    }
    ImGui::SameLine();
    if (ImGui::Button("色替えの反応")) {
        PlayColorChanged();
    }
    ImGui::SameLine();
    if (ImGui::Button("ボス被弾の反応")) {
        PlayBossDamaged();
    }
    if (ImGui::Button("補給の反応")) {
        PlayReloadTick();
    }
#endif // USE_IMGUI
}
