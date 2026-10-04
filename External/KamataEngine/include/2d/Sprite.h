#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <math\Matrix4x4.h>
#include <math\Vector2.h>
#include <math\Vector3.h>
#include <math\Vector4.h>
#include <array>
#include <string>
#include <wrl.h>

namespace KamataEngine {

/// <summary>
/// スプライト
/// </summary>
class Sprite {
public:
	enum class BlendMode {
		kNone,      //!< ブレンドなし
		kNormal,    //!< 通常αブレンド。デフォルト。 Src * SrcA + Dest * (1 - SrcA)
		kAdd,       //!< 加算。Src * SrcA + Dest * 1
		kSubtract,  //!< 減算。Dest * 1 - Src * SrcA
		kMultiply,  //!< 乗算。Src * 0 + Dest * Src
		kScreen,    //!< スクリーン。Src * (1 - Dest) + Dest * 1
		kExclusion, //!< 除外。(1 - Dest) * Src + (1 - Src) * Dest

		kCountOfBlendMode,    //!< ブレンドモード数。指定はしない
	};

public: // サブクラス
	/// <summary>
	/// 頂点データ構造体
	/// </summary>
	struct VertexPosUv {
		Vector3 pos; // xyz座標
		Vector2 uv;  // uv座標
	};

	/// <summary>
	/// 定数バッファ用データ構造体
	/// </summary>
	struct ConstBufferData {
		Matrix4x4 mat; // ３Ｄ変換行列
		Vector2 uvOffset;         // UVオフセット
		Vector2 uvScale;          // UVスケール
		Vector4 color; // 色 (RGBA)
		uint32_t textureDescriptorIndex; // デスクリプタインデックス
	};

public: // 静的メンバ関数
	/// <summary>
	/// インスタンスカウントをリセットする (毎フレームの開始時に呼び出す)
	/// </summary>
	static void ResetInstanceCount();

	/// <summary>
	/// 静的初期化
	/// </summary>
	/// <param name="device">デバイス</param>
	/// <param name="window_width">画面幅</param>
	/// <param name="window_height">画面高さ</param>
	static void StaticInitialize(ID3D12Device* device, int window_width, int window_height, const std::wstring& directoryPath = L"");

	/// <summary>
	/// 描画前処理
	/// </summary>
	/// <param name="cmdList">描画コマンドリスト</param>
	static void PreDraw(ID3D12GraphicsCommandList* cmdList = nullptr, BlendMode blendMode = BlendMode::kNormal);

	/// <summary>
	/// 描画後処理
	/// </summary>
	static void PostDraw();

	/// <summary>
	/// スプライト生成
	/// </summary>
	/// <param name="texNumber">テクスチャハンドル</param>
	/// <param name="position">座標</param>
	/// <param name="color">色</param>
	/// <param name="anchorpoint">アンカーポイント</param>
	/// <param name="isFlipX">左右反転</param>
	/// <param name="isFlipY">上下反転</param>
	/// <returns>生成されたスプライト</returns>
	static Sprite* Create(uint32_t textureHandle, Vector2 position, Vector4 color = {1, 1, 1, 1}, Vector2 anchorpoint = {0.0f, 0.0f}, bool isFlipX = false, bool isFlipY = false);

private: // 静的メンバ変数
	// 頂点数
	static const int kVertNum = 4;
	// デバイス
	static ID3D12Device* sDevice_;
	// コマンドリスト
	static ID3D12GraphicsCommandList* sCommandList_;
	// 射影行列
	static Matrix4x4 sMatProjection_;
	// パイプラインステート（キャッシュ）
	static std::array<ID3D12PipelineState*, static_cast<size_t>(BlendMode::kCountOfBlendMode)> sPipelineStates_;
	// ルートシグネチャ（キャッシュ）
	static std::array<ID3D12RootSignature*, static_cast<size_t>(BlendMode::kCountOfBlendMode)> sRootSignatures_;

	// 共通頂点バッファ
	static Microsoft::WRL::ComPtr<ID3D12Resource> sVertexBuffer_;
	static D3D12_VERTEX_BUFFER_VIEW sVbView_;
	// インスタンスバッファ
	static const uint32_t kMaxInstanceCount = 1024;
	static Microsoft::WRL::ComPtr<ID3D12Resource> sInstanceBuffer_;
	static ConstBufferData* sInstanceMap_;
	static uint32_t sInstanceCount_;
	// 現在のブレンドモード
	static BlendMode sBlendMode_;
	// インスタンスバッファの書き込みオフセット
	static uint32_t sInstanceIndexOffset_;

public: // メンバ関数
	/// <summary>
	/// 初期化
	/// </summary>
	/// <returns>成否</returns>
	bool Initialize();

	/// <summary>
	/// テクスチャハンドルの設定
	/// </summary>
	/// <param name="textureHandle">テクスチャハンドル</param>
	void SetTextureHandle(uint32_t textureHandle);

	uint32_t GetTextureHandle() const { return textureHandle_; }

	/// <summary>
	/// 座標の設定
	/// </summary>
	/// <param name="position">座標</param>
	void SetPosition(const Vector2& position);

	const Vector2& GetPosition() const { return position_; }

	/// <summary>
	/// 角度の設定
	/// </summary>
	/// <param name="rotation">角度</param>
	void SetRotation(float rotation);

	float GetRotation() const { return rotation_; }

	/// <summary>
	/// サイズの設定
	/// </summary>
	/// <param name="size">サイズ</param>
	void SetSize(const Vector2& size);

	const Vector2& GetSize() const { return size_; }

	/// <summary>
	/// アンカーポイントの設定
	/// </summary>
	/// <param name="anchorpoint">アンカーポイント</param>
	void SetAnchorPoint(const Vector2& anchorpoint);

	const Vector2& GetAnchorPoint() const { return anchorPoint_; }

	/// <summary>
	/// 色の設定
	/// </summary>
	/// <param name="color">色</param>
	void SetColor(const Vector4& color) { color_ = color; };

	const Vector4& GetColor() const { return color_; }

	/// <summary>
	/// 左右反転の設定
	/// </summary>
	/// <param name="isFlipX">左右反転</param>
	void SetIsFlipX(bool isFlipX);

	bool GetIsFlipX() const { return isFlipX_; }

	/// <summary>
	/// 上下反転の設定
	/// </summary>
	/// <param name="isFlipX">上下反転</param>
	void SetIsFlipY(bool isFlipY);

	bool GetIsFlipY() const { return isFlipY_; }

	/// <summary>
	/// テクスチャ範囲設定
	/// </summary>
	/// <param name="texBase">テクスチャ左上座標</param>
	/// <param name="texSize">テクスチャサイズ</param>
	void SetTextureRect(const Vector2& texBase, const Vector2& texSize);

	/// <summary>
	/// 描画
	/// </summary>
	void Draw();

private: // メンバ変数
	// 定数バッファマップ
	// ConstBufferData* constMap_ = nullptr;
	// テクスチャ番号
	UINT textureHandle_ = 0;
	// Z軸回りの回転角
	float rotation_ = 0.0f;
	// 座標
	Vector2 position_{};
	// スプライト幅、高さ
	Vector2 size_ = {100.0f, 100.0f};
	// アンカーポイント
	Vector2 anchorPoint_ = {0, 0};
	// ワールド行列
	Matrix4x4 matWorld_{};
	// 色
	Vector4 color_ = {1, 1, 1, 1};
	// 左右反転
	bool isFlipX_ = false;
	// 上下反転
	bool isFlipY_ = false;
	// テクスチャ始点
	Vector2 texBase_ = {0, 0};
	// テクスチャ幅、高さ
	Vector2 texSize_ = {100.0f, 100.0f};
	// リソース設定
	D3D12_RESOURCE_DESC resourceDesc_;

private: // メンバ関数
	/// <summary>
	/// コンストラクタ
	/// </summary>
	Sprite();
	Sprite(uint32_t textureHandle, Vector2 position, Vector2 size, Vector4 color, Vector2 anchorpoint, bool isFlipX, bool isFlipY);
};

} // namespace KamataEngine