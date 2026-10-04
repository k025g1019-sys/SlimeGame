#pragma once
#include "Fade.h"
#include "KamataEngine.h"

/// <summary>
/// ゲームクリアシーン（仮）。開始時にファンファーレを1回鳴らし、決定でタイトルへ戻る。
/// 見た目は仮UI：背景色の板（white1x1.png）と DebugText の文字（ASCIIのみ）だけ。
/// 本番の演出（リザルト・スコアなど）はここへ足していく。
/// 調整項目は GlobalVariables の "Clear" グループ（ImGuiで編集できる。このシーンは自分の Update 冒頭で Apply する）
/// </summary>
class GameClearScene {
public:
	~GameClearScene();

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// 更新
	/// </summary>
	void Update();

	/// <summary>
	/// 描画
	/// </summary>
	void Draw();

	// シーンが終了したか（mainがタイトルシーンへ切り替える）
	bool IsFinished() const { return isFinished_; }

	// 調整項目を登録（既定値をGlobalVariablesへ追加する）
	static void RegisterGlobalVariables();
	// 調整項目を適用（GlobalVariablesの現在値を静的メンバへ反映する）
	static void ApplyGlobalVariables();

private:
	// シーンの進行状態
	enum class Phase {
		kFadeIn,  // フェードイン中
		kMain,    // 入力受付中
		kFadeOut, // フェードアウト中
	};

	// 背景の板（画面全体。色は調整項目）
	KamataEngine::Sprite* backgroundSprite_ = nullptr;
	// フェード
	Fade fade_;

	// シーンの進行状態
	Phase phase_ = Phase::kFadeIn;
	// シーンが終了したか
	bool isFinished_ = false;
	// 経過フレーム（文字の揺れに使う）
	int32_t frameCounter_ = 0;

	// --- 調整項目（GlobalVariables の "Clear" グループ） ---
	// フェードの時間（秒）
	static inline float kFadeDuration = 0.7f;
	// 背景色（RGB 0～1）
	static inline KamataEngine::Vector3 kBackgroundColor = {0.06f, 0.16f, 0.1f};
	// 見出し（GAME CLEAR）の中心位置（ピクセル。zは未使用）と倍率、上下の揺れ幅（ピクセル）
	static inline KamataEngine::Vector3 kTitlePosition = {960.0f, 420.0f, 0.0f};
	static inline float kTitleScale = 7.0f;
	static inline float kTitleBobAmplitude = 12.0f;
	// 案内文字の中心位置と倍率
	static inline KamataEngine::Vector3 kPressPosition = {960.0f, 760.0f, 0.0f};
	static inline float kPressScale = 3.0f;
};
