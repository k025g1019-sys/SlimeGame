#include "GlobalVariables.h"

#define NOMINMAX
#include <Windows.h>

#include <nlohmann/json.hpp>

#include <cassert>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <iomanip>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

using namespace KamataEngine;

using json = nlohmann::json;

namespace {
// グローバル変数の保存先ファイルパス
const std::string kDirectoryPath = "Resources/GameData/GlobalVariables/";

// 値を、登録済みの項目と同じ型（expected の型）にそろえる（戻り値：そろえられたか）。
// JSONは 1 と 1.0 を別の型（整数・小数）として読むため、手で 1 と書いた小数の項目や、
// コード側で int↔float を変えた項目がそのままだと Get～Value の std::get が例外を投げてゲームごと落ちる。
// 整数⇔小数は変換し、それ以外の型違い（Vector3 の項目に数値が入っている等）は変換できないので false を返す
bool ConvertItemType(const Item& value, const Item& expected, Item* out) {
	if (value.index() == expected.index()) {
		*out = value;
		return true;
	}
	if (std::holds_alternative<float>(expected)) {
		if (const int32_t* intValue = std::get_if<int32_t>(&value)) {
			*out = static_cast<float>(*intValue);
			return true;
		}
	}
	if (std::holds_alternative<int32_t>(expected)) {
		if (const float* floatValue = std::get_if<float>(&value)) {
			*out = static_cast<int32_t>(std::lround(*floatValue));
			return true;
		}
	}
	return false;
}

// 項目の追加の共通処理。未登録なら既定値で追加する（読み込み済みの値は上書きしない）。
// 読み込み済みの値の型が登録する型と違うときは型をそろえ、そろえられなければ既定値に戻す
void AddItemImpl(const std::string& groupName, Group& group, const std::string& key, const Item& defaultValue) {
	auto it = group.find(key);
	if (it == group.end()) {
		group[key] = defaultValue;
		return;
	}
	Item converted;
	if (ConvertItemType(it->second, defaultValue, &converted)) {
		it->second = converted;
	} else {
		// 型が合わないJSONの値は使えないので既定値にする（出力ウィンドウに知らせる。Saveすると正しい型で保存し直される）
		OutputDebugStringA(std::format("GlobalVariables: {}.{} has a wrong type in the json. default value is used.\n", groupName, key).c_str());
		it->second = defaultValue;
	}
}

// JSONの値を項目の値に変換する（戻り値：対応している型だったか）
bool JsonToItem(const json& value, Item* out) {
	// bool型（is_number_integerより先に判定する。jsonのtrue/falseは整数ではないが順序を明確にしておく）
	if (value.is_boolean()) {
		*out = value.get<bool>();
		return true;
	}
	// int32_t型
	if (value.is_number_integer()) {
		*out = value.get<int32_t>();
		return true;
	}
	// float型
	if (value.is_number_float()) {
		*out = static_cast<float>(value.get<double>());
		return true;
	}
	// 要素数3の数値の配列はVector3型
	if (value.is_array() && value.size() == 3 && value.at(0).is_number() && value.at(1).is_number() && value.at(2).is_number()) {
		*out = Vector3{value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>()};
		return true;
	}
	return false;
}
} // namespace

GlobalVariables* GlobalVariables::GetInstance() {
	static GlobalVariables instance;
	return &instance;
}

void GlobalVariables::CreateGroup(const std::string& groupName) {
	// 指定名のグループがなければ追加する
	datas_[groupName];
}

void GlobalVariables::SetValue(const std::string& groupName, const std::string& key, int32_t value) { datas_[groupName][key] = value; }

void GlobalVariables::SetValue(const std::string& groupName, const std::string& key, float value) { datas_[groupName][key] = value; }

void GlobalVariables::SetValue(const std::string& groupName, const std::string& key, const Vector3& value) { datas_[groupName][key] = value; }

void GlobalVariables::SetValue(const std::string& groupName, const std::string& key, bool value) { datas_[groupName][key] = value; }

void GlobalVariables::AddItem(const std::string& groupName, const std::string& key, int32_t value) {
	// 項目が未登録のときだけ既定値として追加する（読み込み済みの値は上書きしない。型が違えばそろえる）
	AddItemImpl(groupName, datas_[groupName], key, Item(std::in_place_type<int32_t>, value));
}

void GlobalVariables::AddItem(const std::string& groupName, const std::string& key, float value) {
	// 項目が未登録のときだけ既定値として追加する（読み込み済みの値は上書きしない。型が違えばそろえる）
	AddItemImpl(groupName, datas_[groupName], key, Item(std::in_place_type<float>, value));
}

void GlobalVariables::AddItem(const std::string& groupName, const std::string& key, const Vector3& value) {
	// 項目が未登録のときだけ既定値として追加する（読み込み済みの値は上書きしない。型が違えばそろえる）
	AddItemImpl(groupName, datas_[groupName], key, Item(std::in_place_type<Vector3>, value));
}

void GlobalVariables::AddItem(const std::string& groupName, const std::string& key, bool value) {
	// 項目が未登録のときだけ既定値として追加する（読み込み済みの値は上書きしない。型が違えばそろえる）
	AddItemImpl(groupName, datas_[groupName], key, Item(std::in_place_type<bool>, value));
}

int32_t GlobalVariables::GetIntValue(const std::string& groupName, const std::string& key) const {
	// 指定グループ・指定キーが存在するかチェックしてから値を取得
	assert(datas_.contains(groupName));
	const Group& group = datas_.at(groupName);
	assert(group.contains(key));
	return std::get<int32_t>(group.at(key));
}

float GlobalVariables::GetFloatValue(const std::string& groupName, const std::string& key) const {
	// 指定グループ・指定キーが存在するかチェックしてから値を取得
	assert(datas_.contains(groupName));
	const Group& group = datas_.at(groupName);
	assert(group.contains(key));
	return std::get<float>(group.at(key));
}

Vector3 GlobalVariables::GetVector3Value(const std::string& groupName, const std::string& key) const {
	// 指定グループ・指定キーが存在するかチェックしてから値を取得
	assert(datas_.contains(groupName));
	const Group& group = datas_.at(groupName);
	assert(group.contains(key));
	return std::get<Vector3>(group.at(key));
}

bool GlobalVariables::GetBoolValue(const std::string& groupName, const std::string& key) const {
	// 指定グループ・指定キーが存在するかチェックしてから値を取得
	assert(datas_.contains(groupName));
	const Group& group = datas_.at(groupName);
	assert(group.contains(key));
	return std::get<bool>(group.at(key));
}

void GlobalVariables::SaveFile(const std::string& groupName) {
	// グループを検索（未登録なら停止）
	std::map<std::string, Group>::iterator itGroup = datas_.find(groupName);
	assert(itGroup != datas_.end());

	// グループ名をキーにしたjsonオブジェクトを組み立てる
	json root = json::object();
	root[groupName] = json::object();

	// 各項目を保持している型に応じてjsonへ登録する
	for (auto& [itemName, item] : itGroup->second) {
		if (std::holds_alternative<int32_t>(item)) {
			root[groupName][itemName] = std::get<int32_t>(item);
		} else if (std::holds_alternative<float>(item)) {
			root[groupName][itemName] = std::get<float>(item);
		} else if (std::holds_alternative<Vector3>(item)) {
			// Vector3は要素数3のfloat配列として保存する
			Vector3 value = std::get<Vector3>(item);
			root[groupName][itemName] = json::array({value.x, value.y, value.z});
		} else if (std::holds_alternative<bool>(item)) {
			root[groupName][itemName] = std::get<bool>(item);
		}
	}

	// 保存先ディレクトリがなければ作成する（GameDataなど途中の階層もまとめて作る）
	std::filesystem::path dir(kDirectoryPath);
	if (!std::filesystem::exists(dir)) {
		std::filesystem::create_directories(dir);
	}

	// 書き込むJSONファイルのフルパスを合成してファイルを開く
	std::string filePath = kDirectoryPath + groupName + ".json";
	std::ofstream ofs;
	ofs.open(filePath);

	// ファイルオープン失敗?
	if (ofs.fail()) {
		std::string message = "Failed open data file for write.";
		MessageBoxA(nullptr, message.c_str(), "GlobalVariables", 0);
		assert(0);
		return;
	}

	// ファイルにjson文字列を書き込む(インデント幅4)
	ofs << std::setw(4) << root << std::endl;
	ofs.close();
}

void GlobalVariables::LoadFiles() {
	// 保存先ディレクトリがなければスキップする（初回起動時など）
	std::filesystem::path dir(kDirectoryPath);
	if (!std::filesystem::exists(dir)) {
		return;
	}

	// ディレクトリ内の.jsonファイルをすべて読み込む
	std::filesystem::directory_iterator dir_it(dir);
	for (const std::filesystem::directory_entry& entry : dir_it) {
		const std::filesystem::path& filePath = entry.path();

		// .jsonファイル以外はスキップ
		std::string extension = filePath.extension().string();
		if (extension.compare(".json") != 0) {
			continue;
		}

		// 拡張子を除いたファイル名＝グループ名として読み込む
		LoadFile(filePath.stem().string());
	}
}

void GlobalVariables::LoadFile(const std::string& groupName) {
	// 読み込むJSONファイルのフルパスを合成してファイルを開く
	std::string filePath = kDirectoryPath + groupName + ".json";
	std::ifstream ifs;
	ifs.open(filePath);

	// ファイルオープン失敗?
	if (ifs.fail()) {
		std::string message = "Failed open data file for read.";
		MessageBoxA(nullptr, message.c_str(), "GlobalVariables", 0);
		assert(0);
		return;
	}

	// json文字列からjsonのデータ構造に展開（手で編集して書き間違えたファイルは、知らせて読み飛ばす＝既定値のまま動く）
	json root;
	try {
		ifs >> root;
	} catch (const json::exception& exception) {
		std::string message = std::format("{}.json: parse error (skipped)\n{}", groupName, exception.what());
		MessageBoxA(nullptr, message.c_str(), "GlobalVariables", 0);
		return;
	}
	ifs.close();

	// グループを検索（ファイル名とグループ名が一致しないファイルは不正）
	json::iterator itGroup = root.find(groupName); // オブジェクトでない root のときも end() が返る
	if (itGroup == root.end() || !itGroup->is_object()) {
		std::string message = std::format("{}.json: group \"{}\" not found (skipped)", groupName, groupName);
		MessageBoxA(nullptr, message.c_str(), "GlobalVariables", 0);
		return;
	}

	// 各項目をjsonの型に応じて登録する
	Group& group = datas_[groupName];
	for (json::iterator itItem = itGroup->begin(); itItem != itGroup->end(); ++itItem) {
		const std::string& itemName = itItem.key();
		Item value;
		if (!JsonToItem(*itItem, &value)) {
			continue; // 対応していない型（文字列など）は無視する
		}

		// すでに登録済みの項目（実行中の Reload）は、登録時の型にそろえて上書きする
		// （起動時の読み込みは登録より前なので、型は後の AddItem がそろえる）
		auto itExisting = group.find(itemName);
		if (itExisting == group.end()) {
			group[itemName] = value;
		} else if (Item converted; ConvertItemType(value, itExisting->second, &converted)) {
			itExisting->second = converted;
		} else {
			OutputDebugStringA(std::format("GlobalVariables: {}.{} has a wrong type in the json. ignored.\n", groupName, itemName).c_str());
		}
	}
}

void GlobalVariables::Update() {
#ifdef USE_IMGUI
	// 初回だけ左側（HUDの下）に置く（以降は動かした位置が imgui.ini に残る）
	ImGui::SetNextWindowPos(ImVec2(24.0f, 180.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Global Variables", nullptr, ImGuiWindowFlags_MenuBar)) {
		ImGui::End();
		return;
	}
	if (!ImGui::BeginMenuBar()) {
		ImGui::End();
		return;
	}

	// 各グループをメニューとして表示する
	for (auto& [groupName, group] : datas_) {
		if (!ImGui::BeginMenu(groupName.c_str())) {
			continue;
		}

		// 各項目を保持している型に応じたウィジェットで表示・編集する
		for (auto& [itemName, item] : group) {
			if (std::holds_alternative<int32_t>(item)) {
				int32_t* ptr = std::get_if<int32_t>(&item);
				ImGui::DragInt(itemName.c_str(), ptr, 1);
			} else if (std::holds_alternative<float>(item)) {
				float* ptr = std::get_if<float>(&item);
				ImGui::DragFloat(itemName.c_str(), ptr, 0.01f);
			} else if (std::holds_alternative<Vector3>(item)) {
				Vector3* ptr = std::get_if<Vector3>(&item);
				ImGui::DragFloat3(itemName.c_str(), reinterpret_cast<float*>(ptr), 0.01f);
			} else if (std::holds_alternative<bool>(item)) {
				bool* ptr = std::get_if<bool>(&item);
				ImGui::Checkbox(itemName.c_str(), ptr);
			}
		}

		// 改行
		ImGui::Text("\n");

		// Saveボタンでこのグループをその場でJSONファイルに保存できる
		if (ImGui::Button("Save")) {
			SaveFile(groupName);
			std::string message = std::format("{}.json saved.", groupName);
			MessageBoxA(nullptr, message.c_str(), "GlobalVariables", 0);
		}

		// Reloadボタンで保存済みJSONの値へ戻せる（保存していない編集を破棄する）。
		// 一度も保存していないグループはファイルが無いので、その旨を案内して何もしない
		ImGui::SameLine();
		if (ImGui::Button("Reload")) {
			if (std::filesystem::exists(kDirectoryPath + groupName + ".json")) {
				LoadFile(groupName);
				std::string message = std::format("{}.json reloaded.", groupName);
				MessageBoxA(nullptr, message.c_str(), "GlobalVariables", 0);
			} else {
				std::string message = std::format("{}.json not found. (not saved yet)", groupName);
				MessageBoxA(nullptr, message.c_str(), "GlobalVariables", 0);
			}
		}

		ImGui::EndMenu();
	}

	ImGui::EndMenuBar();
	ImGui::End();
#endif // USE_IMGUI
}
