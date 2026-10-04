#pragma once
#include "KamataEngine.h"

/// <summary>
/// フェード（最前面に黒いスプライトを重ね、画面全体を 黒→透明・透明→黒 に変化させる。前作から流用）
/// </summary>
class Fade {
public:
	// フェードの状態
	enum class Status {
		kNone,    // フェードなし
		kFadeIn,  // フェードイン中（黒→透明）
		kFadeOut, // フェードアウト中（透明→黒）
	};

	~Fade();

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

	/// <summary>
	/// フェード開始
	/// </summary>
	/// <param name="status">フェードイン or フェードアウト</param>
	/// <param name="duration">フェードにかける時間（秒）</param>
	void Start(Status status, float duration);

	/// <summary>
	/// フェード停止（状態を kNone に戻す）
	/// </summary>
	void Stop();

	/// <summary>
	/// 今のフェードが終了したか
	/// </summary>
	bool IsFinished() const;

	/// <summary>
	/// 現在の状態を取得
	/// </summary>
	Status GetStatus() const { return status_; }

private:
	// テクスチャハンドル
	uint32_t textureHandle_ = 0;
	// 画面全体を覆う黒い板スプライト
	KamataEngine::Sprite* sprite_ = nullptr;

	// 現在の状態
	Status status_ = Status::kNone;
	// フェードの持続時間（秒）
	float duration_ = 0.0f;
	// 経過時間カウンター（秒）
	float counter_ = 0.0f;
};
