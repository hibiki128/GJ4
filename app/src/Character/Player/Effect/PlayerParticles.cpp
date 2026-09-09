#include "PlayerParticles.h"
#include "Particle/gpu/ParticleCSEmitter.h"
#include "Particle/gpu/ParticleCSSpawner.h"
#include "Utility/debug/log/Logger.h"
#include <algorithm>
#include <cmath>
#include <string>
#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

using namespace Hagine;

namespace {

	/// <summary>効果の作り方の表</summary>
	struct EffectDesc {
		PlayerParticles::Id id;
		const char* templateName; // Assets/jsons/ParticleCS/<これ>.json
		const char* label;        // 調整UIでの表示名
		int instanceCount;        // 続けて出すときに何回ぶん重ねられるか
	};

	// 歩きは出しっぱなしなので1体でよい。回避と着地は続けて出ることがあるので2体持つ
	constexpr EffectDesc kEffectDescs[] = {
		{PlayerParticles::Id::DodgeJelly, "Player_DodgeJelly", "回避: ゼリー飛沫", 2},
		{PlayerParticles::Id::Walk, "Player_Walk", "移動: 足元の粒", 1},
		{PlayerParticles::Id::Landing, "Player_LandBurst", "着地: 跳ね上がる粒", 2},
		{PlayerParticles::Id::PerfectRipple, "Player_PerfectRipple", "ジャスト回避: 波紋", 2},
		{PlayerParticles::Id::PerfectBurst, "Player_PerfectBurst", "ジャスト回避: ゼリー粒", 2},
	};

	// --- 足元の高さ（粒を地面すれすれから出すため） ---
	constexpr float kFootOffset = 0.5f;

	// --- 回避の飛沫の飛ばし方（向きが絡むので json ではなくここで持つ） ---
	constexpr float kBehindOffset = 0.45f; // プレイヤーからどれだけ後ろで弾けさせるか
	constexpr float kJellyHeight = 0.25f;  // 地面へ埋まらないよう少し上げる
	constexpr float kBackSpeed = 4.0f;     // 後ろへ飛ぶ速さ
	constexpr float kSpreadSpeed = 1.8f;   // 左右と前後への散らばり
	constexpr float kUpSpeedMin = 0.4f;    // 上へ飛ぶ速さ（最小）
	constexpr float kUpSpeedMax = 2.4f;    // 上へ飛ぶ速さ（最大）
	constexpr float kJellyAlpha = 0.65f;   // 出た瞬間の濃さ（半透明にしてゼリーらしく）

	// --- 着地の粒の飛ばし方（強さで変わるので json ではなくここで持つ） ---
	constexpr float kLandUpSpeedMin = 2.0f; // 上へ飛ぶ速さ（そっと着地したとき）
	constexpr float kLandUpSpeedMax = 7.0f; // 上へ飛ぶ速さ（高いところから落ちたとき）
	constexpr float kLandSideSpeed = 2.2f;  // 横へ広がる速さ
	constexpr float kLandAlpha = 0.7f;      // 出た瞬間の濃さ

	// --- 移動の粒の濃さ（色だけ体に合わせて、濃さはテンプレートと同じ出方にする） ---
	constexpr float kWalkAlpha = 1.0f;

	// --- ジャスト回避の粒（体の色に合わせるので濃さだけここで持つ） ---
	constexpr float kRippleAlpha = 0.7f;      // 波紋が出た瞬間の濃さ
	constexpr float kPerfectBurstAlpha = 0.8f; // 飛沫が出た瞬間の濃さ
	constexpr float kPerfectHeight = 0.1f;     // 体の中心から少し上で弾けさせる

	/// <summary>テンプレートから1体出して、プレイヤー用の使い方に合わせる</summary>
	/// <param name="templateName">テンプレート名</param>
	/// <returns>ParticleCSEmitter*: 出せなければ nullptr</returns>
	ParticleCSEmitter* SpawnOne(const char* templateName) {
		ParticleCSEmitter* emitter = ParticleCSSpawner::GetInstance()->Spawn(templateName);
		if (!emitter) {
			return nullptr;
		}
		emitter->SetAuto(false);    // 出したいときにこちらから出す（移動の粒は Update が入れ直す）
		emitter->SetVisible(false); // 発生範囲のワイヤーはゲーム画面に要らない
		// Spawn 直後は "<名前>_1" のような複製名になっている。
		// このままだと調整UIの保存が別ファイルへ行ってしまうので、テンプレート名へ戻す
		emitter->SetName(templateName);
		return emitter;
	}

} // namespace

PlayerParticles* PlayerParticles::GetInstance() {
	static PlayerParticles instance;
	return &instance;
}

void PlayerParticles::Init() {
	for (const EffectDesc& desc : kEffectDescs) {
		Effect& effect = Get(desc.id);
		effect.templateName = desc.templateName;
		effect.label = desc.label;

		// 覚えているポインタは無条件に捨てて、必ず出し直す。
		//
		// シーンを切り替えると ParticleCSSpawner が実体を捨てるので、ここへ来る時点で
		// 前のシーンのポインタはすべて無効になっている。
		// 「生きているか」で選り分けてはいけない。IsAlive はアドレスの一致で見るので、
		// 解放された跡地に別の効果のエミッターが確保されると「生きている」と誤判定し、
		// よその効果のエミッターを掴んだまま使い続けてしまう
		// （BossParticles が先に出し直すため、2周目以降そこと取り違える）
		effect.emitters.clear();

		while (static_cast<int>(effect.emitters.size()) < desc.instanceCount) {
			ParticleCSEmitter* emitter = SpawnOne(desc.templateName);
			if (!emitter) {
				// json が無い。その効果が出ないだけで、他の効果もゲームも止まらない
				Logger::Warn(std::string("PlayerParticles: ") + desc.templateName +
					".json を読めなかったので「" + desc.label + "」は出ない");
				break;
			}
			effect.emitters.push_back(emitter);
		}
		effect.next = 0;
	}

	walkRequested_ = false;
	walkEmitting_ = false;

	std::string ready = "PlayerParticles: エミッターを用意した";
	for (const EffectDesc& desc : kEffectDescs) {
		ready += std::string(" ") + desc.label + "=" + std::to_string(Get(desc.id).emitters.size());
	}
	Logger::Info(ready);
}

void PlayerParticles::Update() {
	Effect& effect = Get(Id::Walk);
	ParticleCSEmitter* walk = nullptr;
	if (!effect.emitters.empty() && ParticleCSSpawner::GetInstance()->IsAlive(effect.emitters[0])) {
		walk = effect.emitters[0];
	}

	if (walk) {
		if (walkRequested_) {
			walk->SetTranslate(walkPosition_);
			// 体の色に合わせる。出したあとの粒は色を変えないので、
			// 色を切り替えた瞬間から新しく出る粒だけが新しい色になる
			walk->SetStartColor(Vector4{walkColor_.x, walkColor_.y, walkColor_.z, kWalkAlpha});
			walk->SetEndColor(Vector4{walkColor_.x, walkColor_.y, walkColor_.z, 0.0f});
		}
		// 発生の入り切りは変わったときだけ触る。
		// 要求が来なくなれば止まるので、ステート側は動いている間ずっと要求を出すだけでよい
		if (walkRequested_ != walkEmitting_) {
			walk->SetAuto(walkRequested_);
			walkEmitting_ = walkRequested_;
			if (walkRequested_) {
				++effect.burstCount;
				LogFirstBurst(Id::Walk, walk);
			}
		}
	}

	// 要求は1フレーム限り
	walkRequested_ = false;
}

void PlayerParticles::RequestWalk(const Vector3& playerPosition, const Vector4& bodyColor) {
	walkRequested_ = true;
	walkPosition_ = Vector3{playerPosition.x, playerPosition.y - kFootOffset, playerPosition.z};
	walkColor_ = bodyColor;
}

void PlayerParticles::BurstDodgeJelly(const Vector3& position, const Vector3& dodgeDirection, const Vector4& bodyColor) {
	ParticleCSEmitter* emitter = NextEmitter(Id::DodgeJelly);
	if (!emitter) {
		return;
	}

	// 体の後ろで弾けさせる。飛び出した向きが取れないときは足元でそのまま散らす
	Vector3 back = {-dodgeDirection.x, 0.0f, -dodgeDirection.z};
	if (back.LengthSq() > 0.0001f) {
		back = back.Normalize();
	} else {
		back = Vector3{0.0f, 0.0f, 0.0f};
	}

	emitter->SetTranslate(position + back * kBehindOffset + Vector3{0.0f, kJellyHeight, 0.0f});

	// 速度は「後ろ向きの速さ」を中心に、前後左右へ散らばりぶんの幅を持たせる。
	// 向きは入力で決まるので、json ではなく毎回ここで入れ直す
	const Vector3 center = back * kBackSpeed;
	emitter->SetMinVelocity(Vector3{center.x - kSpreadSpeed, kUpSpeedMin, center.z - kSpreadSpeed});
	emitter->SetMaxVelocity(Vector3{center.x + kSpreadSpeed, kUpSpeedMax, center.z + kSpreadSpeed});

	// 本体と同系色にして、消えるまでに透けきるようにする
	emitter->SetStartColor(Vector4{bodyColor.x, bodyColor.y, bodyColor.z, kJellyAlpha});
	emitter->SetEndColor(Vector4{bodyColor.x, bodyColor.y, bodyColor.z, 0.0f});

	emitter->EmitOnce();
	++Get(Id::DodgeJelly).burstCount;
	LogFirstBurst(Id::DodgeJelly, emitter);
}

void PlayerParticles::BurstLanding(const Vector3& position, float strength01, const Vector4& bodyColor) {
	ParticleCSEmitter* emitter = NextEmitter(Id::Landing);
	if (!emitter) {
		return;
	}

	// 足元で弾けさせる
	emitter->SetTranslate(Vector3{position.x, position.y - kFootOffset, position.z});

	// 上向きに飛び散らせる。高いところから落ちたときほど高く跳ねる
	const float strength = std::clamp(strength01, 0.0f, 1.0f);
	const float upSpeed = kLandUpSpeedMin + (kLandUpSpeedMax - kLandUpSpeedMin) * strength;
	emitter->SetMinVelocity(Vector3{-kLandSideSpeed, upSpeed * 0.4f, -kLandSideSpeed});
	emitter->SetMaxVelocity(Vector3{kLandSideSpeed, upSpeed, kLandSideSpeed});

	emitter->SetStartColor(Vector4{bodyColor.x, bodyColor.y, bodyColor.z, kLandAlpha});
	emitter->SetEndColor(Vector4{bodyColor.x, bodyColor.y, bodyColor.z, 0.0f});

	emitter->EmitOnce();
	++Get(Id::Landing).burstCount;
	LogFirstBurst(Id::Landing, emitter);
}

void PlayerParticles::BurstPerfectDodge(const Vector3& position, const Vector4& bodyColor) {
	const Vector3 center = position + Vector3{0.0f, kPerfectHeight, 0.0f};

	// 波紋。広がり方と消え方はテンプレートに任せて、ここは位置と色だけ渡す
	if (ParticleCSEmitter* ripple = NextEmitter(Id::PerfectRipple)) {
		ripple->SetTranslate(center);
		ripple->SetStartColor(Vector4{bodyColor.x, bodyColor.y, bodyColor.z, kRippleAlpha});
		ripple->SetEndColor(Vector4{bodyColor.x, bodyColor.y, bodyColor.z, 0.0f});
		ripple->EmitOnce();
		++Get(Id::PerfectRipple).burstCount;
		LogFirstBurst(Id::PerfectRipple, ripple);
	}

	// 飛沫。向きは決めず、テンプレートの速度のまま外へ散らす
	if (ParticleCSEmitter* burst = NextEmitter(Id::PerfectBurst)) {
		burst->SetTranslate(center);
		burst->SetStartColor(Vector4{bodyColor.x, bodyColor.y, bodyColor.z, kPerfectBurstAlpha});
		burst->SetEndColor(Vector4{bodyColor.x, bodyColor.y, bodyColor.z, 0.0f});
		burst->EmitOnce();
		++Get(Id::PerfectBurst).burstCount;
		LogFirstBurst(Id::PerfectBurst, burst);
	}
}

ParticleCSEmitter* PlayerParticles::NextEmitter(Id id) {
	Effect& effect = Get(id);
	if (effect.emitters.empty()) {
		return nullptr;
	}

	ParticleCSEmitter* emitter = effect.emitters[effect.next];
	effect.next = (effect.next + 1) % effect.emitters.size();
	if (!ParticleCSSpawner::GetInstance()->IsAlive(emitter)) {
		return nullptr;
	}
	return emitter;
}

void PlayerParticles::BurstTemplateOnly(Id id, const Vector3& position) {
	ParticleCSEmitter* emitter = NextEmitter(id);
	if (!emitter) {
		return;
	}

	// 位置だけ渡して、向きも色も触らずに出す
	emitter->SetTranslate(position);
	emitter->EmitOnce();
	++Get(id).burstCount;
	LogFirstBurst(id, emitter);
}

void PlayerParticles::LogFirstBurst(Id id, const ParticleCSEmitter* emitter) const {
	const Effect& effect = effects_[static_cast<size_t>(id)];
	if (effect.burstCount != 1 || !emitter) {
		return;
	}

	const Vector3 at = emitter->GetTranslate();
	Logger::Info(std::string("PlayerParticles: 「") + effect.label + "」を初めて出した 位置(" +
		std::to_string(at.x) + ", " + std::to_string(at.y) + ", " + std::to_string(at.z) + ")");
}

void PlayerParticles::DrawImGui() {
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("プレイヤーの粒##PlayerParticles")) {
		return;
	}
	ImGui::Indent();
	ImGui::TextWrapped("見た目を触って「GPU設定を保存」を押すと Assets/jsons/ParticleCS/<名前>.json に書き戻る。"
		"ただし飛ぶ向きと色は出すたびにコードから入れ直すので、ここで変えても残らない。");

	ImGui::SeparatorText("試し撃ちの共通設定");
	ImGui::DragFloat3("出す位置", &testPosition_.x, 0.1f);
	ImGui::SliderAngle("飛び出す向き", &testYaw_);
	ImGui::ColorEdit4("体の色", &testColor_.x);

	for (size_t index = 0; index < effects_.size(); ++index) {
		Effect& effect = effects_[index];
		const Id id = static_cast<Id>(index);

		ImGui::PushID(static_cast<int>(index));
		if (effect.emitters.empty()) {
			ImGui::TextColored(ImVec4{1.0f, 0.4f, 0.4f, 1.0f},
				"%s: 出せていない（%s.json と ParticleCSGroup 側を確認）", effect.label, effect.templateName);
			ImGui::PopID();
			continue;
		}

		if (ImGui::TreeNode(effect.label)) {
			// 出していないのか、出ているのに見えないのかを切り分けるための表示。
			// 「生きている粒」が増えていれば、少なくとも発生はできている（1〜2フレーム遅れて反映）
			size_t alive = 0;
			for (ParticleCSEmitter* emitter : effect.emitters) {
				alive += emitter->GetTotalAliveParticles();
			}
			ImGui::Text("出した回数 %d / 生きている粒 %zu / エミッター %zu 体", effect.burstCount, alive,
				effect.emitters.size());
			const Vector3 at = effect.emitters[0]->GetTranslate();
			ImGui::Text("いまの位置 (%.2f, %.2f, %.2f)", at.x, at.y, at.z);

			if (ImGui::Button("ゲームと同じ出し方で出す")) {
				switch (id) {
				case Id::DodgeJelly:
					BurstDodgeJelly(testPosition_, Vector3{std::sinf(testYaw_), 0.0f, std::cosf(testYaw_)}, testColor_);
					break;
				case Id::Landing:
					BurstLanding(testPosition_, 1.0f, testColor_);
					break;
				case Id::Walk:
					RequestWalk(testPosition_, testColor_);
					break;
				case Id::PerfectRipple:
				case Id::PerfectBurst:
					// ジャスト回避は波紋と飛沫がひと組なので、どちらのボタンからも両方出す
					BurstPerfectDodge(testPosition_, testColor_);
					break;
				default:
					break;
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("テンプレートのまま出す")) {
				BurstTemplateOnly(id, testPosition_);
			}
			if (id == Id::Walk) {
				ImGui::TextDisabled("移動の粒はボタンを押している間だけ出る（要求が止まると自然に止まるため）");
			}

			effect.emitters[0]->DrawImGui();
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	ImGui::Unindent();
#endif // USE_IMGUI
}
