#pragma once

#include <KamataEngine.h>

/// <summary>
/// 画像（テクスチャ）のハンドル管理。
/// 複数のクラスで使う画像はここで1回だけ読み込み、ハンドルを extern で共有する
/// （TextureManager::Load は同じ名前なら同じハンドルを返すので、各クラスで直接 Load してもよい）。
/// 【画像の増やし方】Resources/ に png を置き、extern を1行・Resources.cpp に実体と Load を1行ずつ足す
/// ※今はまだ画像の素材が無い（HUD・タイトルなどは white1x1.png の色付きの板と DebugText で仮に作っている）
/// </summary>

// 読み込み関数（main起動時に1回呼ぶ）
void LoadResources();
