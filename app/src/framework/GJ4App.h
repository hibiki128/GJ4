#pragma once

#include <Framework.h>
#include <memory>

/// <summary>
/// アプリケーション本体
/// </summary>
class GJ4App : public Hagine::Framework
{
public: // メンバ関数
    /// <summary>
    /// 初期化
    /// </summary>
    void Initialize() override;

    /// <summary>
    /// 終了
    /// </summary>
    void Finalize() override;

    /// <summary>
    /// 更新
    /// </summary>
    void Update() override;

    /// <summary>
    /// 描画
    /// </summary>
    void Draw() override;
};
