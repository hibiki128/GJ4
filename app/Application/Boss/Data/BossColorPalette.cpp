#include "BossColorPalette.h"
#include "data/DataHandler.h"
#include <algorithm>
#include <cctype>

namespace {
constexpr const char *kColorIds[kGameColorCount] = {"RED", "BLUE", "GREEN", "YELLOW"};
} // namespace

void BossColorPalette::LoadMaster() {
    // jsons/Boss/ColorMaster.json（無ければ既定色のまま）
    Hagine::DataHandler master("Boss", "ColorMaster");
    for (int i = 0; i < kGameColorCount; ++i) {
        rgba_[i] = master.Load<Hagine::Vector4>(kColorIds[i], rgba_[i]);
    }
}

void BossColorPalette::SetUsedColors(const std::vector<Color> &colors) {
    usedColors_.clear();
    for (Color color : colors) {
        // 同じ色を二重に登録しない（色配布の確率が偏るため）
        if (std::find(usedColors_.begin(), usedColors_.end(), color) == usedColors_.end()) {
            usedColors_.push_back(color);
        }
    }
    if (usedColors_.empty()) {
        usedColors_ = {Color::RED, Color::BLUE, Color::GREEN, Color::YELLOW};
    }
}

Hagine::Vector4 BossColorPalette::GetRgba(Color color) const {
    const int index = ToIndex(color);
    if (index < 0 || index >= kGameColorCount) {
        return Hagine::Vector4{1.0f, 1.0f, 1.0f, 1.0f};
    }
    return rgba_[index];
}

const char *BossColorPalette::GetIdText(Color color) {
    const int index = ToIndex(color);
    if (index < 0 || index >= kGameColorCount) {
        return "UNKNOWN";
    }
    return kColorIds[index];
}

bool BossColorPalette::TryParse(const std::string &id, Color &out) {
    std::string upper = id;
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    for (int i = 0; i < kGameColorCount; ++i) {
        if (upper == kColorIds[i]) {
            out = FromIndex(i);
            return true;
        }
    }
    return false;
}

Color BossColorPalette::FromIndex(int index) {
    if (index < 0 || index >= kGameColorCount) {
        return Color::RED;
    }
    return static_cast<Color>(index);
}
