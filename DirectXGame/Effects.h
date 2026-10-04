#pragma once
#include "KamataEngine.h"
#include <memory>
#include <vector>

struct GameModels;

/// <summary>
/// 3Dのエフェクト（上限付きのプール。粒1つにつき WorldTransform と ObjectColor を1つずつ持ち、使い回す）。
/// ・爆発      : 加算合成で膨らんで消えるオレンジの球（SpawnExplosion）
/// ・取り込み  : 緑の輪っか状に広がる小球（SpawnAbsorb。画面と平行な面に広がる）
/// ・火花      : 飛び散る小箱（SpawnSparks。被弾=赤、ボス被弾=オレンジ、弾が消える=灰色 など色で使い分け）
/// プールが一杯のときは新しい粒を出さない（古い粒が消えるのを待つ）
/// </summary>
class Effects {
public:
	// 粒の数の上限（WorldTransform・ObjectColor の数。必要になった分だけ作る）
	static constexpr int kMaxParticles = 256;

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(const GameModels* models);

	// すべて消す（フェーズのやり直しなど）
	void Clear();

	// 爆発（radius：最大の半径）
	void SpawnExplosion(const KamataEngine::Vector3& position, float radius);
	// 取り込みの輪っか（color：粒の色）
	void SpawnAbsorb(const KamataEngine::Vector3& position, const KamataEngine::Vector3& color);
	// 火花（count 個を speed（/秒）で飛び散らせる）
	void SpawnSparks(const KamataEngine::Vector3& position, const KamataEngine::Vector3& color, int count, float speed);

	/// <summary>
	/// 1フレーム分の更新（ポーズ中は呼ばない）
	/// </summary>
	void Update();

	/// <summary>
	/// 見た目（行列・色）の更新（一時停止中も毎フレーム）
	/// </summary>
	void UpdateVisual();

	// 描画（不透明パス：火花の箱）
	void DrawOpaque(const KamataEngine::Camera& camera) const;
	// 描画（半透明パス：取り込みの粒）
	void DrawTranslucent(const KamataEngine::Camera& camera) const;
	// 描画（加算パス：爆発）
	void DrawAdditive(const KamataEngine::Camera& camera) const;

	// 使用中の粒の数（デバッグ表示用）
	int GetActiveCount() const;

	// 調整項目を登録（既定値をGlobalVariablesへ追加する）
	static void RegisterGlobalVariables();
	// 調整項目を適用（GlobalVariablesの現在値を静的メンバへ反映する）
	static void ApplyGlobalVariables();

	// --- 調整項目（GlobalVariables の "Effect" グループ） ---
	// 爆発：時間（秒）・色
	static inline float kExplosionTime = 0.4f;
	static inline KamataEngine::Vector3 kExplosionColor = {1.0f, 0.45f, 0.1f};
	// 取り込み：粒の数・広がる速さ（/秒）・時間（秒）・粒の大きさ・減速の強さ（/秒。速さが1秒で e^-値 倍になる。6.3 ≒ 1フレームで0.9倍）
	static inline int32_t kAbsorbParticleCount = 10;
	static inline float kAbsorbSpeed = 6.0f;
	static inline float kAbsorbTime = 0.45f;
	static inline float kAbsorbParticleSize = 0.25f;
	static inline float kAbsorbDamping = 6.3f;
	// 火花：時間（秒）・大きさ・重さ（輪の外向きへ落ちる加速度。/秒²）
	static inline float kSparkTime = 0.5f;
	static inline float kSparkSize = 0.22f;
	static inline float kSparkGravity = 12.0f;

private:
	// 粒の種類（描くパスとモデルが決まる）
	enum class Kind {
		kExplosion, // 加算の球
		kOrb,       // 半透明の球
		kSpark,     // 不透明の箱
	};

	// 粒1つ
	struct Particle {
		bool active = false;
		Kind kind = Kind::kSpark;
		KamataEngine::Vector3 position{};
		KamataEngine::Vector3 velocity{};
		KamataEngine::Vector3 color{};
		float startSize = 1.0f;
		float endSize = 1.0f;
		float life = 0.0f;    // 経過時間
		float maxLife = 1.0f; // 寿命
		float spin = 0.0f;    // 回転（火花の箱）
		KamataEngine::WorldTransform worldTransform;
		KamataEngine::ObjectColor objectColor;
	};

	// 空いている粒を探す（無ければ上限まで新しく作る。上限なら nullptr）
	Particle* Acquire();
	// 指定の種類の粒を描く
	void DrawKind(const KamataEngine::Camera& camera, Kind kind, KamataEngine::Model* model) const;

	const GameModels* models_ = nullptr;
	// 粒のプール（WorldTransform はコピー・ムーブ不可なので unique_ptr で持つ）
	std::vector<std::unique_ptr<Particle>> particles_;
	// 火花の飛ぶ向きを決める通し番号（乱数を使わず決定的にばらす）
	int sparkSerial_ = 0;
};
