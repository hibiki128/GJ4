#pragma once

/// ===================================================
/// プリコンパイル済みヘッダ
///
/// 変更頻度が低く、多くの .cpp から読まれる重いヘッダだけを置く場所。
/// app.vcxproj で強制インクルード(/FI)と /Yu を設定しているので、
/// 各 .cpp の先頭に #include "pch.h" と書く必要は無い。
///
/// ここに Hagine エンジンや本プロジェクト自身のヘッダは入れないこと。
/// 1つ直すたびに src 以下の全ソースが再コンパイルになり、かえって遅くなる。
/// ===================================================

// Windows.h の min/max マクロは MyMath などと衝突するため必ず無効化する。
// プロジェクト設定の NOMINMAX と同一の定義なので、再定義警告は出ない
#ifndef NOMINMAX
#define NOMINMAX
#endif

// --- 標準ライブラリ ---
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <numbers>
#include <random>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// --- Windows / DirectX ---
#include <Windows.h>
#include <d3d12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <wrl.h>

// --- 外部ライブラリ ---
#include <DirectXTex/DirectXTex.h>
#include <imgui.h>
#include <nlohmann/json.hpp>
