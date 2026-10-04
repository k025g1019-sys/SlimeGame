#define NOMINMAX
#include "GameOverScene.h"
#include "DebugTextUtil.h"
#include "GameConfig.h"
#include "GameInput.h"
#include "GlobalVariables.h"
#include <algorithm>
using namespace KamataEngine;

namespace {
// 調整項目のグループ名
const char* kGroupName = "GameOver";
// カーソル移動とみなす上下入力の閾値
const float kCursorMoveThreshold = 0.5f;
// 項目の表示名（DebugTextはASCIIのみ。並びはカーソル位置と同じ）
const char* const kItemLabels[] = {"RETRY", "TO TITLE"};
} // namespace

GameOverScene::~GameOverScene() {
	delete backgroundSprite_;
	for (Sprite* sprite : itemSprites_) {
		delete sprite;
	}
}

void GameOverScene::Initialize() {
	// 白い1pxテクスチャを色付きの板に引き伸ばして使う（仮UI。本番は画像に差し替える）
	const uint32_t textureHandle = TextureManager::Load("white1x1.png");
	backgroundSprite_ = Sprite::Create(textureHandle, {0.0f, 0.0f});
	backgroundSprite_->SetSize({static_cast<float>(GameConfig::kScreenWidth), static_cast<float>(GameConfig::kScreenHeight)});
	for (Sprite*& sprite : itemSprites_) {
		sprite = Sprite::Create(textureHandle, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.5f, 0.5f});
	}

	fade_.Initialize();
	fade_.Start(Fade::Status::kFadeIn, kFadeDuration);
}

void GameOverScene::Update() {
	// ImGuiでの変更を毎フレーム反映する
	ApplyGlobalVariables();

	fade_.Update();

	switch (phase_) {
	case Phase::kFadeIn:
		// フェードイン中も上下入力を記録しておく（ゲーム中から押しっぱなしの入力を、
		// 受付開始の瞬間に「入れた瞬間」と誤検出してカーソルが勝手に動かないように）
		previousUiMoveY_ = GameInput::GetInstance()->GetUiMoveY();
		if (fade_.IsFinished()) {
			phase_ = Phase::kMain;
		}
		break;
	case Phase::kMain: {
		GameInput* input = GameInput::GetInstance();

		// 上下入力の「入れた瞬間」でカーソルを動かす
		const float uiMoveY = input->GetUiMoveY();
		if (uiMoveY > kCursorMoveThreshold && previousUiMoveY_ <= kCursorMoveThreshold) {
			cursorIndex_ = (cursorIndex_ + kItemCount - 1) % kItemCount;
		}
		if (uiMoveY < -kCursorMoveThreshold && previousUiMoveY_ >= -kCursorMoveThreshold) {
			cursorIndex_ = (cursorIndex_ + 1) % kItemCount;
		}
		previousUiMoveY_ = uiMoveY;

		// 決定で行き先を確定してフェードアウトへ
		if (input->IsTriggered(GameInput::Action::kDecide)) {
			result_ = (cursorIndex_ == 0) ? Result::kRetry : Result::kToTitle;
			phase_ = Phase::kFadeOut;
			fade_.Start(Fade::Status::kFadeOut, kFadeDuration);
		}
		break;
	}
	case Phase::kFadeOut:
		if (fade_.IsFinished()) {
			isFinished_ = true;
		}
		break;
	}

	// 背景と項目の見た目を反映する。
	// 選択中の項目は少し大きく・明るい色（SelectedScale / SelectedColor）、非選択は透明度を下げる（UnselectedAlpha）
	backgroundSprite_->SetColor({kBackgroundColor.x, kBackgroundColor.y, kBackgroundColor.z, 1.0f});
	for (int32_t i = 0; i < kItemCount; i++) {
		const bool isSelected = (i == cursorIndex_);
		const float scale = isSelected ? kSelectedScale : 1.0f;
		const Vector3& color = isSelected ? kSelectedColor : kUnselectedColor;
		itemSprites_[i]->SetPosition({kItemPosition.x, kItemPosition.y + kItemSpacing * static_cast<float>(i)});
		itemSprites_[i]->SetSize({kItemSize.x * scale, kItemSize.y * scale});
		itemSprites_[i]->SetColor({color.x, color.y, color.z, isSelected ? 1.0f : kUnselectedAlpha});
	}
}

void GameOverScene::Draw() {
	Sprite::PreDraw(DirectXCommon::GetInstance()->GetCommandList());
	backgroundSprite_->Draw();
	for (Sprite* sprite : itemSprites_) {
		sprite->Draw();
	}

	// 見出しと項目の文字（DebugTextはASCIIのみ。箱より手前に出すため、箱の後で DrawAll する）
	PrintDebugTextCentered("GAME OVER", kTitlePosition.x, kTitlePosition.y, kTitleScale);
	for (int32_t i = 0; i < kItemCount; i++) {
		const float labelScale = kLabelScale * ((i == cursorIndex_) ? kSelectedScale : 1.0f);
		PrintDebugTextCentered(kItemLabels[i], kItemPosition.x, kItemPosition.y + kItemSpacing * static_cast<float>(i), labelScale);
	}
	DebugText::GetInstance()->DrawAll();
	Sprite::PostDraw();

	// フェードは最前面
	fade_.Draw();
}

void GameOverScene::RegisterGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	globalVariables->CreateGroup(kGroupName);
	globalVariables->AddItem(kGroupName, "FadeDuration", kFadeDuration);
	globalVariables->AddItem(kGroupName, "BackgroundColor", kBackgroundColor);
	globalVariables->AddItem(kGroupName, "TitlePosition", kTitlePosition);
	globalVariables->AddItem(kGroupName, "TitleScale", kTitleScale);
	globalVariables->AddItem(kGroupName, "ItemPosition", kItemPosition);
	globalVariables->AddItem(kGroupName, "ItemSpacing", kItemSpacing);
	globalVariables->AddItem(kGroupName, "ItemSize", kItemSize);
	globalVariables->AddItem(kGroupName, "LabelScale", kLabelScale);
	globalVariables->AddItem(kGroupName, "SelectedColor", kSelectedColor);
	globalVariables->AddItem(kGroupName, "UnselectedColor", kUnselectedColor);
	globalVariables->AddItem(kGroupName, "SelectedScale", kSelectedScale);
	globalVariables->AddItem(kGroupName, "UnselectedAlpha", kUnselectedAlpha);
}

void GameOverScene::ApplyGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	// フェード時間は0にすると割り算が壊れるので下限を付ける
	kFadeDuration = std::max(globalVariables->GetFloatValue(kGroupName, "FadeDuration"), 0.01f);
	kBackgroundColor = globalVariables->GetVector3Value(kGroupName, "BackgroundColor");
	kTitlePosition = globalVariables->GetVector3Value(kGroupName, "TitlePosition");
	kTitleScale = globalVariables->GetFloatValue(kGroupName, "TitleScale");
	kItemPosition = globalVariables->GetVector3Value(kGroupName, "ItemPosition");
	kItemSpacing = globalVariables->GetFloatValue(kGroupName, "ItemSpacing");
	kItemSize = globalVariables->GetVector3Value(kGroupName, "ItemSize");
	kLabelScale = globalVariables->GetFloatValue(kGroupName, "LabelScale");
	kSelectedColor = globalVariables->GetVector3Value(kGroupName, "SelectedColor");
	kUnselectedColor = globalVariables->GetVector3Value(kGroupName, "UnselectedColor");
	kSelectedScale = globalVariables->GetFloatValue(kGroupName, "SelectedScale");
	kUnselectedAlpha = globalVariables->GetFloatValue(kGroupName, "UnselectedAlpha");
}
