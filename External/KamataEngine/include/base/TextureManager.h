#pragma once

#include <array>
#include <d3dx12.h>
#include <string>
#include <unordered_map>
#include <wrl.h>
#include <memory>

namespace KamataEngine {

/// <summary>
/// テクスチャマネージャ
/// </summary>
class TextureManager {
public:
	// デスクリプターの数
	static const size_t kNumDescriptors = 1024;

	/// <summary>
	/// テクスチャ
	/// </summary>
	struct Texture {
		// テクスチャリソース
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		// デスクリプタインデックス
		uint32_t descriptorIndex = 0;
		// 名前
		std::string name;
	};

	/// <summary>
	/// 読み込み
	/// </summary>
	/// <param name="fileName">ファイル名</param>
	/// <returns>テクスチャハンドル</returns>
	static uint32_t Load(const std::string& fileName);

	/// <summary>
	/// 読み込み解除
	/// </summary>
	/// <param name="textureHandle">テクスチャハンドル</param>
	static bool Unload(uint32_t textureHandle);

	/// <summary>
	/// シングルトンインスタンスの取得
	/// </summary>
	/// <returns>シングルトンインスタンス</returns>
	static TextureManager* GetInstance();

	/// <summary>
	/// 終了処理
	/// </summary>
	static void Terminate();

	/// <summary>
	/// システム初期化
	/// </summary>
	/// <param name="directoryPath">テクスチャを読み込むディレクトリ</param>
	void Initialize(std::string directoryPath = "Resources/");

	/// <summary>
	/// 全テクスチャリセット
	/// </summary>
	void ResetAll();

	/// <summary>
	/// リソース情報取得
	/// </summary>
	/// <param name="textureHandle">テクスチャハンドル</param>
	/// <returns>リソース情報</returns>
	const D3D12_RESOURCE_DESC GetResourceDesc(uint32_t textureHandle);

	/// <summary>
	/// デスクリプタテーブルをセット
	/// </summary>
	/// <param name="commandList">コマンドリスト</param>
	/// <param name="rootParamIndex">ルートパラメータ番号</param>
	/// <param name="textureHandle">テクスチャハンドル</param>
	void SetGraphicsRootDescriptorTable(ID3D12GraphicsCommandList* commandList, UINT rootParamIndex, uint32_t textureHandle);

	/// <summary>
	/// デスクリプタインデックスを取得
	/// </summary>
	/// <param name="textureHandle">テクスチャハンドル</param>
	/// <returns>デスクリプタインデックス</returns>
	uint32_t GetDescriptorIndex(uint32_t textureHandle);

private:
	TextureManager(const TextureManager&) = delete;
	const TextureManager& operator=(const TextureManager&) = delete;

	static std::unique_ptr<TextureManager> sInstance_;

public:
	struct Passkey {
	private:
		friend TextureManager;
		Passkey() = default;
	};

	TextureManager(Passkey);

private:
	friend std::default_delete<TextureManager>;
	TextureManager() = default;
	~TextureManager() = default;

	// C++のbitsetが内部詳細にアクセスできないのでfindFirst用に自作
	template<size_t kNumberOfBits> class Bitset {

	public:
		Bitset();
		size_t FindFirst() const;
		void Set(size_t bitIndex, bool value = true);
		void Reset();
		void Reset(size_t bitIndex);
		bool Test(size_t bitIndex) const;

	private:
		uint64_t& GetWord(size_t bitIndex);
		const uint64_t& GetWord(size_t bitIndex) const;

	private:
		static constexpr size_t kCountOfWord = (kNumberOfBits == 0 ? 1 : ((kNumberOfBits - 1) / (8 * sizeof(uint64_t)) + 1));
		static constexpr size_t kBitsPerWord = 8 * sizeof(uint64_t);
		static constexpr size_t kBitsPerWordMask = kBitsPerWord - 1;
		static constexpr size_t kBitIndexToWordIndex = 6;

		uint64_t words_[kCountOfWord];
	};

	// デバイス
	ID3D12Device* device_;
	// ディレクトリパス
	std::string directoryPath_;
	// テクスチャコンテナ
	std::array<Texture, kNumDescriptors> textures_;
	Bitset<kNumDescriptors> useTable_;
	// アップロードリソース
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> intermediateResources;
	// 転送用コマンドリスト
	ID3D12GraphicsCommandList* commandListforTransfer_;

	/// <summary>
	/// 読み込み
	/// </summary>
	/// <param name="fileName">ファイル名</param>
	uint32_t LoadInternal(const std::string& fileName);

	/// <summary>
	/// 読み込み解除
	/// </summary>
	/// <param name="textureHandle">テクスチャハンドル</param>
	bool UnloadInternal(uint32_t textureHandle);
};

} // namespace KamataEngine