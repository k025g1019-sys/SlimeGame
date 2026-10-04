#pragma once
#include <memory>

#ifdef USE_IMGUI
#include <d3d12.h>
#include <imgui.h>
#include <wrl.h>
#endif

namespace KamataEngine {

class WinApp;
class DirectXCommon;

class ImGuiManager {
public:
	static ImGuiManager* GetInstance();

	/// <summary>
	/// 終了処理
	/// </summary>
	static void Terminate();
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// 終了
	/// </summary>
	void Finalize();

	/// <summary>
	/// ImGui受付開始
	/// </summary>
	void Begin();

	/// <summary>
	/// ImGui受付終了
	/// </summary>
	void End();

	/// <summary>
	/// 描画
	/// </summary>
	void Draw();

private:
#ifdef USE_IMGUI
	// DirectX基盤インスタンス（借りてくる）
	DirectXCommon* dxCommon_ = nullptr;
	// デスクリプタインデックス
	uint32_t descriptorIndex_ = 0;
#endif
private:
	ImGuiManager(const ImGuiManager&) = delete;
	const ImGuiManager& operator=(const ImGuiManager&) = delete;

	static std::unique_ptr<ImGuiManager> sInstance_;

public:
	struct Passkey {
	private:
		friend ImGuiManager;
		Passkey() = default;
	};

	ImGuiManager(Passkey);

private:
	friend std::default_delete<ImGuiManager>;
	ImGuiManager() = default;
	~ImGuiManager();
};

} // namespace KamataEngine