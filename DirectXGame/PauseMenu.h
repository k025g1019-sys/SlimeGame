#pragma once
#include "KamataEngine.h"

/// <summary>
/// ポーズメニュー（メニューボタンで開く）。ゲームに戻る／タイトルへ戻る を選べる。
/// 見た目は仮UI：白い1pxテクスチャ（white1x1.png）を色付きの箱に引き伸ばし、文字は DebugText（ASCIIのみ）で出す。
/// 選択中の項目は少し大きく・明るく、非選択の項目は透明度を下げて見分けられるようにする。
/// 本番の見た目にするときは、箱と文字を画像スプライトに差し替える（カーソル操作のロジックはそのまま使える）。
/// 配置・大きさ・強調の調整は GlobalVariables の "Pause" グループ（ImGuiで編集できる）
/// </summary>
class PauseMenu {
public:
	// メニューで選ばれた操作
	enum class Choice {
		kNone,    // まだ何も選ばれていない
		kResume,  // つづける（メニューを閉じる）
		kToTitle, // タイトルへ戻る
	};

	~PauseMenu();

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// メニューを開く
	/// </summary>
	void Open();

	/// <summary>
	/// メニューを閉じる
	/// </summary>
	void Close() { isOpen_ = false; }

	// メニューが開いているか（開いている間、GameScene はゲームの更新を止める）
	bool IsOpen() const { return isOpen_; }

	/// <summary>
	/// 更新（開いている間のカーソル操作と決定）
	/// </summary>
	/// <returns>このフレームに選ばれた操作（決定していなければ kNone）</returns>
	Choice Update();

	/// <summary>
	/// 描画（スプライト描画パスの中で呼ぶ。文字を箱より手前に出すため、中で DebugText::DrawAll も呼ぶ）
	/// </summary>
	void Draw();

	// 調整項目を登録（既定値をGlobalVariablesへ追加する）
	static void RegisterGlobalVariables();
	// 調整項目を適用（GlobalVariablesの現在値を静的メンバへ反映する）
	static void ApplyGlobalVariables();

private:
	// メニュー項目の数（ゲームに戻る／タイトルへ）
	static const int32_t kItemCount = 2;

	// 上下入力の「入れた瞬間」判定（押しっぱなしでカーソルが連続移動しないようにする）
	int32_t GetCursorMove();
	// 項目の箱の位置・大きさ・色をカーソル位置と調整項目から反映する
	void UpdateLayout();

	// 画面を暗くするオーバーレイ
	KamataEngine::Sprite* overlaySprite_ = nullptr;
	// 項目の背景の箱（ゲームに戻る／タイトルへ）。1項目に1つずつ持つ
	KamataEngine::Sprite* itemSprites_[kItemCount] = {};
	// メニューが開いているか
	bool isOpen_ = false;
	// カーソル位置（0=ゲームに戻る、1=タイトルへ）
	int32_t cursorIndex_ = 0;
	// 前フレームの上下入力
	float previousUiMoveY_ = 0.0f;

	// --- 調整項目（GlobalVariables の "Pause" グループ） ---
	// 見出し（PAUSE）の中心位置（ピクセル。zは未使用）と文字の倍率
	static inline KamataEngine::Vector3 kTitlePosition = {960.0f, 330.0f, 0.0f};
	static inline float kTitleScale = 6.0f;
	// 1つ目の項目の中心位置（ピクセル。zは未使用）と、項目の縦の間隔
	static inline KamataEngine::Vector3 kItemPosition = {960.0f, 560.0f, 0.0f};
	static inline float kItemSpacing = 150.0f;
	// 項目の箱の大きさ（x=幅・y=高さ。zは未使用）と文字の倍率
	static inline KamataEngine::Vector3 kItemSize = {560.0f, 110.0f, 0.0f};
	static inline float kLabelScale = 4.0f;
	// 箱の色（選択中／非選択。RGB 0～1）
	static inline KamataEngine::Vector3 kSelectedColor = {0.25f, 0.55f, 0.95f};
	static inline KamataEngine::Vector3 kUnselectedColor = {0.3f, 0.3f, 0.35f};
	// 選択中の項目の追加拡大率と、非選択の項目の透明度
	static inline float kSelectedScale = 1.1f;
	static inline float kUnselectedAlpha = 0.6f;
};
