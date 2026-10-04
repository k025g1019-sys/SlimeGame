#pragma once

#include <3d\Camera.h>
#include <base\DirectXCommon.h>
#include <3d\Model.h>
#include <3d\WorldTransform.h>
#include <memory>
#include <string>

namespace KamataEngine {

// 軸方向表示
class AxisIndicator {
public:
	// ビューポート矩形範囲
	static const float kViewPortTopLeftX;
	static const float kViewPortTopLeftY;
	static const float kViewPortWidth;
	static const float kViewPortHeight;
	static const float kCameraDistance;

	// モデル名
	static const std::string kModelName;

	/// <summary>
	/// シングルトンインスタンスの取得
	/// </summary>
	/// <returns>シングルトンインスタンス</returns>
	static AxisIndicator* GetInstance();

	/// <summary>
	/// 終了処理
	/// </summary>
	static void Terminate();

	/// <summary>
	/// トレースするカメラのセット
	/// </summary>
	/// <param name="targetCamera">トレースするカメラ</param>
	static void SetTargetCamera(const Camera* targetCamera);

	/// <summary>
	/// 表示フラグのセット
	/// </summary>
	/// <param name="isVisible">表示フラグ</param>
	static void SetVisible(bool isVisible);

	/// /// <summary>
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

private:
	AxisIndicator& operator=(const AxisIndicator&) = delete;

	static std::unique_ptr<AxisIndicator> sInstance_;

public:
	struct Passkey {
	private:
		friend AxisIndicator;
		Passkey() = default;
	};

	AxisIndicator(Passkey);

private:
	friend std::default_delete<AxisIndicator>;
	AxisIndicator() = default;
	~AxisIndicator() = default;

	// DirectX基盤
	DirectXCommon* dxCommon_ = nullptr;
	// モデル
	std::unique_ptr<Model> model_;
	// カメラ
	Camera camera_;
	// ワールドトランスフォーム
	WorldTransform worldTransform_;
	// トレースするカメラ
	const Camera* targetCamera_ = nullptr;
	// 表示フラグ
	bool isVisible_ = false;
};

} // namespace KamataEngine