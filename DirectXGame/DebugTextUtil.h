#pragma once
#include "KamataEngine.h"
#include <string>

/// <summary>
/// DebugText の小さな補助関数（タイトル・メニューなどの仮UIの文字を中央揃えで出す用）。
/// ※DebugText は ASCII のみ（日本語は出ない）。日本語の文字は画像にして Sprite で出すこと
/// ※DebugText は Print で予約し、スプライト描画パス（Sprite::PreDraw～PostDraw）の中の
/// 　DebugText::GetInstance()->DrawAll() の時点でまとめて描かれる。DrawAll は1パスの中で何度呼んでもよく、
/// 　呼んだ時点までに Print した文字がその位置（重なり順）に描かれる
/// </summary>

// 1文字の大きさ（倍率1のとき。debugfont.png の1文字 9x18 ピクセル）
inline constexpr float kDebugTextCharWidth = static_cast<float>(KamataEngine::DebugText::kFontWidth);
inline constexpr float kDebugTextCharHeight = static_cast<float>(KamataEngine::DebugText::kFontHeight);

// 指定した中心座標（ピクセル）に文字列を中央揃えで予約する
inline void PrintDebugTextCentered(const std::string& text, float centerX, float centerY, float scale) {
	const float width = kDebugTextCharWidth * scale * static_cast<float>(text.size());
	const float height = kDebugTextCharHeight * scale;
	KamataEngine::DebugText::GetInstance()->Print(text, centerX - width * 0.5f, centerY - height * 0.5f, scale);
}
