#pragma once

class Player;
class PlayerContext;

class PlayerStateBase {
public:
	virtual ~PlayerStateBase() {};
	// 調整パラメータをデバッグUIへ登録する（Player::Init から一度だけ呼ばれる）
	virtual void RegisterParams() {};
	virtual void Enter(Player&, PlayerContext&) {};
	virtual void Update(Player&, PlayerContext&) {};
	virtual void Exit(Player&, PlayerContext&) {};
};
