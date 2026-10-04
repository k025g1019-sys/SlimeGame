#pragma once

#include <Windows.h>
#include <chrono>
#include <cstdlib>
#include <d3d12.h>
#include <d3dx12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <string>

#include "WinApp.h"
#include <memory>
#include <vector>

namespace KamataEngine {

class DescriptorHeapManager;
class GraphicsPipelineManager;
class ShaderManager;

/// <summary>
/// DirectX汎用
/// </summary>
class DirectXCommon {
public: // メンバ関数
	/// <summary>
	/// シングルトンインスタンスの取得
	/// </summary>
	/// <returns></returns>
	static DirectXCommon* GetInstance();

	/// <summary>
	/// 終了処理
	/// </summary>
	static void Terminate();

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(int32_t backBufferWidth = WinApp::kWindowWidth, int32_t backBufferHeight = WinApp::kWindowHeight, bool enableDebugLayer = false);

	/// <summary>
	/// 描画前処理
	/// </summary>
	void PreDraw();

	/// <summary>
	/// 描画後処理
	/// </summary>
	void PostDraw();

	/// <summary>
	/// レンダーターゲットのクリア
	/// </summary>
	void ClearRenderTarget();

	/// <summary>
	/// 深度バッファのクリア
	/// </summary>
	void ClearDepthBuffer();

	/// <summary>
	/// 初期化済みチェック
	/// </summary>
	/// <returns>初期化済みフラグ</returns>
	bool IsInitialized() const { return initialized_; }

	/// <summary>
	/// デバイスの取得
	/// </summary>
	/// <returns>デバイス</returns>
	ID3D12Device* GetDevice() const { return device_.Get(); }

	/// <summary>
	/// 転送用コマンドリストの取得
	/// </summary>
	/// <returns>転送用コマンドリスト</returns>
	ID3D12GraphicsCommandList* GetCommandListForTransfer() const { return commandListForTransfer_.Get(); }

	/// <summary>
	/// 描画コマンドリストの取得
	/// </summary>
	/// <returns>描画コマンドリスト</returns>
	ID3D12GraphicsCommandList* GetCommandList() const { return commandListForRender_.Get(); }

	/// <summary>
	/// バックバッファの幅取得
	/// </summary>
	/// <returns>バックバッファの幅</returns>
	int32_t GetBackBufferWidth() const;

	/// <summary>
	/// バックバッファの高さ取得
	/// </summary>
	/// <returns>バックバッファの高さ</returns>
	int32_t GetBackBufferHeight() const;

	// バックバッファの数を取得
	size_t GetBackBufferCount() const { return backBuffers_.size(); }

	void SetRenderTargets(bool sRGB);

	/// <summary>
	/// 転送用リソースポインタの追加
	/// </summary>
	/// <returns>追加された転送用リソースポインタ</returns>
	Microsoft::WRL::ComPtr<ID3D12Resource>& AddResourcePointerForTransfer();

	DescriptorHeapManager* GetDescriptorHeapManager() const { return descriptorHeapManager_.get(); }
	GraphicsPipelineManager* GetGraphicsPipelineManager() const { return graphicsPipelineManager_.get(); }
	ShaderManager* GetShaderManager() const { return shaderManager_.get(); }



private: // メンバ変数
	// ウィンドウズアプリケーション管理
	WinApp* winApp_;

	bool initialized_ = false;

	// Direct3D関連
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;
	Microsoft::WRL::ComPtr<ID3D12Device> device_;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandListForTransfer_;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandListForRender_;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocatorForTransfer_;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocatorForRender_;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> backBuffers_;
	Microsoft::WRL::ComPtr<ID3D12Resource> depthBuffer_;
	std::vector<uint32_t> rtvIndices_;
	uint32_t dsvIndex_ = 0;
	Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> resourcesForTransfer;
	UINT64 fenceVal_ = 0;
	int32_t backBufferWidth_ = 0;
	int32_t backBufferHeight_ = 0;
	
	// 内部マネージャ
	std::unique_ptr<DescriptorHeapManager> descriptorHeapManager_;
	std::unique_ptr<GraphicsPipelineManager> graphicsPipelineManager_;
	std::unique_ptr<ShaderManager> shaderManager_;

	HANDLE frameLatencyWaitableObject_;
	std::chrono::steady_clock::time_point reference_;
	int32_t refreshRate_ = 0;


private: // メンバ関数
	DirectXCommon(const DirectXCommon&) = delete;
	const DirectXCommon& operator=(const DirectXCommon&) = delete;

	static std::unique_ptr<DirectXCommon> sInstance_;

public:
	struct Passkey {
	private:
		friend DirectXCommon;
		Passkey() = default;
	};

	DirectXCommon(Passkey);

private: // メンバ関数
	friend std::default_delete<DirectXCommon>;
	DirectXCommon() = default;
	~DirectXCommon() = default;

	/// <summary>
	/// DXGIデバイス初期化
	/// </summary>
	void InitializeDXGIDevice(bool enableDebugLayer = false);

	/// <summary>
	/// スワップチェーンの生成
	/// </summary>
	void CreateSwapChain();

	/// <summary>
	/// コマンド関連初期化
	/// </summary>
	void InitializeCommand();

	/// <summary>
	/// レンダーターゲット生成
	/// </summary>
	void CreateFinalRenderTargets();

	/// <summary>
	/// 深度バッファ生成
	/// </summary>
	void CreateDepthBuffer();

	/// <summary>
	/// フェンス生成
	/// </summary>
	void CreateFence();

	/// <summary>
	/// DXC関連初期化
	/// </summary>
	void InitializeDXC();
};

} // namespace KamataEngine