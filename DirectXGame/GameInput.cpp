#define NOMINMAX
#include "GameInput.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <map>
#include <nlohmann/json.hpp>

// XInput（パッド入力）のライブラリ（古いWindowsでも動く互換版）
#pragma comment(lib, "xinput9_1_0.lib")

using namespace KamataEngine;
using json = nlohmann::json;

namespace {
// 割り当てファイルのパス（実行時の作業ディレクトリは DirectXGame/）
const char* kBindingFilePath = "Resources/GameData/Input.json";

// スティックのデッドゾーン（XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE 相当）
const int kStickDeadZone = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;

// スティックの生値（-32768~32767）を、デッドゾーンを除いた -1.0～+1.0 へ変換する
float NormalizeStick(SHORT value) {
	if (std::abs(static_cast<int>(value)) < kStickDeadZone) {
		return 0.0f;
	}
	return std::clamp(static_cast<float>(value) / 32767.0f, -1.0f, 1.0f);
}

// 小文字に揃える（Input.jsonの値を大文字小文字を気にせず書けるようにする）
std::string ToLower(const std::string& text) {
	std::string result = text;
	std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return result;
}

// Input.jsonのアクション名（並びは GameInput::Action と同じ順。Input.jsonに無いアクションは既定の割り当てが使われる）
const char* const kActionNames[] = {"Menu", "Decide", "Flatten"};
static_assert(std::size(kActionNames) == GameInput::kActionCount, "kActionNames の数が kActionCount と一致していない（Action を増やしたら両方そろえる）");

// キーボードのキー名 → 仮想キーコード（Input.jsonの "keyboard" に書ける値。大文字小文字は区別しない）
int KeyNameToVirtualKey(const std::string& name) {
	static const std::map<std::string, int> kSpecialKeys = {
	    {"up", VK_UP},         {"down", VK_DOWN},   {"left", VK_LEFT},     {"right", VK_RIGHT},   {"space", VK_SPACE},   {"escape", VK_ESCAPE},
	    {"esc", VK_ESCAPE},    {"enter", VK_RETURN}, {"return", VK_RETURN}, {"tab", VK_TAB},       {"shift", VK_SHIFT},   {"lshift", VK_LSHIFT},
	    {"rshift", VK_RSHIFT}, {"ctrl", VK_CONTROL}, {"lctrl", VK_LCONTROL}, {"rctrl", VK_RCONTROL}, {"alt", VK_MENU},
	};
	const std::string lower = ToLower(name);

	// 英字1文字（A～Z）と数字1文字（0～9）は仮想キーコードがASCIIと同じ
	if (lower.size() == 1) {
		const char c = lower[0];
		if (c >= 'a' && c <= 'z') {
			return 'A' + (c - 'a');
		}
		if (c >= '0' && c <= '9') {
			return c;
		}
	}
	// F1～F12
	if (lower.size() >= 2 && lower[0] == 'f') {
		const int number = std::atoi(lower.c_str() + 1);
		if (number >= 1 && number <= 12) {
			return VK_F1 + (number - 1);
		}
	}
	auto it = kSpecialKeys.find(lower);
	if (it != kSpecialKeys.end()) {
		return it->second;
	}
	return 0; // 不明（呼び出し側でエラーにする）
}

// パッドのボタン名 → PadButton（Input.jsonの "pad" に書ける値。大文字小文字は区別しない）
bool PadNameToButton(const std::string& name, GameInput::PadButton* out) {
	static const std::map<std::string, GameInput::PadButton> kPadButtons = {
	    {"a", GameInput::PadButton::kA},   {"b", GameInput::PadButton::kB},       {"x", GameInput::PadButton::kX},     {"y", GameInput::PadButton::kY},
	    {"lb", GameInput::PadButton::kLB}, {"rb", GameInput::PadButton::kRB},     {"back", GameInput::PadButton::kBack}, {"start", GameInput::PadButton::kStart},
	    {"ls", GameInput::PadButton::kLS}, {"rs", GameInput::PadButton::kRS},     {"lt", GameInput::PadButton::kLT},   {"rt", GameInput::PadButton::kRT},
	};
	auto it = kPadButtons.find(ToLower(name));
	if (it == kPadButtons.end()) {
		return false;
	}
	*out = it->second;
	return true;
}

// PadButton → XINPUTのボタンビット（LT/RTはアナログなので0を返し、呼び出し側で特別扱いする）
WORD PadButtonToMask(GameInput::PadButton button) {
	switch (button) {
	case GameInput::PadButton::kA:
		return XINPUT_GAMEPAD_A;
	case GameInput::PadButton::kB:
		return XINPUT_GAMEPAD_B;
	case GameInput::PadButton::kX:
		return XINPUT_GAMEPAD_X;
	case GameInput::PadButton::kY:
		return XINPUT_GAMEPAD_Y;
	case GameInput::PadButton::kLB:
		return XINPUT_GAMEPAD_LEFT_SHOULDER;
	case GameInput::PadButton::kRB:
		return XINPUT_GAMEPAD_RIGHT_SHOULDER;
	case GameInput::PadButton::kBack:
		return XINPUT_GAMEPAD_BACK;
	case GameInput::PadButton::kStart:
		return XINPUT_GAMEPAD_START;
	case GameInput::PadButton::kLS:
		return XINPUT_GAMEPAD_LEFT_THUMB;
	case GameInput::PadButton::kRS:
		return XINPUT_GAMEPAD_RIGHT_THUMB;
	default:
		return 0;
	}
}
} // namespace

GameInput* GameInput::GetInstance() {
	static GameInput instance;
	return &instance;
}

bool GameInput::LoadBindings() {
	// ファイルを開く
	std::ifstream file(kBindingFilePath);
	if (!file.is_open()) {
		bindingError_ = std::string("open failed: ") + kBindingFilePath;
		return false;
	}

	// JSONの解析（書き間違いがあると例外が飛ぶので受け止めてエラー表示に回す）
	json root;
	try {
		root = json::parse(file);
	} catch (const json::exception& exception) {
		bindingError_ = exception.what();
		return false;
	}

	// 型の確認（get<std::string>() は文字列以外（数値・配列・null）だと例外を投げてゲームごと落ちるため、
	// 読む前に型を確かめてエラー表示に回す）
	if (!root.is_object()) {
		bindingError_ = "root: must be an object { \"keyboard\": {...}, \"pad\": {...} }";
		return false;
	}
	for (const char* sectionName : {"keyboard", "pad"}) {
		if (root.contains(sectionName) && !root.at(sectionName).is_object()) {
			bindingError_ = std::string(sectionName) + ": must be an object { \"ActionName\": \"Key\" }";
			return false;
		}
	}

	// 全アクション分を読み取ってから反映する（途中でエラーが出たら既定/直前の割り当てのまま）
	std::array<Binding, kActionCount> loaded = kDefaultBindings;
	for (size_t i = 0; i < kActionCount; i++) {
		const char* actionName = kActionNames[i];

		// キーボード側（値は文字列1つ。複数のキーを1つのアクションに割り当てることはできない）
		if (root.contains("keyboard") && root.at("keyboard").contains(actionName)) {
			const json& value = root.at("keyboard").at(actionName);
			if (!value.is_string()) {
				bindingError_ = std::string("keyboard.") + actionName + ": must be a string (e.g. \"Space\")";
				return false;
			}
			const std::string keyName = value.get<std::string>();
			const int virtualKey = KeyNameToVirtualKey(keyName);
			if (virtualKey == 0) {
				bindingError_ = std::string("keyboard.") + actionName + ": unknown key \"" + keyName + "\"";
				return false;
			}
			loaded[i].keyboardVk = virtualKey;
		}

		// パッド側（同じく文字列1つ）
		if (root.contains("pad") && root.at("pad").contains(actionName)) {
			const json& value = root.at("pad").at(actionName);
			if (!value.is_string()) {
				bindingError_ = std::string("pad.") + actionName + ": must be a string (e.g. \"A\")";
				return false;
			}
			const std::string buttonName = value.get<std::string>();
			PadButton button;
			if (!PadNameToButton(buttonName, &button)) {
				bindingError_ = std::string("pad.") + actionName + ": unknown button \"" + buttonName + "\"";
				return false;
			}
			loaded[i].padButton = button;
		}
	}

	bindings_ = loaded;
	bindingError_.clear();
	return true;
}

void GameInput::Update() {
	// 前回状態を保存する
	keyStatesPre_ = keyStates_;
	padStatePre_ = padState_;
	padConnectedPre_ = padConnected_;

	// ゲームウィンドウが前面のときだけ入力を受け付ける
	// （GetAsyncKeyStateはフォーカスに関係なくキーを拾うため、裏に回ったら全キー離した扱いにする）
	const bool isForeground = (GetForegroundWindow() == WinApp::GetInstance()->GetHwnd());

	// キーボードの現在状態（仮想キーコード全部の押下状態を取得する）
	for (size_t key = 0; key < keyStates_.size(); key++) {
		keyStates_[key] = isForeground && (GetAsyncKeyState(static_cast<int>(key)) & 0x8000) != 0;
	}

	// パッド0番の現在状態。未接続のときはクリアして「何も押されていない」扱いにする
	XINPUT_STATE state{};
	padConnected_ = isForeground && (XInputGetState(0, &state) == ERROR_SUCCESS);
	padState_ = padConnected_ ? state : XINPUT_STATE{};
}

bool GameInput::IsPadActionDown(Action action, const XINPUT_STATE& state) const {
	const PadButton button = bindings_[static_cast<size_t>(action)].padButton;
	// LT/RT はアナログ値（0~255）なので、閾値以上をボタン押下として扱う
	if (button == PadButton::kLT) {
		return state.Gamepad.bLeftTrigger >= kTriggerThreshold;
	}
	if (button == PadButton::kRT) {
		return state.Gamepad.bRightTrigger >= kTriggerThreshold;
	}
	return (state.Gamepad.wButtons & PadButtonToMask(button)) != 0;
}

bool GameInput::IsKeyActionDown(Action action, bool trigger) const {
	const size_t key = static_cast<size_t>(bindings_[static_cast<size_t>(action)].keyboardVk);
	return trigger ? (keyStates_[key] && !keyStatesPre_[key]) : keyStates_[key];
}

bool GameInput::IsPressed(Action action) const { return IsKeyActionDown(action, false) || (padConnected_ && IsPadActionDown(action, padState_)); }

bool GameInput::IsTriggered(Action action) const {
	// キーボードは現在と前回の状態から「押した瞬間」を判定する
	if (IsKeyActionDown(action, true)) {
		return true;
	}
	// パッドも同様。接続した直後のフレームは前回状態が無く、押しっぱなしのボタンが誤発火する
	// （ワイヤレスの瞬断→再接続など）ので、接続が2フレーム続くまでトリガーは判定しない
	if (!padConnected_ || !padConnectedPre_) {
		return false;
	}
	const bool now = IsPadActionDown(action, padState_);
	const bool pre = IsPadActionDown(action, padStatePre_);
	return now && !pre;
}

bool GameInput::IsReleased(Action action) const {
	// キーボード：前フレームは押していて、今は押していない
	const size_t key = static_cast<size_t>(bindings_[static_cast<size_t>(action)].keyboardVk);
	if (!keyStates_[key] && keyStatesPre_[key]) {
		return true;
	}
	// パッドも同様。接続が2フレーム続いているときだけ判定する（切断の瞬間の誤発火を防ぐ）
	if (!padConnected_ || !padConnectedPre_) {
		return false;
	}
	return !IsPadActionDown(action, padState_) && IsPadActionDown(action, padStatePre_);
}

bool GameInput::IsKeyTriggered(int virtualKey) const {
	const size_t key = static_cast<size_t>(virtualKey);
	return keyStates_[key] && !keyStatesPre_[key];
}

Vector2 GameInput::GetMove() const {
	// キーボード（WASD）とパッド（Lスティック＋十字キー）を合算して各軸 -1～+1 に収める
	// （逆方向の同時入力は相殺される）
	float x = 0.0f;
	float y = 0.0f;
	if (IsKeyDown('D')) {
		x += 1.0f;
	}
	if (IsKeyDown('A')) {
		x -= 1.0f;
	}
	if (IsKeyDown('W')) {
		y += 1.0f;
	}
	if (IsKeyDown('S')) {
		y -= 1.0f;
	}
	if (padConnected_) {
		const WORD buttons = padState_.Gamepad.wButtons;
		if (buttons & XINPUT_GAMEPAD_DPAD_RIGHT) {
			x += 1.0f;
		}
		if (buttons & XINPUT_GAMEPAD_DPAD_LEFT) {
			x -= 1.0f;
		}
		if (buttons & XINPUT_GAMEPAD_DPAD_UP) {
			y += 1.0f;
		}
		if (buttons & XINPUT_GAMEPAD_DPAD_DOWN) {
			y -= 1.0f;
		}
		x += NormalizeStick(padState_.Gamepad.sThumbLX);
		y += NormalizeStick(padState_.Gamepad.sThumbLY);
	}
	return {std::clamp(x, -1.0f, 1.0f), std::clamp(y, -1.0f, 1.0f)};
}

float GameInput::GetUiMoveY() const {
	// 移動軸の上下に、メニュー専用で↑↓キーも合算する
	float y = GetMove().y;
	if (IsKeyDown(VK_UP)) {
		y += 1.0f;
	}
	if (IsKeyDown(VK_DOWN)) {
		y -= 1.0f;
	}
	return std::clamp(y, -1.0f, 1.0f);
}
