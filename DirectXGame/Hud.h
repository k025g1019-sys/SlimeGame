#pragma once
#include "KamataEngine.h"
#include <array>
#include <string>

/// <summary>
/// HUD（2D）。素材が無いので white1x1.png を色付きの矩形に引き伸ばして使い、文字は DebugText（ASCIIのみ）。
/// ・左上：HP の四角（緑。減った分は暗い）
/// ・上中央：「PHASE 2/6  BOSS」とフェーズ名。ボスフェーズ中はボスのHPバー
/// ・画面中央のバナー：PHASE n / WARNING / CLEAR! / GAME CLEAR など（DebugText を拡大。出た瞬間に少し大きく）。
/// 　ボスフェーズのバナーは画面中央のボスに重ならないよう、上寄せ・小さめ・薄い帯（kBossBanner～）
/// ・下部：操作説明（爆弾は通常状態で跳ね返す、も書く）
/// スプライトは1フレーム1024個まで（DebugText の1文字も1個）。ここで使うのは 板 約26枚 ＋ 文字 約200字（フェーズ名は phases.json で24文字まで）
/// </summary>
class Hud {
public:
	// HUDに表示する内容（GameScene が毎フレーム作って渡す）
	struct Info {
		int hp = 0;
		int maxHp = 1;
		int phaseNumber = 1; // 1から
		int phaseCount = 1;
		bool isBossPhase = false;
		std::string phaseName;
		bool showBossBar = false;
		int bossHp = 0;
		int bossMaxHp = 1;
		int bossLevel = 1;
		// バナー（空文字なら出さない）と、出てからの時間（秒）
		std::string bannerText;
		std::string bannerSubText;
		float bannerTime = 0.0f;
		KamataEngine::Vector3 bannerColor = {1.0f, 1.0f, 1.0f}; // 帯の色
		// ボスの見せ場のバナー（WARNING・ボスの CLEAR!・ボス撃破の GAME CLEAR）か。
		// true なら画面中央のボスに重ならないよう、ボス用の位置・大きさ（kBossBanner～）で出す
		bool isBossBanner = false;
	};

	// HPの四角の数の上限（Player::kMaxHp はこの数までに制限される。HPの上限を変えるときはここを変える）
	static constexpr int kMaxHpPips = 20;

	~Hud();

	/// <summary>
	/// 初期化（スプライトを作る）
	/// </summary>
	void Initialize();

	/// <summary>
	/// 表示内容を反映する（毎フレーム）
	/// </summary>
	void Update(const Info& info);

	/// <summary>
	/// 描画（スプライト描画パスの中で呼ぶ。文字は予約するだけで、呼び出し側の DebugText::DrawAll で描かれる）
	/// </summary>
	void Draw();

	// 調整項目を登録（既定値をGlobalVariablesへ追加する）
	static void RegisterGlobalVariables();
	// 調整項目を適用（GlobalVariablesの現在値を静的メンバへ反映する）
	static void ApplyGlobalVariables();

	// --- 調整項目（GlobalVariables の "Hud" グループ） ---
	// HPの四角：左上の位置（ピクセル。zは未使用）・1つの大きさ・間隔・色（残り／減った分）
	static inline KamataEngine::Vector3 kHpPosition = {40.0f, 40.0f, 0.0f};
	static inline float kHpPipSize = 34.0f;
	static inline float kHpPipSpacing = 44.0f;
	static inline KamataEngine::Vector3 kHpColor = {0.3f, 0.95f, 0.4f};
	static inline KamataEngine::Vector3 kHpEmptyColor = {0.12f, 0.2f, 0.14f};
	// 上中央のフェーズ表示の高さ・文字の倍率
	static inline float kPhaseLabelY = 44.0f;
	static inline float kPhaseLabelScale = 3.0f;
	// ボスのHPバー：上端の高さ・大きさ（幅, 高さ）・色
	static inline float kBossBarY = 130.0f;
	static inline KamataEngine::Vector3 kBossBarSize = {760.0f, 24.0f, 0.0f};
	static inline KamataEngine::Vector3 kBossBarColor = {1.0f, 0.5f, 0.1f};
	// バナー：中心の高さ・文字の倍率・副題の倍率・帯の高さ・帯の不透明度・出た瞬間の拡大の時間（秒）
	static inline float kBannerY = 400.0f;
	static inline float kBannerScale = 8.0f;
	static inline float kBannerSubScale = 3.0f;
	static inline float kBannerBandHeight = 230.0f;
	static inline float kBannerBandAlpha = 0.45f;
	static inline float kBannerPopTime = 0.25f;
	// ボスの見せ場のバナー：中心の高さ（既定はボスの頭より上）・文字の倍率・帯の高さ・帯の不透明度
	static inline float kBossBannerY = 265.0f;
	static inline float kBossBannerScale = 6.0f;
	static inline float kBossBannerBandHeight = 180.0f;
	static inline float kBossBannerBandAlpha = 0.3f;
	// 操作説明の文字の倍率
	static inline float kHelpScale = 2.0f;

private:
	Info info_;
	// 左上の板・HPの四角・ボスのバー（背景・中身）・操作説明の帯・バナーの帯
	KamataEngine::Sprite* hpPanelSprite_ = nullptr;
	std::array<KamataEngine::Sprite*, kMaxHpPips> hpPipSprites_{};
	KamataEngine::Sprite* bossBarBackSprite_ = nullptr;
	KamataEngine::Sprite* bossBarFillSprite_ = nullptr;
	KamataEngine::Sprite* helpBarSprite_ = nullptr;
	KamataEngine::Sprite* bannerBandSprite_ = nullptr;
};
