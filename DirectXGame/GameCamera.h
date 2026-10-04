#pragma once
#include "KamataEngine.h"

/// <summary>
/// ゲームのカメラ。筒の軸上の (0, 0, 層のz − kDistance) に置き、+Z（奥）を向く。基本は固定で、
/// 次の層へ進むとき（StartMove）だけ旧層→新層へ kAdvanceDuration 秒で EaseInOut 補間して動く。
/// 被ダメージ時の小さな画面揺れ（sin ベースの決定的な揺れ・減衰）も持つ。
/// ImGui のデバッグカメラ（注視点のまわりを回る。エンジンの DebugCamera はエンジンのInputが前提で使えないため自前）もここ。
/// </summary>
class GameCamera {
public:
	// デバッグカメラ（オンの間は通常のカメラの代わりに使う。Game Debug ウィンドウから操作する）
	struct DebugOrbit {
		bool enabled = false;
		float yawDeg = 30.0f;        // 左右の向き（度）
		float pitchDeg = 15.0f;      // 上下の向き（度。+で見下ろす）
		float distance = 45.0f;      // 注視点からの距離
		float targetOffsetZ = 10.0f; // 注視点（軸上。見ている層の z からのずれ）
	};

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// 指定の層の位置へすぐに置く（補間なし。開始時・デバッグのジャンプ用）
	/// </summary>
	void SnapTo(float layerZ);

	/// <summary>
	/// 旧層→新層への移動を始める（kAdvanceDuration 秒）
	/// </summary>
	void StartMove(float fromLayerZ, float toLayerZ);

	/// <summary>
	/// ゲーム時間の更新（移動の補間・揺れの減衰。ポーズ中は呼ばない）
	/// </summary>
	void Update();

	/// <summary>
	/// 画面を揺らす（strength 倍の振幅。調整項目 ShakeAmplitude が0なら揺れない）
	/// </summary>
	void Shake(float strength = 1.0f);

	/// <summary>
	/// 調整項目・デバッグカメラを反映して行列を更新する（一時停止中も毎フレーム呼ぶ）
	/// </summary>
	void UpdateMatrix();

	// 見ている層の z（移動中は補間中の値）
	float GetFocusZ() const { return focusZ_; }
	// 移動中か
	bool IsMoving() const { return isMoving_; }
	// 層の移動の進み具合（0→1。EaseInOut 済み＝カメラの z と同じ割合。移動していないときは1）
	float GetMoveProgress() const { return moveProgress_; }
	// カメラ（UpdateMatrix 済み）
	const KamataEngine::Camera& GetCamera() const { return camera_; }
	// デバッグカメラの設定（ImGui から直接いじる）
	DebugOrbit& GetDebugOrbit() { return debugOrbit_; }

	// 調整項目を登録（既定値をGlobalVariablesへ追加する）
	static void RegisterGlobalVariables();
	// 調整項目を適用（GlobalVariablesの現在値を静的メンバへ反映する）
	static void ApplyGlobalVariables();

	// --- 調整項目（GlobalVariables の "Camera" グループ） ---
	// 層からの距離（カメラは層の z − この値）・軸からのずれ（x, y）
	static inline float kDistance = 34.0f;
	static inline KamataEngine::Vector3 kOffset = {0.0f, 0.0f, 0.0f};
	// 縦の視野角（度）・描画する最大距離（全層が収まるように）
	static inline float kFovDeg = 45.0f;
	static inline float kFarZ = 2000.0f;
	// 次の層へ進むときの移動時間（秒）
	static inline float kAdvanceDuration = 2.0f;
	// 画面揺れ：振幅（0で無効）・続く時間（秒）・揺れの速さ（1秒あたりの往復）・場面ごとの強さ（振幅への倍率。スライムの被弾・ボスに爆弾が当たった）
	static inline float kShakeAmplitude = 0.5f;
	static inline float kShakeTime = 0.35f;
	static inline float kShakeFrequency = 22.0f;
	static inline float kShakeOnDamage = 1.0f;
	static inline float kShakeOnBossHit = 0.6f;

private:
	KamataEngine::Camera camera_;
	// 見ている層の z
	float focusZ_ = 0.0f;
	// 層の移動（補間）
	bool isMoving_ = false;
	float moveFromZ_ = 0.0f;
	float moveToZ_ = 0.0f;
	float moveTimer_ = 0.0f;
	float moveProgress_ = 1.0f;
	// 揺れ（残り時間・強さ・経過時間）
	float shakeTimer_ = 0.0f;
	float shakeStrength_ = 0.0f;
	float shakeClock_ = 0.0f;
	DebugOrbit debugOrbit_;
};
