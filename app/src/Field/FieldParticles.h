#pragma once

/// <summary>
/// フィールドに漂う粒（Assets/jsons/ParticleCS/Field_Particle.json）を場に出す。
///
/// テンプレート1枚をそのまま置くだけ。位置も出し方も json 側の値を使うので、
/// 見た目を変えたいときは「パーティクル設定」から触って保存すればよく、
/// このコードは触らなくてよい。
///
/// エミッターの実体は ParticleCSSpawner が持っていて、シーンを切り替えると
/// 捨てられる。出したいシーンの初期化から毎回呼ぶこと。
/// </summary>
namespace FieldParticles {

/// <summary>フィールドの粒を1つ出す（シーンの初期化から呼ぶ）</summary>
void Spawn();

} // namespace FieldParticles
