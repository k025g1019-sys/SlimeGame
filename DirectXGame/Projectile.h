#pragma once
#include "Field.h"
#include "KamataEngine.h"
#include <array>

struct GameModels;

// 弾の種類
enum class ProjectileKind {
	kSpike, // トゲ弾（通常状態で当たると痛い／平らなら食べられる）
	kBomb,  // 爆弾（取り込むとダメージ／通常状態の弾力で跳ね返せる）
};

/// <summary>
/// 弾の生成要求。敵・ボスは弾を直接作らず、これを要求リストに積むだけにする
/// （更新のループ中に弾のコンテナを触らないため。GameScene がループの後でまとめて生成する）
/// </summary>
struct ProjectileSpawn {
	ProjectileKind kind = ProjectileKind::kSpike;
	bool fromAir = false;             // true=空中弾（ボスが投げる。airStart から着弾点 theta へ飛ぶ）
	float theta = 0.0f;               // 地上弾：出現する角度 ／ 空中弾：着弾点の角度
	int dir = -1;                     // 地上弾の進む向き（+1=θが増える向き）
	KamataEngine::Vector3 airStart{}; // 空中弾の出発点（ワールド座標）
};

/// <summary>
/// 弾（トゲ弾・爆弾）。GameScene が固定数のプールで持ち、使い回す（IsInUse で使用中か分かる）。
///
/// 【状態】
/// 　kAir       : ボスが投げた空中弾。出発点から着弾点へ kAirTravelTime 秒で直線移動（着弾点に予告マーカー）。着弾で kSurface へ
/// 　kSurface   : 敵の弾として輪に沿って進む（トゲ=滑る、爆弾=転がる）。空中から着地したトゲは kAirSpikeLandPause 秒その場で止まってから滑る
/// 　             （ボスの攻撃で、真ん中の爆弾を跳ね返してから隣のトゲが来るまでに平らになる余裕を作るため）
/// 　kCaptured  : 平らなスライムに乗っている（位置は GameScene が毎フレーム決める）
/// 　kAbsorbing : スライムに吸い込まれている途中（kAbsorbTime 秒）
/// 　kReflected : 跳ね返された爆弾（味方の弾）。逆向きに速く転がり、敵に当たると爆発
/// 　kToBoss    : 跳ね返された爆弾がボスへ直線で飛んでいる（kReflectToBossTime 秒）
/// </summary>
class Projectile {
public:
	enum class State {
		kInactive,
		kAir,
		kSurface,
		kCaptured,
		kAbsorbing,
		kReflected,
		kToBoss,
	};

	// Update の結果（GameScene がこれを見て爆発・消滅などを処理する）
	enum class Event {
		kNone,
		kLanded,  // 空中弾が着弾した（以後は地上弾。重なっていれば通常の接触判定で処理される）
		kFuseOut, // 爆弾の導火線が尽きた（爆発させる）
		kExpired, // トゲ弾が半周進んだ・寿命が尽きた（消す）
		kArrived, // ボスへ飛んだ爆弾が着いた（ボスに当てる）
		kAbsorbed // 吸い込みが終わった（取り込みの効果を出す）
	};

	/// <summary>
	/// 初期化（プールに作ったとき1回だけ）
	/// </summary>
	void Initialize(const GameModels* models);

	/// <summary>
	/// 生成要求から出現させる
	/// </summary>
	void Spawn(const ProjectileSpawn& spawn, float layerZ);

	/// <summary>
	/// 1フレーム分の更新
	/// </summary>
	/// <param name="playerTheta">スライムの角度（着弾後に向かう向きを決める）</param>
	/// <param name="absorbTarget">吸い込み先（スライムの中心。ワールド座標）</param>
	/// <param name="bossTarget">ボスの体の中心（ボスへ飛ぶ爆弾の目標。ワールド座標）</param>
	Event Update(float playerTheta, const KamataEngine::Vector3& absorbTarget, const KamataEngine::Vector3& bossTarget);

	/// <summary>
	/// 見た目（行列・色）の更新（一時停止中も毎フレーム）
	/// </summary>
	void UpdateVisual();

	// 描画（不透明パス）
	void Draw(const KamataEngine::Camera& camera) const;
	// 描画（輪郭のパス＝表面カリングの不透明。爆弾の縁取り。少し大きい光る球の裏側だけが体のまわりに見える）
	void DrawOutline(const KamataEngine::Camera& camera) const;
	// 描画（加算パス。導火線の火花）
	void DrawAdditive(const KamataEngine::Camera& camera) const;
	// 着弾予告マーカー（LineDrawer。空中弾のときだけ描く。kMarkerLineCount 本）
	void DrawMarkerLines() const;
	// 予告マーカー1つの線の本数（外側の円2重 16本×2 ＋ 縮む内側の円 12本。線の本数の見積もりは LineDrawer.h）
	static constexpr int kMarkerLineCount = 16 * 2 + 12;

	// --- 状態 ---
	bool IsInUse() const { return state_ != State::kInactive; }
	State GetState() const { return state_; }
	bool IsBomb() const { return kind_ == ProjectileKind::kBomb; }
	// 味方の弾か（跳ね返された爆弾）
	bool IsFriendly() const { return state_ == State::kReflected || state_ == State::kToBoss; }
	float GetTheta() const { return theta_; }
	float GetHeight() const { return height_; }
	// 弾の半径（判定・爆風の中心の高さに使う）
	float GetRadius() const;
	// 足元の判定
	Footprint GetFootprint() const;
	// 今のワールド座標
	KamataEngine::Vector3 GetWorldPosition() const { return position_; }

	// --- 操作（GameScene の当たり判定から呼ぶ） ---
	// 平らなスライムに乗る（offset：スライムの角度からのずれ）
	void Capture(float offset);
	float GetCaptureOffset() const { return captureOffset_; }
	// 乗っている位置（スライムに合わせて毎フレーム呼ぶ。height は弾の中心の高さ）
	void SetRiderPose(float theta, float height);
	// 吸い込みを始める
	void StartAbsorb();
	// 跳ね返して味方の弾として輪に沿って転がす（向きを逆にして速くする）
	void Reflect();
	// 跳ね返してボスへ直線で飛ばす
	void LaunchToBoss();
	// 消す（プールへ戻す）
	void Kill() { state_ = State::kInactive; }

	// 調整項目を登録（既定値をGlobalVariablesへ追加する）
	static void RegisterGlobalVariables();
	// 調整項目を適用（GlobalVariablesの現在値を静的メンバへ反映する）
	static void ApplyGlobalVariables();

	// --- 調整項目（GlobalVariables の "Projectile" グループ） ---
	// トゲ弾：進む速さ（/秒）・高さ（輪の内面から中心まで）・判定の半径・寿命（秒）・色
	static inline float kSpikeSpeed = 8.0f;
	static inline float kSpikeHeight = 0.7f;
	static inline float kSpikeRadius = 0.45f;
	static inline float kSpikeLifetime = 6.0f;
	static inline KamataEngine::Vector3 kSpikeColor = {1.0f, 0.45f, 0.08f};
	// 爆弾：半径・転がる速さ（/秒）・導火線（秒）・点滅し始める残り時間（秒）・爆風の半径・色
	static inline float kBombRadius = 0.6f;
	static inline float kBombRollSpeed = 5.0f;
	static inline float kBombFuse = 5.5f;
	static inline float kBombBlinkTime = 1.5f;
	static inline float kBlastRadius = 3.0f;
	static inline KamataEngine::Vector3 kBombColor = {0.06f, 0.06f, 0.08f};
	static inline KamataEngine::Vector3 kBombBlinkColor = {0.9f, 0.08f, 0.05f};
	static inline KamataEngine::Vector3 kFriendlyBombColor = {0.08f, 0.35f, 0.15f};
	// 爆弾の見やすさ：縁取りの大きさ（半径の倍率。1で消える）と色（敵の爆弾。味方の爆弾は緑）・導火線の火花の大きさ（半径の倍率）
	// （黒い爆弾は暗い輪の上で見えにくいので、明るい縁取りで形を出す）
	static inline float kBombOutlineScale = 1.16f;
	static inline KamataEngine::Vector3 kBombOutlineColor = {1.0f, 0.3f, 0.08f};
	static inline float kBombSparkScale = 0.5f;
	// 跳ね返し：転がる速さの倍率・ボスへ飛ぶ時間（秒）
	static inline float kReflectSpeedScale = 1.8f;
	static inline float kReflectToBossTime = 0.8f;
	// 空中弾：飛ぶ時間（秒。上限 kMaxAirTravelTime）・予告マーカーの半径・空中から着地したトゲが止まっている時間（秒）・吸い込みの時間（秒）
	static inline float kAirTravelTime = 1.2f;
	static inline float kMarkerRadius = 1.5f;
	static inline float kAirSpikeLandPause = 0.3f;
	static inline float kAbsorbTime = 0.2f;
	// 空中弾の飛ぶ時間の上限（秒。長くすると空中の弾＝予告マーカーが増える）
	static constexpr float kMaxAirTravelTime = 5.0f;

private:
	// 見た目の部品（描く物1つにつき WorldTransform と ObjectColor を1つずつ）
	enum Part {
		kPartBody,    // トゲ：四角すい ／ 爆弾：球
		kPartFuse,    // 爆弾の導火線（細い箱）
		kPartSpark,   // 爆弾の火花（加算の小さな球）
		kPartOutline, // 爆弾の縁取り（光る球を表面カリングで描く）
		kPartCount,
	};

	const GameModels* models_ = nullptr;
	std::array<KamataEngine::WorldTransform, kPartCount> transforms_;
	std::array<KamataEngine::ObjectColor, kPartCount> colors_;

	State state_ = State::kInactive;
	ProjectileKind kind_ = ProjectileKind::kSpike;
	// 輪の上の位置（角度・中心の高さ）と層の z
	float theta_ = 0.0f;
	float height_ = 0.0f;
	float layerZ_ = 0.0f;
	// 進む向き（+1=θが増える向き）
	int dir_ = -1;
	// 進んだ距離・経過時間（トゲの寿命）・導火線の残り（爆弾）・着地して止まっている残り時間（空中から来たトゲ）
	float traveled_ = 0.0f;
	float age_ = 0.0f;
	float fuse_ = 0.0f;
	float landPause_ = 0.0f;
	// 空中弾・ボスへ飛ぶ爆弾・吸い込みの補間（出発点・経過の割合）
	KamataEngine::Vector3 flightStart_{};
	float flightRate_ = 0.0f;
	// 吸い込み中の大きさ（1→小）
	float absorbScale_ = 1.0f;
	// スライムに乗っている位置のずれ（角度）
	float captureOffset_ = 0.0f;
	// 今のワールド座標（見た目と爆発の位置）
	KamataEngine::Vector3 position_{};
	// 回転の演出（トゲのきりもみ）
	float spin_ = 0.0f;
};
