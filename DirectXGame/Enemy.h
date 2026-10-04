#pragma once
#include "Field.h"
#include "KamataEngine.h"
#include "Projectile.h"
#include <array>
#include <vector>

struct GameModels;

// 雑魚敵の種類（phases.json の "type" と対応）
enum class EnemyType {
	kWalker,  // 輪に沿って歩く（接触ダメージのみ）
	kShooter, // その場でトゲ弾を撃つ
	kBomber,  // その場で爆弾を転がす
};

/// <summary>
/// 雑魚敵（紫の虫）。輪の内面に接地して動き、攻撃は弾の生成要求を積むだけ（弾のコンテナはここでは触らない）。
/// GameScene が固定数のプールで持ち、使い回す（IsInUse で使用中か分かる）。HP は1（倒し方は取り込みか、跳ね返した爆弾の爆風）。
///
/// 【状態】
/// 　kSpawning  : 出現演出（kSpawnTime 秒。その間は無害・判定なし）
/// 　kActive    : 通常（歩く・撃つ。当たり判定あり）
/// 　kCaptured  : 平らなスライムに乗っている（位置は GameScene が毎フレーム決める。攻撃しない）
/// 　kAbsorbing : スライムに吸い込まれている途中
/// </summary>
class Enemy {
public:
	enum class State {
		kInactive,
		kSpawning,
		kActive,
		kCaptured,
		kAbsorbing,
	};

	/// <summary>
	/// 初期化（プールに作ったとき1回だけ）
	/// </summary>
	void Initialize(const GameModels* models);

	/// <summary>
	/// 出現させる
	/// </summary>
	/// <param name="serial">フェーズ内の通し番号（最初の攻撃までの待ちを決定的にずらすのに使う）</param>
	void Spawn(EnemyType type, float theta, int dir, int serial, float layerZ);

	/// <summary>
	/// 1フレーム分の更新（出現・移動・攻撃）
	/// </summary>
	/// <param name="playerTheta">スライムの角度（撃つ向きを決める）</param>
	/// <param name="canAttack">攻撃してよいか（フェーズの進行中だけ）</param>
	/// <param name="requests">弾の生成要求を積む先</param>
	void Update(float playerTheta, bool canAttack, std::vector<ProjectileSpawn>* requests);

	/// <summary>
	/// 吸い込み中の更新（target：スライムの中心。戻り値：吸い込みが終わったか）
	/// </summary>
	bool UpdateAbsorb(const KamataEngine::Vector3& target);

	/// <summary>
	/// 見た目（行列・色）の更新（一時停止中も毎フレーム）
	/// </summary>
	void UpdateVisual();

	// 描画（不透明パス）
	void Draw(const KamataEngine::Camera& camera) const;

	// --- 状態 ---
	bool IsInUse() const { return state_ != State::kInactive; }
	// 当たり判定があるか（通常状態）
	bool IsActive() const { return state_ == State::kActive; }
	State GetState() const { return state_; }
	EnemyType GetType() const { return type_; }
	float GetTheta() const { return theta_; }
	// 足元の判定（接地していれば下端0）
	Footprint GetFootprint() const;
	// 体の中心（ワールド座標。エフェクトの位置）
	KamataEngine::Vector3 GetCenterWorld() const;

	// --- 操作（GameScene の当たり判定から呼ぶ） ---
	// 通常状態のスライムに当たった：歩く敵は向きを反転し、スライムから離れる向きへ弾き飛ばされる（kKnockbackDistance）
	void BounceOffPlayer(float playerTheta, float playerHalfWidth);
	// 平らなスライムに乗る（offset：スライムの角度からのずれ）
	void Capture(float offset);
	float GetCaptureOffset() const { return captureOffset_; }
	// 乗っている位置（スライムに合わせて毎フレーム呼ぶ。baseHeight は足元の高さ）
	void SetRiderPose(float theta, float baseHeight);
	// 吸い込みを始める
	void StartAbsorb();
	// 消す（プールへ戻す）
	void Kill() { state_ = State::kInactive; }

	// 調整項目を登録（既定値をGlobalVariablesへ追加する）
	static void RegisterGlobalVariables();
	// 調整項目を適用（GlobalVariablesの現在値を静的メンバへ反映する）
	static void ApplyGlobalVariables();

	// --- 調整項目（GlobalVariables の "Enemy" グループ） ---
	// 体の半径（判定の半幅。高さは半径×1.6）・出現演出の時間（秒）
	static inline float kRadius = 0.9f;
	static inline float kSpawnTime = 0.6f;
	// 出現位置の安全範囲（度）：スライムからこの角度以内に出る敵は、この角度の位置までずらして出す（出た瞬間に重なって理不尽に当たらないように。0で無効）
	static inline float kSpawnSafeAngleDeg = 35.0f;
	// walker の歩く速さ（/秒）・通常状態のスライムに当たったときに弾き飛ばされる距離（弧の長さ）と時間（秒）
	static inline float kWalkerSpeed = 2.5f;
	static inline float kKnockbackDistance = 5.0f;
	static inline float kKnockbackTime = 0.3f;
	// shooter / bomber の攻撃間隔（秒）
	static inline float kShooterInterval = 2.6f;
	static inline float kBomberInterval = 3.6f;
	// 最初の攻撃までの待ち＝間隔 ×（Min～Max を通し番号で決定的にずらした値）
	static inline float kFirstAttackMin = 0.35f;
	static inline float kFirstAttackMax = 0.85f;
	// 攻撃の予備動作（撃つ前に膨らむ）の時間（秒）
	static inline float kAttackWindup = 0.35f;
	// 色（walker / shooter / bomber の体。紫系。bomber は見分けやすいよう青寄り）
	static inline KamataEngine::Vector3 kWalkerColor = {0.45f, 0.15f, 0.7f};
	static inline KamataEngine::Vector3 kShooterColor = {0.6f, 0.12f, 0.55f};
	static inline KamataEngine::Vector3 kBomberColor = {0.16f, 0.2f, 0.75f};

private:
	// 見た目の部品（root は描かない親。それ以外は描く物1つにつき WorldTransform 1つ）
	enum Part {
		kPartBody,
		kPartEyeWhiteL,
		kPartEyeWhiteR,
		kPartPupilL,
		kPartPupilR,
		kPartLegFL,
		kPartLegFR,
		kPartLegBL,
		kPartLegBR,
		kPartAccessory, // shooter：頭のトゲ ／ bomber：背中の爆弾
		kPartCount,
	};
	// 色（同じ色の部品は同じ ObjectColor を共有してよい）
	enum ColorSlot {
		kColorBody,
		kColorEyeWhite,
		kColorPupil,
		kColorAccessory,
		kColorCount,
	};

	// 攻撃する（弾の生成要求を積む）
	void Attack(float playerTheta, std::vector<ProjectileSpawn>* requests);

	const GameModels* models_ = nullptr;
	KamataEngine::WorldTransform root_;
	std::array<KamataEngine::WorldTransform, kPartCount> parts_;
	std::array<KamataEngine::ObjectColor, kColorCount> colors_;

	State state_ = State::kInactive;
	EnemyType type_ = EnemyType::kWalker;
	// 輪の上の位置（角度・足元の高さ）と層の z
	float theta_ = 0.0f;
	float baseHeight_ = 0.0f;
	float layerZ_ = 0.0f;
	// 歩く向き（+1=θが増える向き）と見た目の向き（顔を向ける側）
	int dir_ = -1;
	int facing_ = -1;
	// 弾き飛ばされている残り時間
	float knockbackTimer_ = 0.0f;
	// 状態の経過時間・攻撃までの残り時間・歩いた量（脚の動き）
	float stateTimer_ = 0.0f;
	float attackTimer_ = 0.0f;
	float walkPhase_ = 0.0f;
	// スライムに乗っている位置のずれ（角度）
	float captureOffset_ = 0.0f;
	// 吸い込み（出発点・目標・経過の割合）
	KamataEngine::Vector3 absorbStart_{};
	KamataEngine::Vector3 absorbTarget_{};
	float absorbRate_ = 0.0f;
};
