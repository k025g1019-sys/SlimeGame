#pragma once
#include "Field.h"
#include "KamataEngine.h"
#include <array>

struct GameModels;

/// <summary>
/// プレイヤー（緑のスライム）。輪の内面に張り付いて、何もしなくても反時計回り（θ が増える向き。画面下では右）へ進み続ける。
/// ・速度：A/D（GetMove().x）で少しだけ調整（下限 kMinSpeed > 0 なので後退はしない）
/// ・平ら：スペース（kFlatten）を押している間は平ら、離した瞬間に元に戻る（復元イベント＝取り込みの合図）
/// ・見た目の「平らさ」はバネで目標へ追従し、ぷるんと揺れる（跳ね返し・被弾でバネにインパルス）
/// ・体は半透明（半透明パス）、目と芯は不透明（不透明パス）
/// 取り込み・跳ね返しなど、他の物との関係は GameScene（GameSceneInteraction.cpp）が判定する
/// </summary>
class Player {
public:
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(const GameModels* models);

	/// <summary>
	/// 開始状態に戻す（HP満タン・開始角度・通常状態。z は今いる層）
	/// </summary>
	void Reset(float z);

	/// <summary>
	/// 1フレーム分の更新
	/// </summary>
	/// <param name="moveX">速度調整の入力（-1～+1。+で加速）</param>
	/// <param name="flattenHeld">平らボタンを押しているか</param>
	/// <param name="canMove">進んでよいか（false なら止まる。敗北演出中など）</param>
	void Update(float moveX, bool flattenHeld, bool canMove);

	/// <summary>
	/// 見た目（行列・色）の更新（一時停止中も毎フレーム）
	/// </summary>
	void UpdateVisual();

	// 描画（不透明パス：目と芯）
	void DrawOpaque(const KamataEngine::Camera& camera) const;
	// 描画（半透明パス：体）
	void DrawTranslucent(const KamataEngine::Camera& camera) const;

	// --- 状態 ---
	float GetTheta() const { return theta_; }
	// 角度を直接置く（層の移動中に開始角度へ寄せる演出で使う）
	void SetTheta(float theta);
	// 開始角度（ラジアン。調整項目 StartAngleDeg から。どのフェーズもこの角度から始まる）
	static float GetStartTheta();
	// 角速度（ラジアン/秒。ボスの着弾点の予測に使う）
	float GetAngularSpeed() const;
	float GetSpeed() const { return speed_; }
	float GetZ() const { return z_; }
	void SetZ(float z) { z_ = z; }
	bool IsFlat() const { return isFlat_; }
	// このフレームに平らになった／元に戻ったか
	bool IsFlattenTriggered() const { return flattenTriggered_; }
	bool IsRestoreTriggered() const { return restoreTriggered_; }
	// 今の状態（通常/平ら）の足元の判定
	Footprint GetFootprint() const;
	// 平らのときの足元の判定（乗る・取り込む判定に使う）
	Footprint GetFlatFootprint() const;
	// 乗った物を置く高さ（見た目の上面。バネで揺れる）と、乗れる範囲の半幅（角度）
	float GetRiderHeight() const;
	float GetRiderHalfAngle() const;
	// 体の中心（ワールド座標。取り込みの吸い込み先・エフェクトの位置）
	KamataEngine::Vector3 GetCenterWorld() const;
	int GetHp() const { return hp_; }
	bool IsDead() const { return hp_ <= 0; }
	bool IsInvincible() const { return invincibleTimer_ > 0.0f || sDebugInvincible; }

	// --- 操作 ---
	// ダメージを受ける（無敵中は受けない。戻り値：受けたか）
	bool Damage(int amount);
	// 回復する（最大HPまで）
	void Heal(int amount);
	// バネにインパルスを加える（+で平らな側へ、−で縦に伸びる側へ）
	void AddSpringImpulse(float impulse) { flatnessVelocity_ += impulse; }
	// 平らボタンを一度離すまで平らにしない（タイトルの決定・ポーズの解除と同じキーのため）
	void RequireFlattenRelease() { waitRelease_ = true; }
	// 敗北演出を始める（つぶれて薄くなる）
	void StartDefeat();

	// 調整項目を登録（既定値をGlobalVariablesへ追加する）
	static void RegisterGlobalVariables();
	// 調整項目を適用（GlobalVariablesの現在値を静的メンバへ反映する）
	static void ApplyGlobalVariables();

	// デバッグ：無敵（Game Debug の Invincible。JSONには保存しない）
	static inline bool sDebugInvincible = false;

	// --- 調整項目（GlobalVariables の "Player" グループ） ---
	// 開始角度（度。-90=画面の下）
	static inline float kStartAngleDeg = -90.0f;
	// 大きさ：半径・通常の縦の倍率・平らの横幅の倍率・平らの高さの倍率・平らの判定の上面の高さ
	static inline float kRadius = 1.2f;
	static inline float kNormalHeightScale = 0.85f;
	static inline float kFlatWidthScale = 1.9f;
	static inline float kFlatHeightScale = 0.28f;
	static inline float kFlatTop = 0.6f;
	// 乗った物を置ける範囲（見た目の横幅の半分に対する割合。1で体の端まで）
	static inline float kRiderHalfWidthScale = 0.8f;
	// 速さ（弧の長さ/秒）：基本・A/Dでの調整幅・下限・目標速度への追従の速さ（/秒）・平らのときの倍率
	static inline float kBaseSpeed = 4.5f;
	static inline float kSpeedAdjust = 2.5f;
	static inline float kMinSpeed = 1.2f;
	static inline float kSpeedResponse = 6.0f;
	static inline float kFlatSpeedScale = 1.0f;
	// 平らさのバネ：硬さ・減衰・跳ね返したときのインパルス・被弾のインパルス
	static inline float kSpringStiffness = 320.0f;
	static inline float kSpringDamping = 14.0f;
	static inline float kBounceImpulse = -9.0f;
	static inline float kDamageImpulse = 7.0f;
	// 体力：最大HP（上限は HUD の四角の数 Hud::kMaxHpPips）・被弾後の無敵時間（秒）・フェーズクリア時の回復
	static inline int32_t kMaxHp = 5;
	static inline float kInvincibleTime = 1.5f;
	static inline int32_t kPhaseClearHeal = 0;
	// 取り込み：トゲを食べたときの回復・爆弾を取り込んだときのダメージ・平らなときに敵/トゲが乗るか
	static inline int32_t kSpikeEatHeal = 0;
	static inline int32_t kBombAbsorbDamage = 1;
	static inline bool kEnemyStickToFlat = true;
	static inline bool kSpikeStickToFlat = true;
	// 見た目：体の色・不透明度・芯の色・目の大きさ（半径に対する倍率）
	static inline KamataEngine::Vector3 kColor = {0.2f, 0.85f, 0.3f};
	static inline float kAlpha = 0.78f;
	static inline KamataEngine::Vector3 kCoreColor = {0.05f, 0.35f, 0.1f};
	static inline float kEyeSize = 0.16f;

private:
	// 今の平らさ（バネの値）から体の大きさ（半径の楕円体）を求める
	KamataEngine::Vector3 GetBodyScale() const;

	const GameModels* models_ = nullptr;
	// 見た目：root（輪の上の位置・向き。描かない）・体・芯・目
	KamataEngine::WorldTransform root_;
	KamataEngine::WorldTransform body_;
	KamataEngine::WorldTransform core_;
	std::array<KamataEngine::WorldTransform, 2> eyes_;
	KamataEngine::ObjectColor bodyColor_;
	KamataEngine::ObjectColor coreColor_;
	KamataEngine::ObjectColor eyeColor_;

	// 輪の上の位置（角度）・層の z・速さ（弧の長さ/秒）
	float theta_ = 0.0f;
	float z_ = 0.0f;
	float speed_ = 0.0f;
	// 平ら（論理の状態）とこのフレームのイベント
	bool isFlat_ = false;
	bool flattenTriggered_ = false;
	bool restoreTriggered_ = false;
	// 平らボタンを一度離すまで待つか
	bool waitRelease_ = false;
	// 見た目の平らさ（0=通常、1=平ら。バネで追従し、揺れで範囲外にもなる）と、その速度
	float flatness_ = 0.0f;
	float flatnessVelocity_ = 0.0f;
	// 体力・無敵の残り時間・点滅用のフレーム数
	int hp_ = 0;
	float invincibleTimer_ = 0.0f;
	int blinkFrame_ = 0;
	// 敗北演出
	bool defeated_ = false;
	float defeatTimer_ = 0.0f;
};
