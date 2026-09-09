#include "GameOverBossActor.h"
#include "MyMath.h"
#include "frame/Frame.h"
#include "object/base/BaseObjectManager.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

using namespace Hagine;

namespace {

constexpr float kToRadian = std::numbers::pi_v<float> / 180.0f;

} // namespace

void GameOverBossActor::Init(BossFormId form, const Vector3 &groundPosition,
                             const Vector3 &lookTarget, BaseObjectManager *objectManager) {
    groundPosition_ = groundPosition;
    lookTarget_ = lookTarget;

    // --- 第1形態（殻をまとった球体）---
    sphere_ = std::make_unique<Boss>();
    sphere_->Init("GameOverBoss");

    // 生成直後は登場演出の始まりで、殻の球は周囲へ大きく散らされている。
    // そのまま止めると散らばったままになって殻が無いように見えるので、
    // 演出の最終状態（定位置・本来の大きさ）へそろえてから止める
    sphere_->EndAppear();

    // 状態遷移と攻撃を止める。止めているあいだも殻の見た目は作り直される
    sphere_->SetPaused(true);

    // --- 第2形態（蜘蛛）---
    spider_ = std::make_unique<BossSpider>();
    spider_->Init("GameOverBossSpider");

    // 相手（倒れたプレイヤー）は歩く先としては渡さない。渡すと詰め寄って構図が崩れる。
    // 攻撃も止めるので、この蜘蛛は跳ねも撃ちもしない
    spider_->SetAttackEnabled(false);

    if (objectManager) {
        objectManager->RegisterExternal(sphere_.get());
        objectManager->RegisterExternal(spider_.get());
    }

    // 出さないほうは隠すだけにする。実行中に作り直すと、前フレームのGPUコマンドが
    // 参照しているリソースを解放してしまう
    SetForm(form);
}

void GameOverBossActor::RegisterParams() {
    sphereParams_.Register("Hover", &sphereHover_, {0.01f, 0.0f, 5.0f});
    sphereParams_.Register("HoverPeriod", &sphereHoverPeriod_, {0.05f, 0.2f, 20.0f});
    sphereParams_.Register("SpinSpeed", &sphereSpinSpeed_, {0.5f, -180.0f, 180.0f});
    sphereParams_.Register("Lift", &sphereLift_, {0.05f, -5.0f, 30.0f});
    sphereParams_.Register("ShellExpandMin", &shellExpandMin_, {0.01f, 0.2f, 4.0f});
    sphereParams_.Register("ShellExpandMax", &shellExpandMax_, {0.01f, 0.2f, 4.0f});
    sphereParams_.Register("ShellWavePeriod", &shellWavePeriod_, {0.05f, 0.2f, 20.0f});
    sphereParams_.Register("ShellWaveCount", &shellWaveCount_, {0.05f, 0.1f, 6.0f});

    spiderParams_.Register("StepSwayRadius", &stepSwayRadius_, {0.05f, 0.0f, 6.0f});
    spiderParams_.Register("StepSwayPeriod", &stepSwayPeriod_, {0.1f, 0.5f, 30.0f});
    spiderParams_.Register("StepTriggerRatio", &stepTriggerRatio_, {0.01f, 0.05f, 1.0f});
    spiderParams_.Register("LegStanceBend", &legStanceBend_, {0.01f, 0.0f, 1.0f});
    // 出す絵に合わせて正面を向かせるための回し量。触るとその場で足も置き直す
    GameParamHub::Options facingOptions{};
    facingOptions.speed = 0.5f;
    facingOptions.min = -180.0f;
    facingOptions.max = 180.0f;
    facingOptions.onChange = [this] { FaceSpider(); };
    spiderParams_.Register("FacingDeg", &facingDegrees_, facingOptions);
}

void GameOverBossActor::SetForm(BossFormId form) {
    form_ = form;

    if (sphere_) {
        // 描画も殻のコンピュートも、この1つで止まる
        sphere_->SetFormVisible(form_ == BossFormId::Sphere);
        if (form_ != BossFormId::Sphere) {
            // 隠すときは殻を本来の位置へ戻す。次に出したとき、
            // 縮みきった形のまま現れないようにする
            sphere_->SetShellExpansion(1.0f);
        }
    }

    if (!spider_) {
        return;
    }
    if (form_ == BossFormId::Spider) {
        StandSpider();
    } else {
        spider_->Hide();
    }
}

void GameOverBossActor::StandSpider() {
    // 変形の演出は見せず、いきなり立っている状態から始める。
    // Awaken が足の置き場所を決め、SkipTransform がそこへ立たせる
    spider_->Awaken(groundPosition_, 0.0f);
    spider_->SkipTransform();
    // 向き直ってから足を置き直す。逆にすると脚だけ元の向きに取り残される
    FaceSpider();

    // 立ち姿は最後まで変えない。足踏み以外の動きは出さない
    spider_->SetLegBend(legStanceBend_, 0.01f);
    spider_->SetLegTuck(0.0f, 0.01f);

    ApplyStepTrigger();
}

void GameOverBossActor::ApplyStepTrigger() {
    if (!spider_) {
        return;
    }

    // ゲーム中の敷居は「歩いて動き回る蜘蛛」に合わせた広さで、揺れ幅よりずっと大きい。
    // そのままだと足が定位置から離れきらず、踏み替えが一度も起きない＝脚が動かない。
    // ここは歩かないので、揺れ幅から敷居を決め直す
    spider_->GetParameters().stepTrigger =
        (std::max)(0.05f, stepSwayRadius_ * stepTriggerRatio_);
}

void GameOverBossActor::FaceSpider() {
    if (!spider_) {
        return;
    }

    // まず倒れた相手のほうを向いてから、調整ぶんだけ回す。
    // 「相手を向く」だけだと構図によって背中や横しか見えないので、
    // ここで正面が見える角度まで回せるようにしてある
    spider_->FaceTowards(lookTarget_);
    spider_->AddBodyYaw(facingDegrees_ * kToRadian);
    // 向きを変えたら足も置き直す。しないと脚だけ元の向きに取り残される
    spider_->ReplantFeet();

    appliedFacingDegrees_ = facingDegrees_;
}

void GameOverBossActor::SetGroundPosition(const Vector3 &position) {
    groundPosition_ = position;

    // 蜘蛛は足が地面に貼り付いたままなので、置き場所を動かしたら足も連れていく。
    // そうしないと、脚だけ元の位置へ伸びたまま胴だけが移る
    if (spider_) {
        Vector3 bodyPosition = spider_->GetBodyPosition();
        bodyPosition.x = groundPosition_.x;
        bodyPosition.z = groundPosition_.z;
        spider_->SetBodyPosition(bodyPosition);
        spider_->ReplantFeet();
    }
}

void GameOverBossActor::SetLookTarget(const Vector3 &position) {
    lookTarget_ = position;
    FaceSpider();
}

void GameOverBossActor::Update() {
    const float deltaTime = Frame::DeltaTime();
    motionTime_ += deltaTime;

    if (form_ == BossFormId::Spider) {
        UpdateSpiderForm(deltaTime);
    } else {
        UpdateSphereForm(deltaTime);
    }
}

void GameOverBossActor::UpdateSphereForm(float deltaTime) {
    (void)deltaTime;
    if (!sphere_) {
        return;
    }

    // 止めてあるので、位置と向きはここが決めきってよい。
    // ゆっくり浮き沈みしながら回り続けると、勝ち残ったものが息をしているように見える
    const float hoverPeriod = (std::max)(0.2f, sphereHoverPeriod_);
    const float hover = std::sin(motionTime_ * 2.0f * std::numbers::pi_v<float> / hoverPeriod) * sphereHover_;
    const float bodyRadius = sphere_->GetBodyRadius();

    WorldTransform *transform = sphere_->GetWorldTransform();
    transform->translation_ =
        groundPosition_ + Vector3{0.0f, bodyRadius + sphereLift_ + hover, 0.0f};

    // 軸を少し傾けて回す（真上を軸にすると極のパーツが永久に見えないため）
    const Vector3 axis = Vector3{0.25f, 1.0f, 0.15f}.Normalize();
    const Quaternion spin =
        Quaternion::FromAxisAngle(axis, motionTime_ * sphereSpinSpeed_ * kToRadian);
    transform->quaternionRotation_ = spin;

    // 殻の膨らみを上から下へ流す。広がったところは同色の融合が切れて殻がばらけ、
    // 縮んだところはまた1つの塊に戻るので、波が通り抜けていくように見える。
    //
    // 殻はボスのローカル空間に並んでいるので、そのまま上を向きに使うと
    // 回転に合わせて波の向きまで回ってしまう。回転の逆を掛けて
    // 「いまワールドの上はローカルのどちら向きか」を渡す
    const Vector3 localUp = spin.Conjugate() * Vector3{0.0f, 1.0f, 0.0f};

    const float wavePeriod = (std::max)(0.2f, shellWavePeriod_);
    const float phase = motionTime_ * 2.0f * std::numbers::pi_v<float> / wavePeriod;
    sphere_->SetShellExpansionWave(localUp, shellExpandMin_, shellExpandMax_, phase,
                                   shellWaveCount_);
}

void GameOverBossActor::UpdateSpiderForm(float deltaTime) {
    (void)deltaTime;
    if (!spider_) {
        return;
    }

    // UIから向きを動かされていたら、足ごと向き直す
    if (std::abs(facingDegrees_ - appliedFacingDegrees_) > 0.01f) {
        FaceSpider();
    }

    // 揺れ幅は調整UIから動かせるので、敷居も毎フレーム合わせ直す。
    // 片方だけ変えられると、また踏み替えが起きない組み合わせに戻ってしまう
    ApplyStepTrigger();

    // --- その場で足踏みする ---
    // 出すのは足踏みだけ。胴を体重移動のぶんだけ揺らせば、足は地面に貼り付いたままなので、
    // 定位置から離れた脚が1本ずつ勝手に踏み替わる（BossSpider の歩容そのもの）。
    // 揺れは向いている向きを基準に取り、左右を主・前後をその半分の速さにしてある。
    // 同じ速さで揃えると8本が一斉に踏み替わって行進に見えてしまう
    const float swayPeriod = (std::max)(0.5f, stepSwayPeriod_);
    const float swayPhase = motionTime_ * 2.0f * std::numbers::pi_v<float> / swayPeriod;

    const float yaw = spider_->GetBodyYaw();
    const Vector3 forward{std::cos(yaw), 0.0f, std::sin(yaw)};
    const Vector3 right{-std::sin(yaw), 0.0f, std::cos(yaw)};
    const Vector3 sway = right * (std::sin(swayPhase) * stepSwayRadius_) +
                         forward * (std::sin(swayPhase * 0.5f) * stepSwayRadius_ * 0.45f);

    // 高さは蜘蛛自身が足の位置から決めるので、ここでは触らない（触っても上書きされる）
    Vector3 bodyPosition = spider_->GetBodyPosition();
    bodyPosition.x = groundPosition_.x + sway.x;
    bodyPosition.z = groundPosition_.z + sway.z;
    spider_->SetBodyPosition(bodyPosition);
}

void GameOverBossActor::DispatchCompute() {
    // 殻のメタボールを持っているのは第1形態だけ。
    // 隠しているときは Boss 側が中で弾く
    if (sphere_) {
        sphere_->DispatchShellCompute();
    }
}

void GameOverBossActor::DrawImGui() {
#ifdef USE_IMGUI
    if (form_ == BossFormId::Spider) {
        ImGui::TextDisabled("足踏み: 揺れ幅 %.2f ／ %.1f 秒でひと往復（踏み替えの敷居 %.2f）",
                            stepSwayRadius_, stepSwayPeriod_,
                            spider_ ? spider_->GetParameters().stepTrigger : 0.0f);
        ImGui::TextDisabled("向き: 相手のほうから %+.1f 度（FacingDeg で正面を出す）",
                            facingDegrees_);
    } else if (sphere_) {
        ImGui::TextDisabled("殻の波: %.2f 〜 %.2f 倍を上から下へ、%.1f 秒でひと巡り（波 %.1f 本）",
                            shellExpandMin_, shellExpandMax_, shellWavePeriod_, shellWaveCount_);
    }
    ImGui::TextDisabled("動きの調整は ゲームパラメータ の GameOver/Boss/%s から行う",
                        GameOverContext::GetFormName(form_));
#endif // USE_IMGUI
}
