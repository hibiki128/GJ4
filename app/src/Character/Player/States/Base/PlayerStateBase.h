#pragma once

class Player;
class PlayerContext;

class PlayerStateBase {
public:
	virtual ~PlayerStateBase() {};
	virtual void Enter(Player&, PlayerContext&) {};
	virtual void Update(Player&, PlayerContext&) {};
	virtual void Exit(Player&, PlayerContext&) {};
};

