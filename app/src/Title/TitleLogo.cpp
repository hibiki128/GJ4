#include "TitleLogo.h"
#include "src/Boss/Data/BossEasing.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

using namespace Hagine;

namespace {

/// <summary>フチの太さ（ピクセル）。太さを変えると焼き直しが要るのでここに置いている</summary>
constexpr float kOutlineThickness = 9.0f;

/// <summary>フチの色。指定どおり真っ黒にする</summary>
constexpr Vector4 kOutlineColor = {0.0f, 0.0f, 0.0f, 1.0f};

/// <summary>スプライト名（書き出されるPNGのファイル名にもなるのでASCIIにしておく）</summary>
constexpr const char *kSpriteIds[] = {"TitleLogo0", "TitleLogo1", "TitleLogo2", "TitleLogo3",
                                      "TitleLogo4"};

/// <summary>はじけ具合の立ち上がりに使う割合（短く跳ね上がって、ゆっくり戻る）</summary>
constexpr float kPopAttackRatio = 0.25f;

constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kToRadian = kPi / 180.0f;

/// <summary>0〜1の割合で2色を混ぜる（アルファは混ぜない）</summary>
Vector4 MixRgb(const Vector4 &from, const Vector4 &to, float rate) {
    return {from.x + (to.x - from.x) * rate, from.y + (to.y - from.y) * rate,
            from.z + (to.z - from.z) * rate, from.w};
}

} // namespace

void TitleLogo::Init() {
    // 1文字ずつ別のテクスチャに焼く。まとめて1枚にすると文字ごとに動かせない。
    // 焼いた画像は縦が全文字で同じ（フォントの上下の余白ぶんまで含めた高さ）なので、
    // 同じ高さ・同じYで並べれば字面はそろう
    for (int i = 0; i < kCharCount; ++i) {
        glyphs_[i].text.Create(kSpriteIds[i], kChars[i], kOutlineThickness, kOutlineColor);
    }

    Restart();
}

void TitleLogo::RegisterParams() {
    params_.Register("Position", &position_, {1.0f});
    params_.Register("CharHeight", &charHeight_, {1.0f, 20.0f, 600.0f});
    params_.Register("Tracking", &tracking_, {0.5f, -200.0f, 200.0f});
    params_.Register("BaseColor", &baseColor_, {0.01f, 0.0f, 1.0f, true});

    params_.Register("DropHeight", &dropHeight_, {2.0f, 0.0f, 1200.0f});
    params_.Register("DropTime", &dropTime_, {0.01f, 0.05f, 3.0f});
    params_.Register("DropInterval", &dropInterval_, {0.01f, 0.0f, 1.0f});

    params_.Register("IdleAmplitude", &idleAmplitude_, {0.005f, 0.0f, 0.5f});
    params_.Register("IdlePeriod", &idlePeriod_, {0.01f, 0.1f, 8.0f});
    params_.Register("IdleSharpness", &idleSharpness_, {0.01f, 0.0f, 1.0f});
    params_.Register("IdleWave", &idleWave_, {0.01f, 0.0f, 1.0f});
    params_.Register("IdleBob", &idleBob_, {0.5f, 0.0f, 80.0f});
    params_.Register("TiltDegrees", &tiltDegrees_, {0.1f, 0.0f, 30.0f});
    params_.Register("TiltPeriod", &tiltPeriod_, {0.05f, 0.2f, 20.0f});

    params_.Register("PopInterval", &popInterval_, {0.05f, 0.3f, 20.0f});
    params_.Register("PopStagger", &popStagger_, {0.01f, 0.0f, 1.0f});
    params_.Register("PopDuration", &popDuration_, {0.01f, 0.05f, 3.0f});
    params_.Register("PopScale", &popScale_, {0.01f, 0.0f, 1.5f});
    params_.Register("PopHop", &popHop_, {0.5f, 0.0f, 200.0f});
    params_.Register("PopTilt", &popTilt_, {0.1f, 0.0f, 45.0f});
    params_.Register("PopColorMix", &popColorMix_, {0.01f, 0.0f, 1.0f});
}

void TitleLogo::SetPalette(const BossColorPalette &palette, const std::vector<Color> &usedColors) {
    palette_ = palette;
    usedColors_ = usedColors;

    // 文字ごとに色を割り当てておく。ボスの殻と同じ並びなので、
    // はじけたときに「あの色」が出る
    for (int i = 0; i < kCharCount; ++i) {
        if (usedColors_.empty()) {
            continue;
        }
        glyphs_[i].colorId = usedColors_[static_cast<size_t>(i) % usedColors_.size()];
    }
}

void TitleLogo::Restart() {
    elapsed_ = 0.0f;
    popTimer_ = 0.0f;
    waveTime_ = -1.0f;

    for (Glyph &glyph : glyphs_) {
        glyph.hasLanded = false;
        glyph.popTime = -1.0f;
        // 潰れ具合も初期状態へ戻す（前回の着地の余韻を持ち越さない）
        glyph.reaction = PlayerComponentReaction{};
    }
}

void TitleLogo::Update(float deltaTime) {
    elapsed_ += deltaTime;

    // --- 落ちてきて着地する ---
    for (int i = 0; i < kCharCount; ++i) {
        Glyph &glyph = glyphs_[i];
        const float landAt = static_cast<float>(i) * dropInterval_ + dropTime_;

        if (!glyph.hasLanded && elapsed_ >= landAt) {
            glyph.hasLanded = true;
            // 着地のぷにっはプレイヤーと同じものを使う。
            // 自前で似た式を書くと、ゲーム中と手ざわりがずれる
            glyph.reaction.PlayLanding(1.0f);
        }

        // 着地した文字だけ呼吸を続ける。位相を文字ぶんずらして、波が左から右へ流れるようにする
        if (glyph.hasLanded) {
            glyph.reaction.SetLoop(idleAmplitude_, idlePeriod_, idleSharpness_,
                                   static_cast<float>(i) * idleWave_);
        }
        glyph.reaction.Update();
    }

    // --- ひと巡りのポップ ---
    // 全部そろってから回し始める。落ちている最中に混ぜると何が起きているか読めない
    const bool allLanded = glyphs_[kCharCount - 1].hasLanded;
    if (allLanded && waveTime_ < 0.0f) {
        popTimer_ += deltaTime;
        if (popTimer_ >= popInterval_) {
            popTimer_ = 0.0f;
            waveTime_ = 0.0f;
        }
    }

    if (waveTime_ >= 0.0f) {
        const float previous = waveTime_;
        waveTime_ += deltaTime;

        // またいだ文字を順にはじけさせる（1フレームで複数またいでも取りこぼさない）
        for (int i = 0; i < kCharCount; ++i) {
            const float startAt = static_cast<float>(i) * popStagger_;
            if (previous <= startAt && waveTime_ > startAt) {
                glyphs_[i].popTime = 0.0f;
            }
        }

        const float waveEnd = static_cast<float>(kCharCount - 1) * popStagger_ + popDuration_;
        if (waveTime_ > waveEnd) {
            waveTime_ = -1.0f;
        }
    }

    for (Glyph &glyph : glyphs_) {
        if (glyph.popTime < 0.0f) {
            continue;
        }
        glyph.popTime += deltaTime;
        if (glyph.popTime > popDuration_) {
            glyph.popTime = -1.0f;
        }
    }
}

float TitleLogo::CalcDropOffset(int index, float &outAlpha) const {
    const float startAt = static_cast<float>(index) * dropInterval_;
    const float time = elapsed_ - startAt;

    if (time <= 0.0f) {
        outAlpha = 0.0f;
        return -dropHeight_;
    }
    if (time >= dropTime_ || dropTime_ <= 0.0f) {
        outAlpha = 1.0f;
        return 0.0f;
    }

    const float progress = time / dropTime_;
    // 落下は加速させる。等速で降りてくると軽く見えて、着地の潰れが嘘くさくなる
    outAlpha = std::clamp(progress * 3.0f, 0.0f, 1.0f);
    return -dropHeight_ * (1.0f - progress * progress);
}

float TitleLogo::CalcPopAmount(const Glyph &glyph, float popDuration) {
    if (glyph.popTime < 0.0f || popDuration <= 0.0f) {
        return 0.0f;
    }

    const float progress = std::clamp(glyph.popTime / popDuration, 0.0f, 1.0f);
    if (progress < kPopAttackRatio) {
        return SmoothInOut(progress / kPopAttackRatio);
    }
    return 1.0f - SmoothInOut((progress - kPopAttackRatio) / (1.0f - kPopAttackRatio));
}

void TitleLogo::Draw() {
    // --- 横並びの位置を決める ---
    // 幅は文字ごとに違う（フォントの送り幅そのまま）ので、実測して詰める
    float widths[kCharCount] = {};
    float totalWidth = 0.0f;
    for (int i = 0; i < kCharCount; ++i) {
        widths[i] = glyphs_[i].text.WidthAt(charHeight_);
        totalWidth += widths[i];
    }
    totalWidth += tracking_ * static_cast<float>(kCharCount - 1);

    float cursorX = position_.x - totalWidth * 0.5f;

    for (int i = 0; i < kCharCount; ++i) {
        Glyph &glyph = glyphs_[i];
        const float centerX = cursorX + widths[i] * 0.5f;
        cursorX += widths[i] + tracking_;

        float alpha = 0.0f;
        const float dropOffset = CalcDropOffset(i, alpha);
        if (alpha <= 0.0f) {
            continue; // まだ出てきていない
        }

        // 潰れ・伸びはプレイヤーと同じ計算から受け取る（x=横 / y=縦）
        const Vector3 squash = glyph.reaction.Apply(Vector3{1.0f, 1.0f, 1.0f});
        const float pop = CalcPopAmount(glyph, popDuration_) * kPopStrength[i];
        const float grow = 1.0f + popScale_ * pop;

        const float squashedHeight = charHeight_ * squash.y;
        const float width = widths[i] * squash.x * grow;
        const float height = squashedHeight * grow;

        // 潰れたぶんは下端を地面に残す。中心を持ち上げたままだと、
        // 潰れているのに浮いて見えて「踏ん張っている」感じが出ない。
        // ふくらむぶん（grow）は補正しない。中心から外へはじけさせたいので
        const float footShift = (charHeight_ - squashedHeight) * 0.5f;

        // 待機の漂いとポップの跳ね
        const float bob =
            glyph.hasLanded
                ? std::sin(2.0f * kPi * (elapsed_ / idlePeriod_ + static_cast<float>(i) * idleWave_)) *
                      idleBob_
                : 0.0f;
        const float hop = -popHop_ * pop;

        // 傾き。待機はゆっくりした波、はじけた瞬間は交互にひねる
        const float tiltSign = (i % 2 == 0) ? 1.0f : -1.0f;
        const float tilt =
            tiltDegrees_ *
                std::sin(2.0f * kPi * (elapsed_ / tiltPeriod_ + static_cast<float>(i) * idleWave_)) +
            popTilt_ * pop * tiltSign;

        // はじけた瞬間だけ殻の色が乗る（フチは黒のまま。黒に何を掛けても黒なので濁らない）
        Vector4 color = baseColor_;
        if (pop > 0.0f && !usedColors_.empty()) {
            color = MixRgb(baseColor_, palette_.GetRgba(glyph.colorId), pop * popColorMix_);
        }
        color.w = baseColor_.w * alpha;

        glyph.text.DrawTransformed(
            Vector2{centerX, position_.y + dropOffset + bob + hop + footShift},
            Vector2{width, height}, color, tilt * kToRadian);
    }
}

void TitleLogo::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SeparatorText("タイトルロゴ");

    if (ImGui::Button("登場からやり直す")) {
        Restart();
    }
    ImGui::SameLine();
    if (ImGui::Button("いまはじけさせる")) {
        // 待ちを飛ばしてひと巡りを始める
        popTimer_ = 0.0f;
        waveTime_ = 0.0f;
        for (Glyph &glyph : glyphs_) {
            glyph.popTime = -1.0f;
        }
    }

    ImGui::TextDisabled("位置・大きさ・動きの調整は ゲームパラメータ > Title/Logo");
    ImGui::TextDisabled("フチの太さ（%.0fpx）と黒は焼き込みなので、変えるにはコードの", kOutlineThickness);
    ImGui::TextDisabled("kOutlineThickness を直して起動し直すこと");
#endif // USE_IMGUI
}
