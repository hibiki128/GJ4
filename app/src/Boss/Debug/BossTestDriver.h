#pragma once
#include "src/Boss/Spider/BossSpider.h"
#include "src/Boss/Boss.h"
#include "src/Boss/Debug/BossTestProjectile.h"
#include "src/Character/ColorStruct.h"
#include "src/Interface/IColorProvider.h"
#include <memory>
#include <vector>

namespace Hagine {
class ViewProjection;
}

/// <summary>
/// 連鎖マッチ検証用のデバッグ射撃（プレイヤーの射撃が実装されるまでの代役）。
///
/// 操作: [1][2][3][4] 色切り替え / [F] 発射
/// カメラの位置と向きを射線として使い、ソフトロックオン → 疑似弾 → 命中通知 と、
/// 本番と同じ経路（IBossTargetQuery）を通す。
/// プレイヤー側の射撃が入ったら SetEnabled(false) で止める（Release では既定で無効）。
/// IColorProvider も実装しているので、プレイヤーに色APIが生えるまでの代役にもなる。
/// </summary>
class BossTestDriver final : public IColorProvider {
public:
    /// ===================================================
    /// public method
    /// ===================================================

    /// <summary>検証対象のボスを設定する</summary>
    /// <param name="boss">対象のボス（非所有）</param>
    void Init(Boss *boss);

    /// <summary>いま撃つ相手（変形が終わっていれば蜘蛛、そうでなければ球体形態）</summary>
    IBossTargetQuery *ActiveTarget();
    /// <summary>第2形態を登録する（変形が終わったら撃つ相手をこちらへ切り替える）</summary>
    /// <param name="spider">蜘蛛形態（非所有）</param>
    void SetSpider(BossSpider *spider) { pSpider_ = spider; }


    /// <summary>入力・ロックオン・弾の更新</summary>
    /// <param name="viewProjection">射線に使うビュープロジェクション</param>
    void Update(const Hagine::ViewProjection &viewProjection);

    /// <summary>弾の描画</summary>
    /// <param name="viewProjection">ビュープロジェクション</param>
    void Draw(const Hagine::ViewProjection &viewProjection);

    /// <summary>デバッグUI</summary>
    void DrawImGui();

    /// ===================================================
    /// IColorProvider
    /// ===================================================

    Color GetSelectedColor() const override { return selectedColor_; }

    /// ===================================================
    /// getter / setter
    /// ===================================================

    void SetEnabled(bool enabled) { isEnabled_ = enabled; }
    bool IsEnabled() const { return isEnabled_; }
    void SetSelectedColor(Color color) { selectedColor_ = color; }
    const LockOnResult &GetLockOnResult() const { return lockOn_; }

private:
    /// ===================================================
    /// private method
    /// ===================================================

    /// <summary>キー入力で色を切り替える</summary>
    void UpdateColorSelection();

    /// <summary>照準方向からロックオン対象を探し、強調表示を更新する</summary>
    /// <param name="origin">射撃開始位置</param>
    /// <param name="aimDirection">照準方向</param>
    void UpdateLockOn(const Hagine::Vector3 &origin, const Hagine::Vector3 &aimDirection);

    /// <summary>弾を撃つ</summary>
    /// <param name="origin">射撃開始位置</param>
    /// <param name="aimDirection">照準方向</param>
    void FireProjectile(const Hagine::Vector3 &origin, const Hagine::Vector3 &aimDirection);

    /// <summary>使い終わった弾を再利用する（無ければ新規生成）</summary>
    /// <returns>BossTestProjectile*: 使用可能な弾（上限に達していれば nullptr）</returns>
    BossTestProjectile *AcquireProjectile();

    /// ===================================================
    /// private variables
    /// ===================================================

    Boss *pBoss_ = nullptr;         // 検証対象（非所有）
    BossSpider *pSpider_ = nullptr; // 第2形態（非所有）。変形が終わったらこちらを撃つ

    Color selectedColor_ = Color::RED; // 現在の色
    LockOnResult lockOn_{};         // 現在のロックオン結果
    BulletHitResult lastHit_{};     // 直近の着弾結果（UI表示用）

    std::vector<std::unique_ptr<BossTestProjectile>> projectiles_{}; // 弾のプール
    int projectileSerial_ = 0;      // 弾の名前に使う連番

    float fireCoolDown_ = 0.0f;     // 発射クールダウン残り（秒）
    float fireInterval_ = 0.18f;    // 発射間隔（秒）
    float projectileSpeed_ = 45.0f; // 弾速（単位/秒）
    float projectileRadius_ = 0.3f; // 弾の半径
    float projectileLife_ = 3.0f;   // 弾の寿命（秒）
    float correctionRate_ = 12.0f;  // 軌道補正の強さ

#ifdef _DEBUG
    bool isEnabled_ = true;
#else
    bool isEnabled_ = false; // Release ではプレイヤーの射撃を邪魔しないよう既定で無効
#endif
    bool drawAimLine_ = true; // 照準線・ロックオン位置を線で表示する
};
