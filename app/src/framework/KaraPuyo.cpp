#include "KaraPuyo.h"
#include "src/Settings/GameSettings.h"
#include "src/Boss/Data/BossColorPalette.h"
#include "src/UI/Pause/PauseMenu.h"
#include <Frame.h>
#include <collider/ColliderTagManager.h>

using namespace Hagine;

void KaraPuyo::Initialize() {
    this->Framework::Initialize();
    Framework::LoadResource();
    Framework::PlaySounds();
    Framework::RegisterShortcutKey();

    // -----ゲーム固有の処理-----

    // 保存済みの設定（音量・コントローラー）を読み込んでエンジンへ反映する。
    // Audio と GamePad の初期化が済んだ後でないと反映先が無いので、ここで呼ぶ
    GameSettings::GetInstance()->Initialize();

    // ゲームで使うコライダーのタグを登録する。
    // ここに無いタグはシーンデータやコードから設定しても無視されるので、シーンを作る前に登録しておく
    ColliderTagManager::GetInstance()->RegisterGameTags({"player", "floor"});

    // シーン切り替えの幕（六角形で埋めるやつ）を、ゲームの色マスタと同じ色にそろえる。
    // エンジンは ColorStruct を知らないので、色はこちらから配る
    {
        BossColorPalette palette{};
        palette.LoadMaster();
        std::vector<Hagine::Vector4> transitionColors;
        transitionColors.reserve(kGameColorCount);
        for (int index = 0; index < kGameColorCount; ++index) {
            transitionColors.push_back(palette.GetRgba(BossColorPalette::FromIndex(index)));
        }
        pSceneManager_->GetSceneTransition()->SetColors(transitionColors);
    }

    // 最初のシーンを予約（シーンは REGISTER_SCENE で自己登録済み）
    pSceneManager_->NextSceneReservation("GAME");

    // -----------------------
}

void KaraPuyo::Finalize() {
    // -----ゲーム固有の処理-----

    // ポーズ画面はシングルトンなので、シーンを閉じても中身が残る。
    // 抱えているスプライト（板・数値・文字ラベル）をここで手放しておかないと、
    // 終了時のリークチェックに数百個の残存リソースとして並ぶ
    PauseMenu::GetInstance()->Finalize();

    // -----------------------

    Framework::Finalize();
}

void KaraPuyo::Update() {
    Framework::Update();

    // -----ゲーム固有の処理-----

    // 振動の鳴らしっぱなし防止と、設定変更のまとめ書き出し
    GameSettings::GetInstance()->Update(Frame::DeltaTime());
#ifdef _DEBUG
    if (imGuiManager_->GetEditorMode()) {
        pInput_->UpdateRay(*pSceneManager_->GetBaseScene()->GetViewProjection(), {imGuiManager_->GetScenePosForRay(), imGuiManager_->GetSceneSizeForRay()}, 10000.0f);
    } else {
        pInput_->UpdateRay(*pSceneManager_->GetBaseScene()->GetViewProjection(), {Vector2(0, 0), Vector2(winApp_->GetClientWidth(), winApp_->GetClientHeight())}, 10000.0f);
    }

    imGuiManager_->Begin();
    pImGuizmoManager_->BeginFrame();
    pImGuizmoManager_->SetViewProjection(pSceneManager_->GetBaseScene()->GetViewProjection());
    imGuiManager_->UpdateIni();
    imGuiManager_->SetCurrentScene(pSceneManager_->GetBaseScene());
    imGuiManager_->ShowMainMenu();
    if (imGuiManager_->GetIsShowMainUI()) {
        imGuiManager_->ShowDockSpace();
        imGuiManager_->ShowSceneWindow(offscreen_.get(), pSceneManager_->GetCurrentSceneName());
    }
    imGuiManager_->ShowMainUI(offscreen_.get());

    imGuiManager_->End();
#endif // _DEBUG

#ifndef _DEBUG
    pInput_->UpdateRay(*pSceneManager_->GetBaseScene()->GetViewProjection(), {Vector2(0, 0), Vector2(winApp_->GetClientWidth(), winApp_->GetClientHeight())}, 10000.0f);
#endif // _DEBUG

    // -----------------------
}

void KaraPuyo::Draw() {
    pDrawSystem_->Draw(*pSceneManager_->GetBaseScene()->GetViewProjection());

#ifdef _DEBUG
    imGuiManager_->Draw();
#endif // _DEBUG

    pDxCommon_->PostDraw();
#ifdef _DEBUG
    // メインの Present 後に、独立OSウィンドウへ切り離したImGuiウィンドウを描画する
    imGuiManager_->RenderMultiViewport();
#endif // _DEBUG
}
