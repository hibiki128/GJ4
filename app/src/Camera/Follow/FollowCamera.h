#pragma once
#include "camera/Camera.h"
#include "debug/param/GameParamHub.h"
#include "src/Input/GameInput.h"
#include <functional>
#include <string>
#include <transform/WorldTransform.h>
#include <vector>

namespace Hagine {
class BaseObject;
}

/// <summary>
/// カメラの状態（仕様書 §16）
/// </summary>
enum class CameraMode
{
	Normal,     // プレイヤーだけを追う通常のTPS
	BossBattle, // プレイヤーとボスを同時に画面へ収める
};

/// <summary>
/// 画面に収めたい相手（ボス）の情報（仕様書 §5 の BossCameraTarget）。
/// カメラはボスの具象クラスを知らないので、シーンから値だけを受け取る。
/// ボスの形態が入れ替わっても、シーン側が「今の相手」を返すだけで済む
/// </summary>
struct CameraFrameTarget
{
	Hagine::Vector3 position{}; // 画面に収めたい中心（ボスの中心）
	float radius = 1.0f;        // 収めたい大きさ（脚の広がりなど、見た目の外周半径）
	bool valid = false;         // 収める相手がいるか（いなければプレイヤー追従だけになる）
};

/// <summary>
/// ボスごとに変えるフレーミングの調整値（仕様書 §19）。
/// 脚が大きく広がる蜘蛛型は radius と framingOffset を大きめに、
/// 球体形態は小さめにすると、同じカメラのまま構図を作り分けられる
/// </summary>
struct BossCameraConfig
{
	// 注視点をプレイヤー側へ寄せる割合（1.0 でプレイヤーの真上、0.5 で中間点）。
	// ボス側の重みは 1 - playerWeight なので、2つの重みが食い違うことはない
	float playerWeight = 0.6f;

	// プレイヤーとボスの距離をカメラ距離へ変換する倍率（仕様書 §8）。
	// 画角60度・見下ろし15度のとき、2人とも画面に残すには
	//   distanceScale >= 1.41 * max(playerWeight, 1 - playerWeight)
	// が要る（playerWeight 0.6 なら 0.85 以上）。少し余裕を持たせてある
	float distanceScale = 1.05f;
	// ボスの大きさをどれだけカメラ距離へ足すか（仕様書 §9）
	float bossRadiusScale = 1.0f;
	// 画面の余白。大きいほど2人が中央に小さく収まる（仕様書 §8）
	float framingOffset = 3.0f;

	float minDistance = 5.0f; // これより寄らない
	// これより引かない。上の distanceScale がいくら足りていても、
	// この上限に張り付いた時点で「離れるほど引く」が止まって画面から人がはみ出す。
	// 想定する最大のプレイヤー-ボス距離に対して
	//   maxDistance >= 1.41 * max(playerWeight, 1 - playerWeight) * その距離
	// を満たす値にしておくこと（40離れる想定なら 34 以上）
	float maxDistance = 25.0f;

	// スムージングにかける時間(秒)。大きいほどカメラが重くなる（仕様書 §13）
	float targetSmoothTime = 0.15f;
	float distanceSmoothTime = 0.22f;

	// ボスの中心から注視点をどれだけずらすか（仕様書 §5：中心か少し上）
	Hagine::Vector3 bossTargetOffset = { 0.0f, 0.0f, 0.0f };
};

/// <summary>
/// POPUCOM風・ボス戦カメラ（仕様書「POPUCOM風_ボス戦カメラ実装仕様書」の実装）
///
/// 構造は 注視点(Target) + 向き(Yaw/Pitch) + 距離(Distance)。
///   Forward        = CalcForward(yaw, pitch)
///   CameraPosition = Target - Forward * Distance
///
/// Normal モードはプレイヤーだけを追う普通のTPS、
/// BossBattle モードはプレイヤーとボスの加重中間点を注視し、
/// 2人の距離とボスの大きさからカメラ距離を自動で変えて両方を画面へ収める。
///
/// カメラ本体（位置・向き・行列）は Camera が持ち、このクラスは「どこから何を見るか」だけを決める
/// </summary>
class FollowCamera
{
public:
	// ===================================================
	// 型
	// ===================================================

	/// <summary>画面に収めたい相手を教えてもらう口（シーンが配線する）</summary>
	using FrameTargetProvider = std::function<CameraFrameTarget()>;

	/// <summary>カメラ衝突で当てる相手を教えてもらう口（シーンが配線する）</summary>
	using ObstacleProvider = std::function<std::vector<Hagine::BaseObject*>()>;

	// ===================================================
	// 公開メソッド
	// ===================================================

	/// <summary>
	/// 初期化（カメラを CameraManager へ登録する）
	/// 保存済みの調整値があれば読み込む
	/// </summary>
	/// <param name="cameraName">登録するカメラ名</param>
	void Init(const std::string& cameraName = "追従カメラ");

	/// <summary>
	/// 更新処理（仕様書 §20 の順序で1フレーム分を進める）
	/// </summary>
	/// <param name="input">視点操作の入力（GameInput から受け取る）</param>
	void Update(const CameraInput& input);

	/// <summary>
	/// このカメラを描画に使うカメラにする
	/// 追従位置へ移動させてから切り替えるので、切り替えた瞬間から構図が合っている
	/// </summary>
	void Activate();

	/// <summary>
	/// ImGuiによるデバッグ表示
	/// </summary>
	void DrawImGui();

	/// <summary>
	/// 追従の調整値を保存する（Assets/jsons/FollowCamera/カメラ名.json）
	/// </summary>
	void Save();

	/// <summary>
	/// 保存済みの追従の調整値を読み込む（無ければ既定値のまま）
	/// </summary>
	void Load();

	/// <summary>
	/// カメラの状態を切り替える（仕様書 §17）。
	/// 切り替えた瞬間に構図が飛ばないよう、距離と注視点はスムージングで移る
	/// </summary>
	/// <param name="mode">切り替え先の状態</param>
	void SetMode(CameraMode mode) { mode_ = mode; }

	/// <summary>今のカメラの状態</summary>
	CameraMode GetMode() const { return mode_; }

	/// <summary>
	/// 視点を初期の向き（ターゲットの真後ろ・既定のピッチ）へ戻す
	/// </summary>
	void ResetView();

	/// <summary>
	/// スムージングの遅れを無くして、今の理想の構図へ即座に合わせる
	/// （シーン開始やカメラ切り替えなど、遅れを見せたくない場面で使う）
	/// </summary>
	void SnapToTarget();

	/// <summary>
	/// 被弾などの衝撃をカメラへ加える（後ろへ弾かれてから戻り、収まるまで小さく揺れる）。
	/// 注視点や距離そのものは動かさないので、揺れが収まればいつもの構図へ自然に戻る
	/// </summary>
	/// <param name="strength">強さの倍率（1.0 で調整値どおり）</param>
	void AddImpact(float strength = 1.0f);

	/// <summary>ヨー角(左右の向き・ラジアン)を取得</summary>
	float GetYaw() const { return yaw_; }

	/// <summary>ピッチ角(上下の向き・ラジアン。正で見下ろし)を取得</summary>
	float GetPitch() const { return pitch_; }

	/// <summary>今のカメラ距離を取得</summary>
	float GetDistance() const { return distance_; }

	/// <summary>今の注視点を取得</summary>
	const Hagine::Vector3& GetTargetPosition() const { return target_; }

	/// <summary>カメラを取得（所有は CameraManager）</summary>
	Hagine::Camera* GetCamera() const { return pCamera_; }

	/// <summary>描画へ渡すビュープロジェクションを取得</summary>
	Hagine::ViewProjection& GetViewProjection() { return pCamera_->GetViewProjection(); }

	/// <summary>追従対象（プレイヤー）を設定</summary>
	void SetTarget(const Hagine::WorldTransform* pTarget) { pTarget_ = pTarget; }

	/// <summary>
	/// 画面に収めたい相手（ボス）の取得口を設定する。
	/// 未設定なら BossBattle モードでもプレイヤー追従だけになる
	/// </summary>
	/// <param name="provider">今フレームのボスの位置と大きさを返す関数</param>
	void SetFrameTargetProvider(FrameTargetProvider provider) { frameTargetProvider_ = std::move(provider); }

	/// <summary>
	/// カメラ衝突で当てる相手（壁や地形）の取得口を設定する。
	/// 未設定ならレイキャストは行わず、地面へのめり込み防止だけが働く
	/// </summary>
	/// <param name="provider">遮蔽物になるオブジェクトを返す関数</param>
	void SetObstacleProvider(ObstacleProvider provider) { obstacleProvider_ = std::move(provider); }

private:
	// ===================================================
	// 非公開メソッド
	// ===================================================

	/// <summary>プレイヤーの注視点を求める（仕様書 §5：足元ではなく胸〜頭）</summary>
	Hagine::Vector3 CalcPlayerTarget() const;

	/// <summary>今フレームの「収めたい相手」を取得する（未配線なら無効な相手が返る）</summary>
	CameraFrameTarget CalcFrameTarget() const;

	/// <summary>
	/// 理想の注視点とカメラ距離を求める（仕様書 §6〜§9）
	/// </summary>
	/// <param name="playerTarget">プレイヤーの注視点</param>
	/// <param name="frame">画面に収めたい相手</param>
	/// <param name="outTarget">理想の注視点</param>
	/// <param name="outDistance">理想のカメラ距離</param>
	void CalcDesired(const Hagine::Vector3& playerTarget, const CameraFrameTarget& frame,
		Hagine::Vector3& outTarget, float& outDistance) const;

	/// <summary>
	/// 視点入力から向きを更新する（仕様書 §11・§13.3：入力はなるべく即時）
	/// </summary>
	/// <param name="input">視点操作の入力</param>
	/// <param name="deltaTime">前フレームからの経過秒</param>
	void UpdateAngle(const CameraInput& input, float deltaTime);

	/// <summary>
	/// ボスが画面から外れているときだけ、ボスの方向へゆっくり向き直す（仕様書 §14）
	/// </summary>
	/// <param name="target">今の注視点</param>
	/// <param name="frame">画面に収めたい相手</param>
	/// <param name="deltaTime">前フレームからの経過秒</param>
	void UpdateAutoAlign(const Hagine::Vector3& target, const CameraFrameTarget& frame, float deltaTime);

	/// <summary>ヨー・ピッチからカメラの前方向を求める（仕様書 §3）</summary>
	static Hagine::Vector3 CalcForward(float yaw, float pitch);

	/// <summary>
	/// 壁や地面へめり込まない位置までカメラを手前へ寄せる（仕様書 §15）
	/// </summary>
	/// <param name="target">注視点（レイの始点）</param>
	/// <param name="desiredPosition">遮蔽を考えないカメラ位置</param>
	/// <returns>Vector3: 実際に置くカメラ位置</returns>
	Hagine::Vector3 ResolveCameraCollision(const Hagine::Vector3& target,
		const Hagine::Vector3& desiredPosition) const;

	/// <summary>注視点・向き・距離からカメラの位置と注視点を決める</summary>
	void ApplyToCamera();

	/// <summary>
	/// 被弾の衝撃ぶんの、後ろへ引く量と揺れを求める（衝撃が無ければ両方 0）
	/// </summary>
	/// <param name="outPullBack">カメラ距離へ足す量</param>
	/// <param name="outShake">カメラ位置へ足すズレ</param>
	void CalcImpact(float& outPullBack, Hagine::Vector3& outShake) const;

	/// <summary>調整用のデバッグ線を出す（仕様書 §22）</summary>
	void DrawDebugLines(const Hagine::Vector3& playerTarget, const CameraFrameTarget& frame) const;

	/// <summary>
	/// 実行時に調整できるよう、パラメータを GameParamHub へ登録する。
	/// ハブに保存済みの値があれば、この登録の時点で反映される
	/// </summary>
	/// <param name="cameraName">オーナー名に使うカメラ名</param>
	void RegisterTuningParameters(const std::string& cameraName);

	/// <summary>今のモードで使うスムージング時間を返す</summary>
	float GetTargetSmoothTime() const;
	float GetDistanceSmoothTime() const;

private:
	// ===================================================
	// メンバ変数
	// ===================================================

	Hagine::Camera* pCamera_ = nullptr;               // カメラ本体（所有は CameraManager）
	const Hagine::WorldTransform* pTarget_ = nullptr; // 追従対象（プレイヤー）のワールド変換
	FrameTargetProvider frameTargetProvider_{};       // 画面に収めたい相手の取得口
	ObstacleProvider obstacleProvider_{};             // カメラ衝突で当てる相手の取得口

	CameraMode mode_ = CameraMode::Normal; // 今のカメラの状態

	// 毎フレーム動く値（スムージングの途中経過）
	float yaw_ = 0.0f;               // 今のヨー角
	float pitch_ = 0.0f;             // 今のピッチ角
	Hagine::Vector3 target_{};       // 今の注視点
	float distance_ = 7.0f;          // 今のカメラ距離
	Hagine::Vector3 targetVelocity_{}; // SmoothDamp が持ち越す注視点の速度
	float distanceVelocity_ = 0.0f;    // SmoothDamp が持ち越す距離の速度
	bool hasState_ = false;            // 一度も更新していないなら理想値へ瞬間移動させる

	// 被弾の衝撃（AddImpact で入り、時間で収まる）
	float impactTimer_ = 0.0f;    // 残り時間(秒)。0 なら衝撃は効いていない
	float impactStrength_ = 0.0f; // いま効いている強さの倍率

	// ここから下は ImGui で調整する値（Save / Load の対象）

	// --- 注視点（仕様書 §5） ---
	// プレイヤーの足元からどれだけ上を見るか。胸〜頭あたり（1.0〜1.5m）が目安
	Hagine::Vector3 playerTargetOffset_ = { 0.0f, 1.2f, 0.0f };

	// --- Normal モード（仕様書 §23 Phase 1 の普通のTPS） ---
	float normalDistance_ = 7.0f;          // プレイヤーだけを追うときのカメラ距離
	float normalTargetSmoothTime_ = 0.12f; // 注視点のスムージング時間(秒)
	float normalDistanceSmoothTime_ = 0.15f;

	// --- BossBattle モード（仕様書 §19） ---
	BossCameraConfig bossConfig_{};

	// --- 視点操作（仕様書 §11・§12） ---
	// 角度は調整しやすいように度で持ち、使うときにラジアンへ直す
	float yawSpeedDegrees_ = 140.0f;    // 左右の視点移動の速さ(度/秒)
	float pitchSpeedDegrees_ = 100.0f;  // 上下の視点移動の速さ(度/秒)
	float pitchMinDegrees_ = -20.0f;    // 見上げる限界(度)
	float pitchMaxDegrees_ = 45.0f;     // 見下ろす限界(度)
	float defaultPitchDegrees_ = 15.0f; // 初期の見下ろし角(度)
	// 視点の追いつく速さ(1/秒)。仕様書 §13.3 のとおり既定は 0（＝入力に即時追従）。
	// 手応えを重くしたいときだけ上げる
	float rotateSmoothRate_ = 0.0f;

	// --- 自動補正（仕様書 §14） ---
	bool autoAlignEnabled_ = true;
	float autoAlignDeadDegrees_ = 25.0f;  // これ以内なら画面中央付近とみなして補正しない
	float autoAlignFullDegrees_ = 55.0f;  // これを超えたら画面外とみなして最大の強さで補正する
	float autoAlignSpeedDegrees_ = 70.0f; // 補正の最大の速さ(度/秒)

	// --- カメラ衝突（仕様書 §15） ---
	bool collisionEnabled_ = true;
	float collisionRadius_ = 0.3f;   // カメラの太さ（壁からこれだけ離す）
	float collisionMargin_ = 0.15f;  // 当たった位置からさらに手前へ寄せる余裕
	float minCollisionDistance_ = 1.0f; // これ以上は寄らない（顔の中に入らないように）
	float groundHeight_ = 0.0f;      // 地面の高さ。これ＋余裕より下へはカメラを下ろさない

	// --- 被弾の衝撃（AddImpact で加わる） ---
	float impactDuration_ = 0.35f;    // 衝撃が収まりきるまでの時間(秒)
	float impactPullBack_ = 1.6f;     // 当たった瞬間に後ろへ引かれる距離
	float impactShakeAmount_ = 0.25f; // 揺れの大きさ
	float impactShakeSpeed_ = 38.0f;  // 揺れの速さ（大きいほど細かく震える）

	// --- レンズ（仕様書 §10） ---
	float fovDegrees_ = 60.0f;

	// --- デバッグ表示（仕様書 §22） ---
	bool debugDraw_ = false;

	// GameParamHub への登録用。解除はこのメンバのデストラクタが行う
	Hagine::GameParamOwner params_;
};
