#pragma once
#include "type/Vector2.h"

struct PlayerInput {
	Hagine::Vector2 dir;
	bool move;
	bool jump;
	bool attack;
	bool dash;
	// 回避（押した瞬間だけ true）。ダッシュと違い、押しっぱなしでは何度も出ない
	bool dodge;
	// 選んだ色の添字（-1 なら変更なし）。ColorStruct.h の Color の並びと対応する
	int selectColorIndex = -1;
};

// カメラ（視点）操作の入力
struct CameraInput {
	// 視点を動かす量(-1〜1)。x が左右、y が上下（上に倒すと上を向く）。
	// キーボードは押している間 ±1、ゲームパッドは倒し具合がそのまま入る
	Hagine::Vector2 look;
};

class GameInput {
public:
	GameInput() = default;
	~GameInput() = default;
	// 入力状態を取得
	void UpdateInputState();
	// コンテキストを取得
	const PlayerInput& GetInputContext() const { return context_; };
	// カメラ用のコンテキストを取得
	const CameraInput& GetCameraContext() const { return cameraContext_; };
private:
	// 視点操作の入力を取り込む
	void UpdateCameraInput();
private:
	// コンテキストを保持
	PlayerInput context_{};
	// カメラ用のコンテキストを保持
	CameraInput cameraContext_{};
};
