#pragma once
#include "Fade.h"
#include "KamataEngine.h"

/// <summary>
/// タイトルシーン（仮）。決定でゲームへ進む。
/// 見た目は仮UI：背景色の板（white1x1.png）と DebugText の文字（ASCIIのみ）だけ。
/// 本番のタイトルロゴ・背景・演出はここへ足していく（シーンの流れ＝フェードイン→入力待ち→フェードアウトはそのまま使える）。
/// 調整項目は GlobalVariables の "Title" グループ（ImGuiで編集できる。このシーンは自分の Update 冒頭で Apply する）
/// </summary>
class TitleScene {
public:
	~TitleScene();

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

	// シーンが終了したか（mainが次のシーンへ切り替える）
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

	// フェード
	Fade fade_;
	// 背景の板（画面全体。色は調整項目）
	KamataEngine::Sprite* backgroundSprite_ = nullptr;

	// シーンの進行状態
	Phase phase_ = Phase::kFadeIn;
	// シーンが終了したか
	bool isFinished_ = false;
	// 経過フレーム（案内文字の点滅に使う）
	int32_t frameCounter_ = 0;

	// --- 調整項目（GlobalVariables の "Title" グループ） ---
	// フェードの時間（秒）
	static inline float kFadeDuration = 0.7f;
	// 背景色（RGB 0～1）
	static inline KamataEngine::Vector3 kBackgroundColor = {0.01f, 0.02f, 0.06f};
	// タイトル文字の中心位置（ピクセル。zは未使用）と倍率
	static inline KamataEngine::Vector3 kTitlePosition = {960.0f, 420.0f, 0.0f};
	static inline float kTitleScale = 5.0f;
	// 遊び方の説明（タイトルの下の2行）の文字の倍率と、行の間隔（文字の高さに対する倍率）
	static inline float kHelpScale = 2.0f;
	static inline float kHelpLineSpacing = 1.6f;
	// 案内文字（PRESS SPACE）の中心位置と倍率、点滅の周期（フレーム。0以下で点滅しない）
	static inline KamataEngine::Vector3 kPressPosition = {960.0f, 760.0f, 0.0f};
	static inline float kPressScale = 3.0f;
	static inline int32_t kBlinkFrames = 60;
};
