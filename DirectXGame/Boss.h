#pragma once
#include "KamataEngine.h"
#include "Projectile.h"
#include <array>
#include <vector>

struct GameModels;
class Effects;

/// <summary>
/// ボス（オレンジの箱で組んだゴーレム。仮）。筒の中心（軸上）、層の輪より少し奥（z = 層のz + kDepthOffset）に浮かぶ。
/// ボスフェーズ：登場（初回は奥から飛来）→ 攻撃（空中弾を投げる）→ HP0 で奥へ逃げる（次のボスフェーズの層で待機し、
/// 奥に小さく見える。以降のボスフェーズが無ければ消える）。最後のフェーズのボスは HP0 で撃破演出（爆発の連鎖）。
/// 跳ね返した爆弾が当たると HP−1（白く光ってのけぞる）。
///
/// 【攻撃（仮）】kAttackInterval（レベルが上がるほど短く）ごとに空中弾を レベル×kVolleyPerLevel 発投げる。
/// 着弾点はスライムの予測位置（今の角度＋角速度×飛ぶ時間）を中心に kVolleySpread ずつずらす。
/// 爆弾とトゲは決定的なパターンで混ぜ、kBombEveryNthAttack 回に1回（既定2）は必ず爆弾を1つ含める
/// （ボスを倒す手段が爆弾の跳ね返しのため）。※本番の攻撃・倒し方は未定。ここを差し替える
/// </summary>
class Boss {
public:
	enum class State {
		kAbsent,   // いない（描かない）
		kEnter,    // 奥から飛来中
		kAttack,   // 攻撃中
		kHit,      // のけぞり中（跳ね返した爆弾は当たる）
		kFlee,     // 奥へ逃げている
		kIdleFar,  // 次のボスフェーズの層で待機中（奥に小さく見える）
		kDefeated, // 撃破演出中（爆発の連鎖の後に消える）
	};

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(const GameModels* models, Effects* effects);

	// いない状態にする
	void Hide();
	// 指定の層で待機させる（奥に小さく見える）
	void PlaceIdle(float layerZ);
	// ボスフェーズを始める（その層で待機中ならそのまま攻撃へ、その層へ逃げている途中なら今の位置から飛来を続ける、いなければ奥から飛来）
	void StartPhase(float layerZ, int level, int hp);

	/// <summary>
	/// 1フレーム分の更新
	/// </summary>
	/// <param name="battleActive">攻撃してよいか（フェーズの kBattle 中だけ）</param>
	/// <param name="playerTheta">スライムの角度</param>
	/// <param name="playerAngularSpeed">スライムの角速度（ラジアン/秒。着弾点の予測に使う）</param>
	/// <param name="requests">弾の生成要求を積む先</param>
	void Update(bool battleActive, float playerTheta, float playerAngularSpeed, std::vector<ProjectileSpawn>* requests);

	/// <summary>
	/// 見た目（行列・色）の更新（一時停止中も毎フレーム）
	/// </summary>
	void UpdateVisual();

	// 描画（不透明パス）
	void Draw(const KamataEngine::Camera& camera) const;

	// 跳ね返した爆弾が向かう状態か（攻撃中・のけぞり中）
	bool IsTargetable() const { return (state_ == State::kAttack || state_ == State::kHit) && hp_ > 0; }
	// 1ダメージ（攻撃中・のけぞり中だけ効く＝IsTargetable。飛来中などは何もしない。戻り値：HPが0になったか）
	bool Hit();
	// 奥へ逃げる（hasNext：次のボスフェーズがあるか。あればその層 nextLayerZ で待機、無ければ消える）
	void StartFlee(bool hasNext, float nextLayerZ);
	// 撃破演出を始める（最後のフェーズのボス）
	void StartDefeat();

	State GetState() const { return state_; }
	bool IsVisible() const { return state_ != State::kAbsent; }
	int GetHp() const { return hp_; }
	int GetMaxHp() const { return maxHp_; }
	int GetLevel() const { return level_; }
	// 体の中心（ワールド座標。跳ね返した爆弾の目標）
	KamataEngine::Vector3 GetBodyCenter() const;

	// 調整項目を登録（既定値をGlobalVariablesへ追加する）
	static void RegisterGlobalVariables();
	// 調整項目を適用（GlobalVariablesの現在値を静的メンバへ反映する）
	static void ApplyGlobalVariables();

	// --- 調整項目（GlobalVariables の "Boss" グループ） ---
	// 大きさの倍率・層の輪からの奥行き（z）・軸からの高さ
	static inline float kScale = 1.8f;
	static inline float kDepthOffset = 16.0f;
	static inline float kHeight = 0.5f;
	// 登場（奥から飛来する時間・距離）・逃げる時間
	static inline float kEnterTime = 2.5f;
	static inline float kEnterDistance = 140.0f;
	static inline float kFleeTime = 2.2f;
	// 攻撃：間隔（秒。レベル1）・レベルが1上がるごとに掛ける倍率・1レベルあたりの弾数・着弾点の間隔（弧の長さ）・
	// 爆弾を含める攻撃の間隔（N回に1回）・攻撃開始から最初の攻撃までの時間・投げる前の予備動作の時間
	// （着弾点の間隔は「真ん中の爆弾を跳ね返してから隣のトゲが来るまで」に平らになる余裕を作る。狭いと跳ね返しと被弾が同時になる）
	static inline float kAttackInterval = 3.2f;
	static inline float kAttackIntervalLevelScale = 0.8f;
	static inline int32_t kVolleyPerLevel = 1;
	static inline float kVolleySpread = 5.0f;
	static inline int32_t kBombEveryNthAttack = 2;
	static inline float kFirstAttackDelay = 1.2f;
	static inline float kThrowWindup = 0.5f;
	// 被弾：のけぞる時間・白く光る時間
	static inline float kHitTime = 0.6f;
	static inline float kHitFlashTime = 0.25f;
	// 撃破演出：時間・爆発の間隔
	static inline float kDefeatTime = 2.4f;
	static inline float kDefeatExplosionInterval = 0.14f;
	// 揺れ（ゆっくり左右に傾く角度（ラジアン）・速さ）
	static inline float kSwayAmplitude = 0.08f;
	static inline float kSwaySpeed = 1.3f;
	// 色（体・目）と、ボス専用の箱のマテリアルの環境光・拡散光の強さ（陰影を弱めて色を明るく出す。輪などの box には影響しない）
	static inline KamataEngine::Vector3 kBodyColor = {1.0f, 0.4f, 0.06f};
	static inline KamataEngine::Vector3 kEyeColor = {1.0f, 0.95f, 0.3f};
	static inline float kMaterialAmbient = 0.65f;
	static inline float kMaterialDiffuse = 0.55f;

private:
	// 部品（root の子。腕は肩の回転軸（描かない）の子）
	enum Part {
		kPelvis,
		kTorso,
		kHead,
		kEyeL,
		kEyeR,
		kShoulderL, // 描かない（腕の回転軸）
		kShoulderR, // 描かない（腕の回転軸）
		kUpperArmL,
		kHandL,
		kUpperArmR,
		kHandR,
		kLegL,
		kLegR,
		kFootL,
		kFootR,
		kPartCount,
	};

	// 1回分の攻撃（空中弾の生成要求を積む）
	void Throw(float playerTheta, float playerAngularSpeed, std::vector<ProjectileSpawn>* requests);
	// 今の手の位置（投げる弾の出発点。前フレームの行列から取る）
	KamataEngine::Vector3 GetHandPosition(bool rightHand) const;
	// 攻撃の間隔（レベルで短くなる）
	float GetAttackInterval() const;
	// 層の z からボスの待機位置
	static KamataEngine::Vector3 HomePosition(float layerZ);

	const GameModels* models_ = nullptr;
	Effects* effects_ = nullptr;
	KamataEngine::WorldTransform root_;
	std::array<KamataEngine::WorldTransform, kPartCount> parts_;
	// 色（体・目。同じ色の部品は共有）
	KamataEngine::ObjectColor bodyColor_;
	KamataEngine::ObjectColor eyeColor_;

	State state_ = State::kAbsent;
	float stateTimer_ = 0.0f;
	// 位置（飛来・逃走の補間の始点と終点）
	KamataEngine::Vector3 position_{};
	KamataEngine::Vector3 moveFrom_{};
	KamataEngine::Vector3 moveTo_{};
	bool fleeHasNext_ = false;
	// 体力・レベル
	int hp_ = 0;
	int maxHp_ = 1;
	int level_ = 1;
	// 攻撃（次の攻撃までの残り時間・今のフェーズで投げた回数・投げた直後の腕の動きの残り時間）
	float attackTimer_ = 0.0f;
	int attackCount_ = 0;
	float throwRecover_ = 0.0f;
	// 被弾の白い光の残り時間
	float flashTimer_ = 0.0f;
	// 撃破演出の爆発の間隔・回数
	float defeatExplosionTimer_ = 0.0f;
	int defeatExplosionCount_ = 0;
	// 見た目の大きさ（逃げて消えるときに縮む）
	float visualScale_ = 1.0f;
	// 揺れの時計
	float clock_ = 0.0f;
};
