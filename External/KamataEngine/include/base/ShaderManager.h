#pragma once
#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>

namespace KamataEngine {

/// <summary>
/// シェーダーマネージャ (内部用)
/// </summary>
class ShaderManager {
public:
	ShaderManager() = default;
	~ShaderManager() = default;
	ShaderManager(const ShaderManager&) = delete;
	ShaderManager& operator=(const ShaderManager&) = delete;

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// シェーダーのコンパイルと登録
	/// </summary>
	/// <param name="name">登録名</param>
	/// <param name="filePath">HLSLファイルパス</param>
	/// <param name="profile">シェーダープロファイル (vs_6_0, ps_6_0等)</param>
	void Compile(const std::string& name, const std::wstring& filePath, const wchar_t* profile);

	/// <summary>
	/// コンパイル済みバイナリ(.cso)のロード
	/// </summary>
	/// <param name="name">登録名</param>
	/// <param name="filePath">csoファイルパス</param>
	void LoadCSO(const std::string& name, const std::wstring& filePath);

	/// <summary>
	/// シェーダーバイナリの取得
	/// </summary>
	/// <param name="name">登録名</param>
	/// <returns>シェーダーバイナリ</returns>
	IDxcBlob* GetBlob(const std::string& name) const;

	/// <summary>
	/// ベースパスの設定
	/// </summary>
	void SetBaseDirectory(const std::wstring& path) { baseDirectory_ = path; }

private:
	Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
	Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<IDxcBlob>> shaders_;
	std::wstring baseDirectory_ = L"./Resources/EngineShaders/";
};

} // namespace KamataEngine
