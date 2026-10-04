#include "Boss.h"
#include "Effects.h"
#include "Enemy.h"
#include "Field.h"
#include "GameCamera.h"
#include "GameClearScene.h"
#include "GameConfig.h"
#include "GameInput.h"
#include "GameOverScene.h"
#include "GameScene.h"
#include "GlobalVariables.h"
#include "Hud.h"
#include "KamataEngine.h"
#include "PauseMenu.h"
#include "Player.h"
#include "Projectile.h"
#include "Resources.h"
#include "Sounds.h"
#include "TitleScene.h"
#include <Windows.h>
#include <cassert>
#include <objbase.h>
#include <string>

#ifdef USE_IMGUI
#include <cfloat>
#include <imgui.h>
#endif

using namespace KamataEngine;

// シーン（型）
enum class Scene {
	kUnknown = 0,
	kTitle,     // タイトル
	kGame,      // ゲームプレイ
	kGameOver,  // ゲームオーバー
	kGameClear, // ゲームクリア
};

namespace {
// 現在のシーン
Scene scene = Scene::kUnknown;
// 各シーン（使用中のものだけ生成されている）
TitleScene* titleScene = nullptr;
GameScene* gameScene = nullptr;
GameOverScene* gameOverScene = nullptr;
GameClearScene* gameClearScene = nullptr;

// タイトルシーンを開始する
void StartTitle() {
	scene = Scene::kTitle;
	// タイトルとゲーム中は同じBGMを流す（ファイルが無ければ鳴らないだけ）
	PlayBgm(bgmMain);
	titleScene = new TitleScene();
	titleScene->Initialize();
}

// ゲームシーンを開始する
void StartGame() {
	scene = Scene::kGame;
	// 同じBGMが再生中なら鳴らし直さない（タイトルから続けて流れる）
	PlayBgm(bgmMain);
	gameScene = new GameScene();
	gameScene->Initialize();
}

// ゲームオーバーシーンを開始する
void StartGameOver() {
	scene = Scene::kGameOver;
	// BGMを止める（専用のBGMを用意したら PlayBgm に変える）
	StopBgm();
	gameOverScene = new GameOverScene();
	gameOverScene->Initialize();
}

// ゲームクリアシーンを開始する
void StartGameClear() {
	scene = Scene::kGameClear;
	// BGMを止める（クリアのファンファーレは GameClearScene が鳴らす）
	StopBgm();
	gameClearScene = new GameClearScene();
	gameClearScene->Initialize();
}

// シーン切り替え（現在シーンが終了していたら次のシーンへ）
void ChangeScene() {
	switch (scene) {
	case Scene::kTitle:
		if (titleScene->IsFinished()) {
			// タイトル終了 → ゲームへ
			delete titleScene;
			titleScene = nullptr;
			StartGame();
		}
		break;
	case Scene::kGame:
		if (gameScene->IsFinished()) {
			// ゲーム終了 → 結果に応じて分岐する
			const GameScene::Result result = gameScene->GetResult();
			delete gameScene;
			gameScene = nullptr;
			switch (result) {
			case GameScene::Result::kGameOver:
				StartGameOver();
				break;
			case GameScene::Result::kClear:
				StartGameClear();
				break;
			case GameScene::Result::kToTitle:
			case GameScene::Result::kNone:
				// メニューからタイトルへ戻る
				StartTitle();
				break;
			}
		}
		break;
	case Scene::kGameOver:
		if (gameOverScene->IsFinished()) {
			// リトライならゲームへ、そうでなければタイトルへ
			const GameOverScene::Result result = gameOverScene->GetResult();
			delete gameOverScene;
			gameOverScene = nullptr;
			if (result == GameOverScene::Result::kRetry) {
				StartGame();
			} else {
				StartTitle();
			}
		}
		break;
	case Scene::kGameClear:
		if (gameClearScene->IsFinished()) {
			// クリア後はタイトルへ戻る
			delete gameClearScene;
			gameClearScene = nullptr;
			StartTitle();
		}
		break;
	default:
		break;
	}
}

// 全シーンの破棄（使用していない側は nullptr なので delete しても安全）。
// 終了時と、ImGuiのシーンジャンプで使う。シーンを増やしたらここにも足すこと
// （足し忘れると、そのシーンで終了したときにモデルやスプライトがエンジンの終了処理より前に解放されない）
void DeleteAllScenes() {
	delete titleScene;
	titleScene = nullptr;
	delete gameScene;
	gameScene = nullptr;
	delete gameOverScene;
	gameOverScene = nullptr;
	delete gameClearScene;
	gameClearScene = nullptr;
}

// 現在シーンの更新
void UpdateScene() {
	switch (scene) {
	case Scene::kTitle:
		titleScene->Update();
		break;
	case Scene::kGame:
		gameScene->Update();
		break;
	case Scene::kGameOver:
		gameOverScene->Update();
		break;
	case Scene::kGameClear:
		gameClearScene->Update();
		break;
	default:
		break;
	}
}

// マウス系メッセージのクライアント座標を描画解像度（1920x1080）基準へ変換する。
// 描画は常に1920x1080のバックバッファをウィンドウの大きさへ拡縮表示しているが、
// ImGuiはメッセージに入っているクライアント座標をそのまま使うため、ウィンドウが1920x1080
// でないとき（既定の1600x900など）は見た目のカーソル位置とImGuiの反応位置がずれる。
// ここでメッセージの座標自体を描画解像度へスケールして直す（ゲーム入力はGameInputの
// ポーリングでマウス不使用のため影響しない）
LPARAM ScaleMouseLParam(HWND hwnd, LPARAM lparam) {
	RECT clientRect{};
	if (!GetClientRect(hwnd, &clientRect)) {
		return lparam;
	}
	const int clientWidth = static_cast<int>(clientRect.right - clientRect.left);
	const int clientHeight = static_cast<int>(clientRect.bottom - clientRect.top);
	if (clientWidth <= 0 || clientHeight <= 0) {
		return lparam;
	}
	// 符号付き16bitで取り出す（ドラッグ中はクライアント外の負座標も届く）
	const int x = static_cast<int>(static_cast<short>(LOWORD(lparam)));
	const int y = static_cast<int>(static_cast<short>(HIWORD(lparam)));
	const int scaledX = x * GameConfig::kScreenWidth / clientWidth;
	const int scaledY = y * GameConfig::kScreenHeight / clientHeight;
	return MAKELPARAM(static_cast<WORD>(static_cast<short>(scaledX)), static_cast<WORD>(static_cast<short>(scaledY)));
}

// クライアント座標を持つマウスメッセージか（ホイール系は画面座標なので含めない）
bool IsClientMouseMessage(UINT message) {
	switch (message) {
	case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MBUTTONDBLCLK:
	case WM_XBUTTONDOWN:
	case WM_XBUTTONUP:
	case WM_XBUTTONDBLCLK:
		return true;
	default:
		return false;
	}
}

// 溜まっているウィンドウメッセージをすべて処理する（戻り値：終了要求が来たか）。
// ※エンジンの WinApp::ProcessMessage() は1回の呼び出しで「1件」しか処理しないため使わない。
// 　1フレームに1件ずつだと、キーのリピートやホイールなどでキューに入る量（毎秒30件以上）が
// 　処理量（毎秒60件）に迫り、溜まり始めるとメッセージ経由の入力＝ImGuiの操作や×ボタンだけが
// 　どんどん遅れていく（ゲーム入力はGameInputのポーリングなので遅れず、症状に気づきにくい）
bool ProcessAllMessages() {
	MSG msg{};
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		// 終了要求（×ボタン→ウィンドウ破棄の後に届く）ならループを抜けてよい
		if (msg.message == WM_QUIT) {
			return true;
		}
		// マウス座標をウィンドウの拡縮に合わせて描画解像度へ直す（ImGuiのカーソル位置ずれ対策）
		if (IsClientMouseMessage(msg.message)) {
			msg.lParam = ScaleMouseLParam(msg.hwnd, msg.lParam);
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg); // エンジンの WinApp::WindowProc へ配送される（ImGuiへの入力伝達もここ経由）
	}
	return false;
}

// 現在シーンの描画
void DrawScene() {
	switch (scene) {
	case Scene::kTitle:
		titleScene->Draw();
		break;
	case Scene::kGame:
		gameScene->Draw();
		break;
	case Scene::kGameOver:
		gameOverScene->Draw();
		break;
	case Scene::kGameClear:
		gameClearScene->Draw();
		break;
	default:
		break;
	}
}

#ifdef USE_IMGUI
// ImGuiからのシーン遷移リクエスト（kUnknown=リクエスト無し。フレーム頭の安全なタイミングで反映する）
Scene requestedScene = Scene::kUnknown;

// シーン遷移のデバッグウィンドウ（どのシーンからでも好きなシーンへ飛べる。動作確認用）
void DrawSceneDebugWindow() {
	// 初回だけ右上寄りに置く（以降は動かした位置が imgui.ini に残る）
	ImGui::SetNextWindowPos(ImVec2(1555.0f, 200.0f), ImGuiCond_FirstUseEver);
	ImGui::Begin("Scene Debug");
	const char* sceneName = "?";
	switch (scene) {
	case Scene::kTitle:
		sceneName = "Title";
		break;
	case Scene::kGame:
		sceneName = "Game";
		break;
	case Scene::kGameOver:
		sceneName = "GameOver";
		break;
	case Scene::kGameClear:
		sceneName = "GameClear";
		break;
	default:
		break;
	}
	ImGui::Text("scene: %s", sceneName);
	if (ImGui::Button("Title")) {
		requestedScene = Scene::kTitle;
	}
	ImGui::SameLine();
	if (ImGui::Button("Game")) {
		requestedScene = Scene::kGame;
	}
	ImGui::SameLine();
	if (ImGui::Button("GameOver")) {
		requestedScene = Scene::kGameOver;
	}
	ImGui::SameLine();
	if (ImGui::Button("GameClear")) {
		requestedScene = Scene::kGameClear;
	}
	ImGui::End();
}

// ImGuiで要求されたシーン遷移を反映する（Update/Drawの外の安全なタイミングで呼ぶ。現在のシーンは破棄される）
void ApplyRequestedScene() {
	if (requestedScene == Scene::kUnknown) {
		return;
	}
	DeleteAllScenes();
	switch (requestedScene) {
	case Scene::kTitle:
		StartTitle();
		break;
	case Scene::kGame:
		StartGame();
		break;
	case Scene::kGameOver:
		StartGameOver();
		break;
	case Scene::kGameClear:
		StartGameClear();
		break;
	default:
		break;
	}
	requestedScene = Scene::kUnknown;
}
#endif
} // namespace

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

	// 実行ファイルの隣に Resources フォルダがあれば、そこを作業フォルダにする。
	// （Resourcesは相対パスで読むため、配布パッケージをショートカットやコマンドラインなど
	// 　別の作業フォルダから起動されても正しく動くようにする。
	// 　開発時のexe（Generated\Outputs）の隣にはResourcesが無いので、従来どおり起動時の作業フォルダのまま）
	{
		wchar_t exePath[MAX_PATH]{};
		GetModuleFileNameW(nullptr, exePath, MAX_PATH);
		std::wstring exeDirectory(exePath);
		const size_t lastSlash = exeDirectory.find_last_of(L"\\/");
		if (lastSlash != std::wstring::npos) {
			exeDirectory.resize(lastSlash);
			if (GetFileAttributesW((exeDirectory + L"\\Resources").c_str()) != INVALID_FILE_ATTRIBUTES) {
				SetCurrentDirectoryW(exeDirectory.c_str());
			}
		}
	}

	// --- エンジンの初期化 ---
	// KamataEngine::Initialize は解像度が1280x720固定で、さらにエンジンのInput（DirectInput。Windowsキーを
	// 無効化してしまう）まで初期化するため使わない。同じ内容を1920x1080で、必要なものだけ手動で初期化する。
	// 順番：COM → ウィンドウ → DirectX → ImGui → テクスチャ → スプライト → 3D（ModelCommon・PrimitiveDrawer）
	// 　　　→ デバッグ文字 → オーディオ
	// ※AxisIndicator（右上の軸表示）は描画位置が1280幅前提で固定のため使わない（軸が見たいときは PrimitiveDrawer の線で描く）
	// ※DebugCamera はエンジンのInputが前提で動かないため使わない（GameCamera の ImGui デバッグカメラ＝Game Debug の Debug Camera を参照）

	// COMの初期化（PNGテクスチャの読み込み(WIC)が必要とする。通常はKamataEngine::Initializeが行っている）
	const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	assert(SUCCEEDED(comResult));
	static_cast<void>(comResult);

	// ウィンドウ（縦横比16:9のままドラッグでサイズ変更できる。起動時は作業の邪魔にならないよう
	// 画面いっぱいではなく1600x900で出す。描画は常に1920x1080のバックバッファへ行われ、
	// ウィンドウの大きさに合わせて拡縮表示されるだけなので、ゲーム側の座標は影響を受けない）
	WinApp* winApp = WinApp::GetInstance();
	winApp->SetSizeChangeMode(WinApp::SizeChangeMode::kFixedAspect);
	winApp->CreateGameWindow(GameConfig::kWindowTitle, WS_OVERLAPPEDWINDOW, 1600, 900);

	// DirectX基盤（バックバッファも1920x1080）
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	dxCommon->Initialize(GameConfig::kScreenWidth, GameConfig::kScreenHeight);

#ifdef USE_IMGUI
	// ImGui（デバッグUI）。テクスチャより先に初期化する
	// （ImGuiのフォントがデスクリプタヒープの先頭を使うため、順序を変えるとテクスチャが壊れて真っ白になる）
	ImGuiManager::GetInstance()->Initialize();
#endif

	// テクスチャ（Resources/ 基準）とスプライト（座標系を実解像度に合わせる）
	TextureManager::GetInstance()->Initialize();
	Sprite::StaticInitialize(dxCommon->GetDevice(), GameConfig::kScreenWidth, GameConfig::kScreenHeight);

	// 3D描画の共通部分（KamataEngine::Initialize と同じ内容）。
	// ・白い1pxテクスチャ：モデルのマテリアルの既定テクスチャ（先に読み込んでおく）
	// ・ModelCommon：Model の描画パイプライン・既定のライト（白い平行光源3つ）・既定の ObjectColor
	// ・PrimitiveDrawer：3Dの線（DrawLine3d）。毎フレーム Reset が必要（下のメインループ参照）
	TextureManager::Load("white1x1.png");
	ModelCommon::GetInstance()->Initialize();
	PrimitiveDrawer::GetInstance()->Initialize();

	// デバッグ文字表示（ASCIIのみ。Resources/debugfont.png）
	// （※入力はGameInputの自前実装(GetAsyncKeyState/XInput)を使うため、エンジンのInputは初期化しない。
	// 　エンジンのInputはDirectInputをDISCL_NOWINKEY＝Windowsキー無効で確保してしまうため）
	DebugText::GetInstance()->Initialize();

	// オーディオ（XAudio2。wavは Resources/ 基準の相対パスで読み込む）
	Audio::GetInstance()->Initialize();

	// グローバル変数（調整項目）の読み込み（シーン生成より前に行う。後続のAddItemは読み込み済みの値を上書きしない）
	GlobalVariables::GetInstance()->LoadFiles();

	// ボタン/キー割り当ての読み込み（Resources/GameData/Input.json。無い・壊れているときは既定の割り当てで動く）
	GameInput::GetInstance()->LoadBindings();

	// リソース読み込み
	LoadResources();
	LoadAudio();

	// 各クラスの調整項目の登録（ファイルに無かった項目だけ既定値で追加される。クラスを増やしたらここに足す）
	GameScene::RegisterGlobalVariables();
	Field::RegisterGlobalVariables();
	GameCamera::RegisterGlobalVariables();
	Player::RegisterGlobalVariables();
	Enemy::RegisterGlobalVariables();
	Projectile::RegisterGlobalVariables();
	Boss::RegisterGlobalVariables();
	Effects::RegisterGlobalVariables();
	Hud::RegisterGlobalVariables();
	TitleScene::RegisterGlobalVariables();
	GameOverScene::RegisterGlobalVariables();
	GameClearScene::RegisterGlobalVariables();
	PauseMenu::RegisterGlobalVariables();
	RegisterSoundGlobalVariables();

	// 各クラスの調整項目を適用（読み込んだ値を反映してからシーンを開始する）
	GameScene::ApplyGlobalVariables();
	Field::ApplyGlobalVariables();
	GameCamera::ApplyGlobalVariables();
	Player::ApplyGlobalVariables();
	Enemy::ApplyGlobalVariables();
	Projectile::ApplyGlobalVariables();
	Boss::ApplyGlobalVariables();
	Effects::ApplyGlobalVariables();
	Hud::ApplyGlobalVariables();
	TitleScene::ApplyGlobalVariables();
	GameOverScene::ApplyGlobalVariables();
	GameClearScene::ApplyGlobalVariables();
	PauseMenu::ApplyGlobalVariables();
	ApplySoundGlobalVariables();

	// 最初のシーンを生成・初期化する
#ifdef _DEBUG
	// デバッグ時はゲームシーンから始める（タイトルを飛ばして素早く確認できるように）
	StartGame();
#else
	// 通常はタイトルシーンから始める
	StartTitle();
#endif

	// メインループ
	while (true) {
		// メッセージ処理（終了要求でループを抜ける）。
		// エンジンの WinApp::ProcessMessage() は1フレーム1件しか処理しないため、
		// 全件処理する自前版を使う（詳細は ProcessAllMessages のコメント参照）
		if (ProcessAllMessages()) {
			break;
		}

		// スプライトのインスタンスカウントと、3D線の本数のリセット（毎フレームの開始時に必須。
		// PrimitiveDrawer は Reset しないと線の数が溜まり続け、上限4096本を超えるとメモリを壊す）
		Sprite::ResetInstanceCount();
		PrimitiveDrawer::GetInstance()->Reset();

		// ゲーム入力（論理アクション）の更新。各シーンの更新前に行う
		GameInput::GetInstance()->Update();

		// オーディオの更新（再生し終えたボイスの後始末）
		Audio::GetInstance()->Update();

		// F11でフルスクリーン切り替え（1080pモニターではウィンドウモードだと高さが少し詰まるため）
		if (GameInput::GetInstance()->IsKeyTriggered(VK_F11)) {
			winApp->SetFullscreen(!winApp->IsFullscreen());
		}

#ifdef USE_IMGUI
		// ImGuiで要求されたシーン遷移の反映（前フレームのボタン操作。Update/Drawの外の安全なタイミング）
		ApplyRequestedScene();
#endif

		// シーン切り替え
		ChangeScene();

#ifdef USE_IMGUI
		// ImGuiの受付開始（NewFrame）。各シーンの Update 内で ImGui::Begin/End を呼ぶ前に行う
		ImGuiManager::GetInstance()->Begin();

		// 【ImGuiのカーソル位置ずれ対策 その2】表示サイズを描画解像度へ矯正する。
		// ImGuiのWin32バックエンドは毎フレーム DisplaySize をウィンドウのクライアントサイズ（既定1600x900）に
		// 設定するが、描画は1920x1080のバックバッファへ行われるため、そのままだとImGuiだけが左上に縮んで
		// 描画され、カーソルの見た目と反応位置がずれる。NewFrameの後にここで1920x1080へ上書きすると、
		// 描画時のビューポートがバックバッファ全体になり、等倍・ずれ無しになる
		// （マウス座標側は ProcessAllMessages がクライアント座標→1920x1080へスケール済み）
		{
			ImGuiIO& io = ImGui::GetIO();
			io.DisplaySize = ImVec2(static_cast<float>(GameConfig::kScreenWidth), static_cast<float>(GameConfig::kScreenHeight));
			ImGuiViewport* viewport = ImGui::GetMainViewport();
			viewport->Pos = ImVec2(0.0f, 0.0f);
			viewport->Size = io.DisplaySize;
			viewport->WorkPos = viewport->Pos;
			viewport->WorkSize = viewport->Size;

			// カーソルがウィンドウの外にあるときは「マウス不在」にする。
			// ImGuiバックエンドのポーリング経路（フォアグラウンドのままカーソルが外へ出たとき等）は
			// 未スケールのクライアント座標を入れてしまい、ウィンドウの右端・下端すぐ外のカーソルが
			// 画面内の位置として扱われて幻のホバー表示が出るため、ここで打ち消す
			POINT cursorPoint{};
			RECT clientRect{};
			if (GetCursorPos(&cursorPoint) && ScreenToClient(winApp->GetHwnd(), &cursorPoint) && GetClientRect(winApp->GetHwnd(), &clientRect)) {
				const bool isCursorInside = (cursorPoint.x >= clientRect.left) && (cursorPoint.x < clientRect.right) && (cursorPoint.y >= clientRect.top) && (cursorPoint.y < clientRect.bottom);
				if (!isCursorInside) {
					io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
				}
			}
		}

		// シーン遷移のデバッグウィンドウ（好きなシーンへ飛べる）
		DrawSceneDebugWindow();

		// タイトルなどゲームシーン以外でも調整項目を編集できるようにする
		// （ゲーム中は GameScene::UpdateImGui が同じウィンドウを出すので、ここでは出さない。
		// 　Soundグループの反映もゲームシーン以外はここで行う＝タイトルの決定音などに即効く）。
		// 「GameScene 以外」としているので、シーンを増やしても自動でここが効く。
		// GameScene のように自分で GlobalVariables::Update を呼ぶシーンを増やしたときだけ、ここで除外する
		if (scene != Scene::kGame) {
			GlobalVariables::GetInstance()->Update();
			ApplySoundGlobalVariables();
		}
#endif

		// 現在シーンの更新
		UpdateScene();

#ifdef USE_IMGUI
		// ImGuiの受付終了（Render）。ここまでに積んだウィンドウを描画データに確定させる
		ImGuiManager::GetInstance()->End();
#endif

		// 描画開始（画面と深度バッファのクリア）
		dxCommon->PreDraw();

		// 現在シーンの描画
		DrawScene();

#ifdef USE_IMGUI
		// ImGuiの描画。PreDrawで設定した描画先へ積む（PostDrawの前に呼ぶ）
		ImGuiManager::GetInstance()->Draw();
#endif

		// 描画終了
		dxCommon->PostDraw();
	}

	// シーンの開放（シーンを増やしたら DeleteAllScenes に足す）。
	// モデル・WorldTransform・Camera・ObjectColor などGPUリソースを持つ物は、
	// 下のエンジン終了処理（特に DirectXCommon::Terminate）より前にすべて破棄すること
	DeleteAllScenes();

	// エンジンの終了処理（初期化の逆順）
	// 3D描画の共通部分（KamataEngine::Finalize と同じ順：PrimitiveDrawer → ModelCommon）
	PrimitiveDrawer::Terminate();
	ModelCommon::Terminate();
#ifdef USE_IMGUI
	// ImGuiはシングルトンの破棄（デストラクタが内部でFinalizeを呼ぶ）だけを行う。
	// ・明示的に Finalize() も呼ぶと二重解放でクラッシュする
	// ・必ず DirectXCommon::Terminate() より前に呼ぶこと。呼ばずに放置すると、静的破棄の
	//   タイミング（DirectXCommon破棄後）にFinalizeが走り、解放済みのDescriptorHeapManagerを
	//   触って読み取りアクセス違反になる（×ボタンで閉じたときにクラッシュしていた原因）
	ImGuiManager::Terminate();
#endif
	Audio::Terminate();
	TextureManager::Terminate();
	DirectXCommon::Terminate();
	WinApp::Terminate();
	CoUninitialize();

	return 0;
}
