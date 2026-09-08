#pragma once

enum class Color {
	RED,
	BLUE,
	GREEN,
	YELLOW,
};

/// <summary>ゲームで扱う色の総数（上の Color と対応）</summary>
inline constexpr int kGameColorCount = 4;

/// <summary>色を配列添字へ変換する</summary>
inline constexpr int ToColorIndex(Color color) { return static_cast<int>(color); }

/// <summary>配列添字から色を引く（範囲外なら RED）</summary>
inline constexpr Color FromColorIndex(int index) {
	if (index < 0 || index >= kGameColorCount) {
		return Color::RED;
	}
	return static_cast<Color>(index);
}

/// <summary>色の識別子文字列（"RED" など。範囲外は "UNKNOWN"）</summary>
inline constexpr const char* GetColorIdText(Color color) {
	switch (color) {
	case Color::RED:    return "RED";
	case Color::BLUE:   return "BLUE";
	case Color::GREEN:  return "GREEN";
	case Color::YELLOW: return "YELLOW";
	}
	return "UNKNOWN";
}
