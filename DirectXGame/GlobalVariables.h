#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <variant>

#include <math/Vector3.h>

// 項目（1項目分の値。いろいろな型のいずれかを保持する）
using Item = std::variant<int32_t, float, KamataEngine::Vector3, bool>;
// グループ（項目のリスト。項目は名前をキーとして管理する）
using Group = std::map<std::string, Item>;

/// <summary>
/// グローバル変数（調整項目の一元管理。前作から流用し、bool型対応を追加）。
/// 使い方：
///   1. 各クラスの RegisterGlobalVariables で AddItem（既定値。読み込み済みの値は上書きしない）
///   2. 各クラスの ApplyGlobalVariables で GetXXXValue → 静的メンバへ反映（毎フレーム呼べばImGuiの変更が即反映）
///   3. ImGui の "Global Variables" ウィンドウで編集し、グループごとの Save ボタンでJSON保存
/// 保存先：Resources/GameData/GlobalVariables/＜グループ名＞.json
/// </summary>
class GlobalVariables {
public:
	/// <summary>
	/// インスタンスの取得
	/// </summary>
	/// <returns>インスタンス</returns>
	static GlobalVariables* GetInstance();

	/// <summary>
	/// グループの作成
	/// </summary>
	/// <param name="groupName">グループ名</param>
	void CreateGroup(const std::string& groupName);

	// 値のセット(int)
	void SetValue(const std::string& groupName, const std::string& key, int32_t value);
	// 値のセット(float)
	void SetValue(const std::string& groupName, const std::string& key, float value);
	// 値のセット(Vector3)
	void SetValue(const std::string& groupName, const std::string& key, const KamataEngine::Vector3& value);
	// 値のセット(bool)
	void SetValue(const std::string& groupName, const std::string& key, bool value);

	// 項目の追加(int)
	void AddItem(const std::string& groupName, const std::string& key, int32_t value);
	// 項目の追加(float)
	void AddItem(const std::string& groupName, const std::string& key, float value);
	// 項目の追加(Vector3)
	void AddItem(const std::string& groupName, const std::string& key, const KamataEngine::Vector3& value);
	// 項目の追加(bool)
	void AddItem(const std::string& groupName, const std::string& key, bool value);

	// 値の取得(int)
	int32_t GetIntValue(const std::string& groupName, const std::string& key) const;
	// 値の取得(float)
	float GetFloatValue(const std::string& groupName, const std::string& key) const;
	// 値の取得(Vector3)
	KamataEngine::Vector3 GetVector3Value(const std::string& groupName, const std::string& key) const;
	// 値の取得(bool)
	bool GetBoolValue(const std::string& groupName, const std::string& key) const;

	/// <summary>
	/// 毎フレーム処理（ImGuiで全グループ・全項目を表示/編集）
	/// </summary>
	void Update();

	/// <summary>
	/// ファイルに書き出し
	/// </summary>
	/// <param name="groupName">グループ</param>
	void SaveFile(const std::string& groupName);

	/// <summary>
	/// ディレクトリの全ファイル読み込み
	/// </summary>
	void LoadFiles();

	/// <summary>
	/// ファイルから読み込む
	/// </summary>
	/// <param name="groupName">グループ</param>
	void LoadFile(const std::string& groupName);

private:
	GlobalVariables() = default;
	~GlobalVariables() = default;
	GlobalVariables(const GlobalVariables&) = delete;
	GlobalVariables& operator=(const GlobalVariables&) = delete;

	// 全データ
	std::map<std::string, Group> datas_;
};
