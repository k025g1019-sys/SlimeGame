#pragma once
#include "Fade.h"
#include "KamataEngine.h"

/// <summary>
/// ゲームオーバーシーン（仮）。リトライ／タイトルへ戻る を選べる。
/// 見た目は仮UI：背景色の板と項目の箱（white1x1.png）＋ DebugText の文字（ASCIIのみ）。
/// 選択中の項目は少し大きく・明るく、非選択の項目は透明度を下げて見分けられるようにする。
/// 本番の見た目にするときは、箱と文字を画像スプライトに差し替える（カーソル操作のロジックはそのまま使える）。
/// 調整項目は GlobalVariables の "GameOver" グループ（ImGuiで編集できる。このシーンは自分の Update 冒頭で Apply する）
/// </summary>
class GameOverScene {
public:
	// シーン終了時の行き先
	enum class Result {
		kRetry,   // もう一度ゲームへ
		kToTitle, // タイトルへ戻る
	};

	~GameOverScene();

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
	// 選ばれた行き先
	Result GetResult() const { return result_; }

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

	// メニュー項目の数（リトライ／タイトルへ）
	static const int32_t kItemCount = 2;

	// 背景の板（画面全体。色は調整項目）
	KamataEngine::Sprite* backgroundSprite_ = nullptr;
	// 項目の背景の箱（リトライ／タイトルへ）。1項目に1つずつ持つ
	KamataEngine::Sprite* itemSprites_[kItemCount] = {};
	// フェード
	Fade fade_;

	// シーンの進行状態
	Phase phase_ = Phase::kFadeIn;
	// シーンが終了したか
	bool isFinished_ = false;
	// 選ばれた行き先
	Result result_ = Result::kToTitle;
	// カーソル位置（0=リトライ、1=タイトルへ）
	int32_t cursorIndex_ = 0;
	// 前フレームの上下入力（押しっぱなしでカーソルが連続移動しないようにする）
	float previousUiMoveY_ = 0.0f;

	// --- 調整項目（GlobalVariables の "GameOver" グループ） ---
	// フェードの時間（秒）
	static inline float kFadeDuration = 0.7f;
	// 背景色（RGB 0～1）
	static inline KamataEngine::Vector3 kBackgroundColor = {0.18f, 0.05f, 0.06f};
	// 見出し（GAME OVER）の中心位置（ピクセル。zは未使用）と文字の倍率
	static inline KamataEngine::Vector3 kTitlePosition = {960.0f, 330.0f, 0.0f};
	static inline float kTitleScale = 7.0f;
	// 1つ目の項目の中心位置と、項目の縦の間隔
	static inline KamataEngine::Vector3 kItemPosition = {960.0f, 620.0f, 0.0f};
	static inline float kItemSpacing = 150.0f;
	// 項目の箱の大きさ（x=幅・y=高さ。zは未使用）と文字の倍率
	static inline KamataEngine::Vector3 kItemSize = {560.0f, 110.0f, 0.0f};
	static inline float kLabelScale = 4.0f;
	// 箱の色（選択中／非選択。RGB 0～1）
	static inline KamataEngine::Vector3 kSelectedColor = {0.85f, 0.3f, 0.25f};
	static inline KamataEngine::Vector3 kUnselectedColor = {0.3f, 0.2f, 0.2f};
	// 選択中の項目の追加拡大率と、非選択の項目の透明度
	static inline float kSelectedScale = 1.1f;
	static inline float kUnselectedAlpha = 0.6f;
};
