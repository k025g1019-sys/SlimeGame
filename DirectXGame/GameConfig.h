#pragma once

/// <summary>
/// ゲーム全体の設定（ウィンドウタイトル・画面サイズ）。
/// エンジン既定の WinApp::kWindowWidth(1280x720) ではなく、この値でウィンドウとバックバッファ・スプライト座標系を作る。
/// 画面サイズを参照するときは必ずこちらを使うこと（WinApp::kWindowWidth は1280のままなので使わない）
/// </summary>
namespace GameConfig {
// ウィンドウタイトル（テンプレート C:\KamataEngineTemplate の Tools/NewProject/new_project.ps1 の -Title で書き換えた行。変えるときはここを直す）
inline constexpr const wchar_t* kWindowTitle = L"スライム（仮題）";
// 画面の横幅(ピクセル)
inline constexpr int kScreenWidth = 1920;
// 画面の縦幅(ピクセル)
inline constexpr int kScreenHeight = 1080;
// 1フレームの時間（秒）。ゲームは固定で 1/60 秒ずつ進める（速度・時間の調整項目は「/秒」「秒」で書く）
inline constexpr float kDeltaTime = 1.0f / 60.0f;
} // namespace GameConfig
