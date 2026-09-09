#include "UiText.h"
#include <SpriteManager.h>
#include <graphics/texture/TextureManager.h>
#include <text/TextRenderer.h>
#include <string_view>

using namespace Hagine;

namespace GameUi {

namespace {

/// <summary>
/// TextRenderer で作らせたスプライトからテクスチャパスだけを受け取り、
/// SpriteManager 側の登録は外す。
///
/// TextRenderer の公開APIは「PNGを書き出して SpriteManager に登録する」までを
/// 一括で行うが、ポーズ画面のUIはシーンのスプライトデータに混ぜたくないので、
/// 生成だけ借りて登録は取り消している。
/// </summary>
/// <param name="id">スプライト名</param>
/// <returns>std::string: images ルートからの相対パス。失敗時は空文字</returns>
std::string TakeTexturePath(const std::string &id)
{
    SpriteManager *spriteManager = SpriteManager::GetInstance();
    const std::string path = spriteManager->GetTextureFilePath(id);
    spriteManager->UnregisterSprite(id);
    return path;
}

/// <summary>
/// 位置・大きさ・色をまとめて設定して描く
/// </summary>
void DrawSprite(Sprite *sprite, const Vector2 &center, const Vector2 &size, const Vector4 &color,
                float rotation = 0.0f)
{
    sprite->SetPosition(center);
    sprite->SetSize(size);
    sprite->SetRotation(rotation);
    sprite->SetColor({color.x, color.y, color.z});
    sprite->SetAlpha(color.w);
    sprite->Draw();
}

} // namespace

std::string FontKey()
{
    // 読み込み済みのものをファイル名で引く。サイズはエンジン側の
    // LoadFontTexture に書いてある値がそのまま使われる
    return TextureManager::GetInstance()->FindFontKey(kFontFile);
}

/// ===================================================
/// UiSprite
/// ===================================================

void UiSprite::Initialize(const std::string &texturePath, const Vector2 &anchor)
{
    if (texturePath.empty())
    {
        return;
    }

    sprite_ = std::make_unique<Sprite>();
    sprite_->Initialize(texturePath, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, anchor);
    // Initialize がテクスチャ本来の大きさをセットするので、そこを基準サイズとして覚えておく
    baseSize_ = sprite_->GetSize();
}

void UiSprite::Draw(const Vector2 &position, const Vector2 &size, const Vector4 &color, float rotation)
{
    if (!sprite_ || color.w <= 0.0f)
    {
        return;
    }
    DrawSprite(sprite_.get(), position, size, color, rotation);
}

/// ===================================================
/// UiSpritePool
/// ===================================================

void UiSpritePool::Initialize(const std::string &texturePath, int capacity)
{
    if (texturePath.empty() || capacity <= 0)
    {
        return;
    }

    sprites_.reserve(static_cast<size_t>(capacity));
    for (int i = 0; i < capacity; ++i)
    {
        auto sprite = std::make_unique<Sprite>();
        sprite->Initialize(texturePath, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.5f, 0.5f});
        if (i == 0)
        {
            baseSize_ = sprite->GetSize();
        }
        sprites_.push_back(std::move(sprite));
    }
    used_ = 0;
}

Sprite *UiSpritePool::Next()
{
    if (used_ >= sprites_.size())
    {
        return nullptr; // 用意した枚数を超えた分は描かない
    }
    return sprites_[used_++].get();
}

/// ===================================================
/// UiRect
/// ===================================================

void UiRect::Initialize(int capacity)
{
    pool_.Initialize("debug/white1x1.png", capacity);
}

void UiRect::Draw(const Vector2 &center, const Vector2 &size, const Vector4 &color)
{
    if (color.w <= 0.0f || size.x <= 0.0f || size.y <= 0.0f)
    {
        return;
    }

    Sprite *sprite = pool_.Next();
    if (!sprite)
    {
        return;
    }
    DrawSprite(sprite, center, size, color);
}

/// ===================================================
/// UiText
/// ===================================================

void UiText::Create(const std::string &id, const std::string &text, float outlineThickness,
                    const Vector4 &outlineColor)
{
    const bool outlineEnabled = outlineThickness > 0.0f;
    TextRenderer::GetInstance()->CreateTextSprite(
        id, text, FontKey(), {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f},
        outlineEnabled, outlineThickness, outlineColor);

    Initialize(TakeTexturePath(id));
}

void UiText::DrawCentered(const Vector2 &center, float height, const Vector4 &color)
{
    if (!sprite_ || baseSize_.y <= 0.0f)
    {
        return;
    }

    const float scale = height / baseSize_.y;
    UiSprite::Draw(center, {baseSize_.x * scale, baseSize_.y * scale}, color);
}

void UiText::DrawLeft(float left, float centerY, float height, const Vector4 &color)
{
    if (!sprite_ || baseSize_.y <= 0.0f)
    {
        return;
    }

    const float width = WidthAt(height);
    DrawCentered({left + width * 0.5f, centerY}, height, color);
}

void UiText::DrawTransformed(const Vector2 &center, const Vector2 &size, const Vector4 &color,
                             float rotation)
{
    UiSprite::Draw(center, size, color, rotation);
}

float UiText::WidthAt(float height) const
{
    if (baseSize_.y <= 0.0f)
    {
        return 0.0f;
    }
    return baseSize_.x * (height / baseSize_.y);
}

/// ===================================================
/// UiNumber
/// ===================================================

void UiNumber::Create(const std::string &id, int capacity)
{
    // 文字は読み込んだフォントサイズのピクセル数で焼かれるので、セルもそこから決める。
    // 固定値にすると、フォントを大きく読み込んだときに字がセルをはみ出て切れる
    float fontSize = 60.0f;
    if (const TextureManager::FontData *fontData =
            TextureManager::GetInstance()->GetFontData(FontKey()))
    {
        fontSize = fontData->fontSize;
    }
    // 縦は行の高さぶん、横は最も太い文字ぶんの余裕を見る。フチの太さも足しておく
    cellWidth_ = static_cast<int>(fontSize * 0.78f) + 8;
    cellHeight_ = static_cast<int>(fontSize * 1.32f) + 8;

    TextRenderer::GetInstance()->CreateCharacterAtlasSprite(
        id, kChars, FontKey(), cellWidth_, cellHeight_,
        {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f},
        true, 3.0f, {0.05f, 0.05f, 0.09f, 1.0f});

    pool_.Initialize(TakeTexturePath(id), capacity);
}

void UiNumber::DrawRight(const std::string &text, float right, float centerY, float height, const Vector4 &color)
{
    if (!pool_.IsReady() || color.w <= 0.0f)
    {
        return;
    }

    const float scale = height / static_cast<float>(cellHeight_);
    const float advance = static_cast<float>(cellWidth_) * scale;
    const std::string_view chars(kChars);
    const int count = static_cast<int>(text.size());

    for (int i = 0; i < count; ++i)
    {
        const size_t charIndex = chars.find(text[static_cast<size_t>(i)]);
        if (charIndex == std::string_view::npos)
        {
            continue; // アトラスに無い文字は詰めずに空ける
        }

        Sprite *sprite = pool_.Next();
        if (!sprite)
        {
            return;
        }

        // 何文字目のセルを使うかを左上座標で指定する
        sprite->SetTexLeftTop({static_cast<float>(charIndex) * cellWidth_, 0.0f});
        sprite->SetTexSize({static_cast<float>(cellWidth_), static_cast<float>(cellHeight_)});

        // 右端から数えた位置に置く
        const float centerX = right - advance * (static_cast<float>(count - i) - 0.5f);
        DrawSprite(sprite, {centerX, centerY}, {advance, height}, color);
    }
}

} // namespace GameUi
