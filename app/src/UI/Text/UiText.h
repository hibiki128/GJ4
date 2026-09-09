#pragma once
#include <Sprite.h>
#include <memory>
#include <string>
#include <type/Vector2.h>
#include <type/Vector4.h>
#include <vector>

/// <summary>
/// ポーズ画面などのコード側UI部品。
///
/// スプライトは SpriteManager に登録せず自前で持つ。
/// 登録してしまうと「全シーン共通のスプライトデータ」に手続き生成のUIが混ざり、
/// シーン保存のたびに書き出されてしまうため。
/// 描画は DrawSystem の UI レイヤーから Draw() を呼ぶ。
/// </summary>
namespace GameUi {

/// <summary>
/// 使用するフォント。エンジンが起動時にサイズ60でロード済み（Framework::LoadResource）
/// </summary>
inline constexpr const char *kFontFile = "Buildingsandundertherailwaytracksfree_ver.otf";
inline constexpr float kFontSize = 60.0f;

/// <summary>
/// このUIで使うフォントキーを返す
/// </summary>
std::string FontKey();

/// <summary>
/// スプライト1枚分の最小の描画部品。位置はアンカー基準で、既定は中心アンカー。
/// 1フレームに1回しか描けない点に注意（Sprite は Draw のたびに自分の定数バッファを
/// 書き換えるので、同じスプライトを2回描くと後から書いた内容で2回描かれる）。
/// </summary>
class UiSprite
{
public:
    /// <summary>
    /// テクスチャを指定して生成する
    /// </summary>
    /// <param name="texturePath">images ルートからの相対パス</param>
    /// <param name="anchor">アンカーポイント（既定は中心）</param>
    void Initialize(const std::string &texturePath, const Hagine::Vector2 &anchor = {0.5f, 0.5f});

    /// <summary>
    /// 位置・大きさ・色を指定して1枚描く
    /// </summary>
    void Draw(const Hagine::Vector2 &position, const Hagine::Vector2 &size, const Hagine::Vector4 &color);

    /// <summary>
    /// 生成済みか
    /// </summary>
    bool IsReady() const { return sprite_ != nullptr; }

    /// <summary>
    /// 抱えているスプライトを解放する（アプリの終了処理から呼ぶ）。
    /// スプライト1枚につきGPUのバッファを4本持っているので、
    /// 手放さないと終了時のリークチェックまで残り続ける
    /// </summary>
    void Finalize() { sprite_.reset(); }

    /// <summary>
    /// テクスチャ本来の大きさ（ピクセル）
    /// </summary>
    const Hagine::Vector2 &GetBaseSize() const { return baseSize_; }

protected:
    std::unique_ptr<Hagine::Sprite> sprite_;  // 自前で持つスプライト
    Hagine::Vector2 baseSize_ = {0.0f, 0.0f}; // テクスチャ本来の大きさ
};

/// <summary>
/// 同じテクスチャを1フレームに何枚も描くためのスプライトの置き場。
///
/// Sprite は描画のたびに自分の定数バッファを書き換えるので、1枚のスプライトを
/// 1フレームに2回描くと「最後に書いた内容が2回描かれる」ことになる。
/// 板やゲージのように同じ絵を位置違いで並べたい場合は、必要な枚数ぶん持っておく必要がある。
/// </summary>
class UiSpritePool
{
public:
    /// <summary>
    /// テクスチャと最大枚数を指定して生成する
    /// </summary>
    /// <param name="texturePath">images ルートからの相対パス</param>
    /// <param name="capacity">1フレームに描ける最大枚数</param>
    void Initialize(const std::string &texturePath, int capacity);

    /// <summary>
    /// 使用位置を先頭へ戻す。毎フレーム描き始めに呼ぶ
    /// </summary>
    void BeginFrame() { used_ = 0; }

    /// <summary>
    /// このフレックスでまだ使っていないスプライトを1枚借りる
    /// </summary>
    /// <returns>Sprite*: 借りたスプライト。枚数が尽きたら nullptr</returns>
    Hagine::Sprite *Next();

    /// <summary>
    /// 生成済みか
    /// </summary>
    bool IsReady() const { return !sprites_.empty(); }

    /// <summary>抱えているスプライトをすべて解放する</summary>
    void Finalize()
    {
        sprites_.clear();
        used_ = 0;
    }

    /// <summary>
    /// テクスチャ本来の大きさ（ピクセル）
    /// </summary>
    const Hagine::Vector2 &GetBaseSize() const { return baseSize_; }

private:
    std::vector<std::unique_ptr<Hagine::Sprite>> sprites_; // 借り出す先
    Hagine::Vector2 baseSize_ = {0.0f, 0.0f};              // テクスチャ本来の大きさ
    size_t used_ = 0;                                      // 今フレームで借りた枚数
};

/// <summary>
/// 単色の板。暗幕・パネル・ゲージなど、四角を並べる用途をこれ1つでまかなう
/// </summary>
class UiRect
{
public:
    /// <summary>
    /// 白1x1テクスチャで生成する
    /// </summary>
    /// <param name="capacity">1フレームに描ける最大枚数</param>
    void Initialize(int capacity);

    /// <summary>
    /// 使用位置を先頭へ戻す。毎フレーム描き始めに呼ぶ
    /// </summary>
    void BeginFrame() { pool_.BeginFrame(); }

    /// <summary>抱えているスプライトをすべて解放する</summary>
    void Finalize() { pool_.Finalize(); }

    /// <summary>
    /// 中心・大きさ・色を指定して1枚描く
    /// </summary>
    void Draw(const Hagine::Vector2 &center, const Hagine::Vector2 &size, const Hagine::Vector4 &color);

private:
    UiSpritePool pool_;
};

/// <summary>
/// 文字列を1枚のテクスチャに焼いて描くラベル。
/// テクスチャ生成（PNG書き出し）は重いので、生成は初期化時に1度だけ行う。
/// </summary>
class UiText : public UiSprite
{
public:
    /// <summary>
    /// テキストからスプライトを作る
    /// </summary>
    /// <param name="id">スプライト名（生成されるPNGのファイル名にもなる）</param>
    /// <param name="text">描く文字列（フォントのベイク範囲の都合でASCIIのみ）</param>
    /// <param name="outlineThickness">アウトラインの太さ。0以下でアウトラインなし</param>
    void Create(const std::string &id, const std::string &text, float outlineThickness = 4.0f);

    /// <summary>
    /// 中央揃えで描く
    /// </summary>
    /// <param name="center">中心座標</param>
    /// <param name="height">描画したい高さ（ピクセル）。横幅は比率を保って決まる</param>
    /// <param name="color">色</param>
    void DrawCentered(const Hagine::Vector2 &center, float height, const Hagine::Vector4 &color);

    /// <summary>
    /// 左端を合わせて描く
    /// </summary>
    /// <param name="left">左端のX座標</param>
    /// <param name="centerY">中心のY座標</param>
    /// <param name="height">描画したい高さ（ピクセル）</param>
    /// <param name="color">色</param>
    void DrawLeft(float left, float centerY, float height, const Hagine::Vector4 &color);

    /// <summary>
    /// 指定した高さで描いたときの横幅を返す
    /// </summary>
    float WidthAt(float height) const;
};

/// <summary>
/// 数値表示。文字アトラス（"0123456789.%"）から1文字ずつ切り出して並べる。
/// 値が毎フレーム変わってもテクスチャを作り直さずに済む。
/// </summary>
class UiNumber
{
public:
    /// <summary>
    /// 数値表示用の文字アトラスを作る
    /// </summary>
    /// <param name="id">スプライト名（生成されるPNGのファイル名にもなる）</param>
    /// <param name="capacity">1フレームに描ける最大文字数（行数×桁数ぶん見ておく）</param>
    void Create(const std::string &id, int capacity);

    /// <summary>
    /// 使用位置を先頭へ戻す。毎フレーム描き始めに呼ぶ
    /// </summary>
    void BeginFrame() { pool_.BeginFrame(); }

    /// <summary>
    /// 文字列を右端を揃えて描く（数値は桁が変わるので右揃えの方が落ち着く）
    /// </summary>
    /// <param name="text">"85%" や "1.50" など。アトラスに無い文字は空けて描く</param>
    /// <param name="right">右端のX座標</param>
    /// <param name="centerY">中心のY座標</param>
    /// <param name="height">1文字の高さ（ピクセル）</param>
    /// <param name="color">色</param>
    void DrawRight(const std::string &text, float right, float centerY, float height, const Hagine::Vector4 &color);

    /// <summary>
    /// 生成済みか
    /// </summary>
    bool IsReady() const { return pool_.IsReady(); }

    /// <summary>抱えているスプライトをすべて解放する</summary>
    void Finalize() { pool_.Finalize(); }

private:
    // アトラスに並べる文字。ここに無い文字は描けない
    static constexpr const char *kChars = "0123456789.%";
    // 1文字あたりのセルの大きさ（フォントサイズ60に対して余裕を持たせた値）
    static constexpr int kCellWidth = 44;
    static constexpr int kCellHeight = 76;

    UiSpritePool pool_; // 1文字ぶんのスプライトの置き場
};

} // namespace GameUi
