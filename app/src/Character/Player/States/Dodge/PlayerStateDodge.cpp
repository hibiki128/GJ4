#include "PlayerStateDodge.h"
#include "src/Character/Player/Player.h"

void PlayerStateDodge::Enter(Player& player, PlayerContext& context) {
}

void PlayerStateDodge::Update(Player& player, PlayerContext& context) {
	// ここに回避中の無敵処理などをつくる

	// 回避時間分待ったらダッシュステートに行く
	// 今回はそのままダッシュになる
	player.ChangeState("Dash");
}

void PlayerStateDodge::Exit(Player& player, PlayerContext& context) {
}
