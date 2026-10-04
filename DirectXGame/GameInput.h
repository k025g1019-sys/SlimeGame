#pragma once
#include "KamataEngine.h"
#include <Windows.h>
#include <Xinput.h>
#include <array>
#include <string>

/// <summary>
/// ゲーム全体の入力（論理アクション）層。
/// 「決定」「メニュー」のような論理アクション名でキーボードとXboxコントローラー（XInput）を併用できるようにする。
/// キーボードは GetAsyncKeyState、パッドは XInputGetState を直接使う自前実装。
/// ※エンジンの Input（DirectInput）は使わない。DirectInputはWindowsキーを無効化する設定
/// 　（DISCL_NOWINKEY）で初期化されており、ウィンドウモードでは違和感があるため。
///
/// ボタン/キーの割り当ては Resources/GameData/Input.json で変更できる（LoadBindings で読み込み。
/// 実行中は Game Debug の Reload Input で反映できる）。既定の割り当ては kDefaultBindings 参照。
/// 更新は毎フレーム main がメッセージ処理の直後（各シーンの更新前）に呼ぶ。
///
/// 【アクションの増やし方】次の4か所（＋任意でInput.json）を同じ順番でそろえて足す。
/// 　1. 下の enum class Action に1行追加（例: kJump）
/// 　2. kActionCount を1増やす
/// 　3. kDefaultBindings に既定のキー/ボタンを1行追加（Action と同じ順）
/// 　4. GameInput.cpp の kActionNames に Input.json での名前を追加（Action と同じ順。例: "Jump"）
/// 　5. （任意）Resources/GameData/Input.json の "keyboard" / "pad" に割り当てを書く（書かなければ既定の割り当て）
/// 　表の数が kActionCount と合っていないと static_assert でコンパイルエラーになる
/// 移動（WASD・スティック・十字キー）はアクションではなく GetMove() で読む
/// </summary>
class GameInput {
public:
	// 論理アクション（割り当てはInput.jsonで変更できる。以下のコメントは既定の割り当て）
	enum class Action {
		kMenu,    // メニューの開閉（パッド:メニュー(START) ／ キー:ESC）
		kDecide,  // 決定（パッド:A ／ キー:SPACE。タイトルやメニューなどのUI操作）
		kFlatten, // 平らになる（押している間。パッド:A ／ キー:SPACE。ゲーム中の操作。決定と同じキーなので、シーン開始時は一度離すまで効かない）
	};
	// アクションの総数（割り当てテーブルの大きさ。Action を増やしたらここも増やす）
	static constexpr size_t kActionCount = 3;

	// パッドのボタン（Input.jsonの "pad" に書ける値と対応する）
	enum class PadButton {
		kA,
		kB,
		kX,
		kY,
		kLB,
		kRB,
		kBack,
		kStart,
		kLS, // Lスティック押し込み
		kRS, // Rスティック押し込み
		kLT, // 左トリガー（アナログ値を閾値でボタン扱いにする）
		kRT, // 右トリガー（同上）
	};

	static GameInput* GetInstance();

	/// <summary>
	/// 毎フレーム更新（キーボード・パッドの現在状態の取得と前回状態の保存）
	/// </summary>
	void Update();

	/// <summary>
	/// ボタン/キーの割り当てを Resources/GameData/Input.json から読み込む（main起動時と、ImGuiのReload Inputで呼ぶ）。
	/// ファイルが無い・書き間違いがあるときは既定の割り当てのまま false を返す（エラーは GetBindingError で見られる）
	/// </summary>
	bool LoadBindings();

	// 割り当ての読み込みエラー（成功していれば空文字列。ImGuiでの表示用）
	const std::string& GetBindingError() const { return bindingError_; }

	// アクションに割り当て中のキーボード仮想キーコード（ImGuiでの表示用。Reloadの反映確認に使う）
	int GetKeyboardVk(Action action) const { return bindings_[static_cast<size_t>(action)].keyboardVk; }

	// アクションが押されているか
	bool IsPressed(Action action) const;
	// アクションを押した瞬間か
	bool IsTriggered(Action action) const;
	// アクションを離した瞬間か（溜め攻撃の発動など「離したとき」に使う）
	bool IsReleased(Action action) const;

	// キー単体の「押した瞬間」判定（仮想キーコード指定。F11のフルスクリーン切替などに使う）
	bool IsKeyTriggered(int virtualKey) const;

	// 移動軸（各軸 -1.0～+1.0。上・奥が+y）。Lスティック＋十字キー＋WASDキー。
	// ※矢印キーはアクションに割り当てられるよう、移動には含めない（メニューでは GetUiMoveY で使える）
	KamataEngine::Vector2 GetMove() const;

	// メニュー用の上下軸（-1.0～+1.0。上が正）。移動軸に加えて↑↓キーも効く
	float GetUiMoveY() const;

	// パッドが接続されているか
	bool IsPadConnected() const { return padConnected_; }

private:
	GameInput() = default;
	~GameInput() = default;
	GameInput(const GameInput&) = delete;
	const GameInput& operator=(const GameInput&) = delete;

	// 1アクション分の割り当て（キーボードの仮想キーコード＋パッドのボタン）
	struct Binding {
		int keyboardVk;
		PadButton padButton;
	};

	// 既定の割り当て（Input.jsonが無い・壊れているときに使う。並びは Action と同じ順）
	static constexpr auto kDefaultBindings = std::to_array<Binding>({
	    Binding{VK_ESCAPE, PadButton::kStart}, // kMenu
	    Binding{VK_SPACE,  PadButton::kA    }, // kDecide
	    Binding{VK_SPACE,  PadButton::kA    }, // kFlatten
	});
	static_assert(kDefaultBindings.size() == kActionCount, "kDefaultBindings の行数が kActionCount と一致していない（Action を増やしたら両方そろえる）");

	// キーが押されているか（現在の状態）
	bool IsKeyDown(int virtualKey) const { return keyStates_[static_cast<size_t>(virtualKey)]; }
	// アクションが指定のパッド状態で押されているか
	bool IsPadActionDown(Action action, const XINPUT_STATE& state) const;
	// アクションがキーボードで押されているか（trigger=true なら押した瞬間のみ）
	bool IsKeyActionDown(Action action, bool trigger) const;

	// 現在の割り当て（起動時とReloadでInput.jsonから読み込む）
	std::array<Binding, kActionCount> bindings_ = kDefaultBindings;
	// 割り当ての読み込みエラー
	std::string bindingError_;

	// キーボードの状態（現在・前回）。仮想キーコードで引く
	std::array<bool, 256> keyStates_ = {};
	std::array<bool, 256> keyStatesPre_ = {};

	// パッドの状態（現在・前回）
	XINPUT_STATE padState_ = {};
	XINPUT_STATE padStatePre_ = {};
	bool padConnected_ = false;
	bool padConnectedPre_ = false;

	// LT/RT をボタンとして扱う閾値（0~255。XINPUT_GAMEPAD_TRIGGER_THRESHOLD 相当）
	static inline int kTriggerThreshold = XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
};
