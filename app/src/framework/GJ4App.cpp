#include "GJ4App.h"
#include "src/Settings/GameSettings.h"
#include <Frame.h>
#include <collider/ColliderTagManager.h>

using namespace Hagine;

void GJ4App::Initialize() {
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

    // 最初のシーンを予約（シーンは REGISTER_SCENE で自己登録済み）
    pSceneManager_->NextSceneReservation("GAME");

    // -----------------------
}

void GJ4App::Finalize() {
    // -----ゲーム固有の処理-----

    // -----------------------

    Framework::Finalize();
}

void GJ4App::Update() {
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

void GJ4App::Draw() {
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
