#pragma once
#include "type/Vector3.h"

/// <summary>
/// 射撃が「このフレームの狙い」をどう決めたかの報告。
///
/// レティクルの表示だけのために作ってあるが、中身は射撃が実際に使った値そのもので、
/// 表示用に作り直した値は1つも入っていない。
/// 仕様書 17.1「UIと射撃処理を直接結び付けない」の通り、決めるのは射撃、
/// 表示側はこの報告を読むだけで、逆にここへ書き戻すことはしない。
/// </summary>
struct PlayerAimReport {
	/// <summary>
	/// 画面中央の射線が指す点（ワールド）。カメラの射線上にあるので、
	/// 投影すると必ず画面中央に来る＝照準レティクルの指す先
	/// </summary>
	Hagine::Vector3 aimPoint{};

	/// <summary>照準の点が相手に当たって決まったか（false なら射程の端）</summary>
	bool aimPointHit = false;

	/// <summary>
	/// 弾が実際に最初に当たる点（ワールド）。
	/// カメラではなくマズル（プレイヤーの位置）から、発射と同じ向きへ射線を飛ばして求める。
	/// カメラとマズルの位置が違うぶん、照準の点とは別の場所になることがある
	/// </summary>
	Hagine::Vector3 firePoint{};

	/// <summary>発射の点が相手に当たって決まったか（false なら何にも当たらない）</summary>
	bool firePointHit = false;
};
