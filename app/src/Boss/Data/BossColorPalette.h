#pragma once
#include "src/Character/ColorStruct.h"
#include "type/Vector4.h"
#include <array>
#include <string>
#include <vector>

/// <summary>ゲームで扱う色の総数（ColorStruct.h の Color と対応）</summary>
inline constexpr int kGameColorCount = 4;

/// <summary>
/// 色のマスターデータ（4色固定）と、ボスごとの使用色サブセット（2〜4色）を保持する。
/// 色そのものに固有効果は持たせない。表示色と識別子だけを扱う。
/// マスターは jsons/Boss/ColorMaster.json から読み込む。
/// </summary>
class BossColorPalette {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>マスターデータ（4色のRGBA）を読み込む。ファイルが無ければ既定色のまま</summary>
    void LoadMaster();

    /// <summary>ボスが使う色のサブセットを設定する（空なら全4色を使う）</summary>
    /// <param name="colors">使用する色</param>
    void SetUsedColors(const std::vector<Color> &colors);

    /// <summary>ボスが使う色のサブセットを取得する</summary>
    const std::vector<Color> &GetUsedColors() const { return usedColors_; }

    /// <summary>色のRGBAを取得する</summary>
    /// <param name="color">色</param>
    /// <returns>Vector4: 表示色</returns>
    Hagine::Vector4 GetRgba(Color color) const;

    /// <summary>色の識別子文字列を取得する（"RED" など）</summary>
    static const char *GetIdText(Color color);

    /// <summary>識別子文字列から色を引く</summary>
    /// <param name="id">"RED" などの識別子（大文字小文字は問わない）</param>
    /// <param name="out">見つかった色</param>
    /// <returns>bool: 見つかれば true</returns>
    static bool TryParse(const std::string &id, Color &out);

    /// <summary>色を配列添字へ変換する</summary>
    static int ToIndex(Color color) { return static_cast<int>(color); }

    /// <summary>配列添字を色へ変換する</summary>
    static Color FromIndex(int index);

private:
    /// ===================================================
    /// private variables
    /// ===================================================

    // 既定色（マスターJSONが無い場合はこの値が使われる）
    std::array<Hagine::Vector4, kGameColorCount> rgba_ = {
        Hagine::Vector4{0.90f, 0.25f, 0.25f, 1.0f}, // RED
        Hagine::Vector4{0.25f, 0.50f, 0.95f, 1.0f}, // BLUE
        Hagine::Vector4{0.30f, 0.80f, 0.40f, 1.0f}, // GREEN
        Hagine::Vector4{0.95f, 0.85f, 0.30f, 1.0f}  // YELLOW
    };

    // ボスごとの使用色（既定は3色）
    std::vector<Color> usedColors_ = {Color::RED, Color::BLUE, Color::GREEN};
};
