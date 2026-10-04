#define NOMINMAX
#include "TitleScene.h"
#include "DebugTextUtil.h"
#include "GameConfig.h"
#include "GameInput.h"
#include "GlobalVariables.h"
#include "Sounds.h"
#include <algorithm>
using namespace KamataEngine;

namespace {
// 調整項目のグループ名
const char* kGroupName = "Title";
} // namespace

TitleScene::~TitleScene() { delete backgroundSprite_; }

void TitleScene::Initialize() {
	fade_.Initialize();
	fade_.Start(Fade::Status::kFadeIn, kFadeDuration);

	// 画面全体を覆う背景の板（白い1pxテクスチャを引き伸ばして色を付ける）
	backgroundSprite_ = Sprite::Create(TextureManager::Load("white1x1.png"), {0.0f, 0.0f});
	backgroundSprite_->SetSize({static_cast<float>(GameConfig::kScreenWidth), static_cast<float>(GameConfig::kScreenHeight)});
}

void TitleScene::Update() {
	// ImGuiでの変更を毎フレーム反映する
	ApplyGlobalVariables();

	frameCounter_++;

	// 背景色の反映（ImGuiで変えた瞬間に効くよう毎フレーム設定する）
	backgroundSprite_->SetColor({kBackgroundColor.x, kBackgroundColor.y, kBackgroundColor.z, 1.0f});

	fade_.Update();

	switch (phase_) {
	case Phase::kFadeIn:
		// フェードインが終わったら入力受付へ
		if (fade_.IsFinished()) {
			phase_ = Phase::kMain;
		}
		break;
	case Phase::kMain:
		// 決定でフェードアウトを開始する（決定音を鳴らす）
		if (GameInput::GetInstance()->IsTriggered(GameInput::Action::kDecide)) {
			PlaySe(seDecide);
			phase_ = Phase::kFadeOut;
			fade_.Start(Fade::Status::kFadeOut, kFadeDuration);
		}
		break;
	case Phase::kFadeOut:
		// フェードアウトが終わったらシーン終了
		if (fade_.IsFinished()) {
			isFinished_ = true;
		}
		break;
	}
}

void TitleScene::Draw() {
	Sprite::PreDraw(DirectXCommon::GetInstance()->GetCommandList());
	backgroundSprite_->Draw();

	// タイトルと案内の文字（DebugTextはASCIIのみ。本番はロゴ画像に差し替える）
	PrintDebugTextCentered("SLIME (PROTOTYPE)", kTitlePosition.x, kTitlePosition.y, kTitleScale);
	// 遊び方のひとこと（タイトルの少し下に2行。爆弾だけは飲み込まずに跳ね返す）
	const float helpY = kTitlePosition.y + kDebugTextCharHeight * kTitleScale;
	PrintDebugTextCentered("HOLD SPACE: FLATTEN & CATCH    RELEASE: SWALLOW    A/D: SPEED", kTitlePosition.x, helpY, kHelpScale);
	PrintDebugTextCentered("BOMBS: DON'T SWALLOW! STAY ROUND TO BOUNCE THEM AT THE BOSS", kTitlePosition.x, helpY + kDebugTextCharHeight * kHelpScale * kHelpLineSpacing, kHelpScale);
	// 案内文字は点滅させる（入力受付前のフェード中も表示する）
	const bool isPressVisible = (kBlinkFrames <= 0) || ((frameCounter_ / kBlinkFrames) % 2 == 0) || (phase_ == Phase::kFadeOut);
	if (isPressVisible) {
		PrintDebugTextCentered("PRESS SPACE", kPressPosition.x, kPressPosition.y, kPressScale);
	}
	DebugText::GetInstance()->DrawAll();
	Sprite::PostDraw();

	// フェードは最前面
	fade_.Draw();
}

void TitleScene::RegisterGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	globalVariables->CreateGroup(kGroupName);
	globalVariables->AddItem(kGroupName, "FadeDuration", kFadeDuration);
	globalVariables->AddItem(kGroupName, "BackgroundColor", kBackgroundColor);
	globalVariables->AddItem(kGroupName, "TitlePosition", kTitlePosition);
	globalVariables->AddItem(kGroupName, "TitleScale", kTitleScale);
	globalVariables->AddItem(kGroupName, "HelpScale", kHelpScale);
	globalVariables->AddItem(kGroupName, "HelpLineSpacing", kHelpLineSpacing);
	globalVariables->AddItem(kGroupName, "PressPosition", kPressPosition);
	globalVariables->AddItem(kGroupName, "PressScale", kPressScale);
	globalVariables->AddItem(kGroupName, "BlinkFrames", kBlinkFrames);
}

void TitleScene::ApplyGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	// フェード時間は0にすると割り算が壊れるので下限を付ける
	kFadeDuration = std::max(globalVariables->GetFloatValue(kGroupName, "FadeDuration"), 0.01f);
	kBackgroundColor = globalVariables->GetVector3Value(kGroupName, "BackgroundColor");
	kTitlePosition = globalVariables->GetVector3Value(kGroupName, "TitlePosition");
	kTitleScale = globalVariables->GetFloatValue(kGroupName, "TitleScale");
	kHelpScale = std::max(globalVariables->GetFloatValue(kGroupName, "HelpScale"), 0.1f);
	kHelpLineSpacing = globalVariables->GetFloatValue(kGroupName, "HelpLineSpacing");
	kPressPosition = globalVariables->GetVector3Value(kGroupName, "PressPosition");
	kPressScale = globalVariables->GetFloatValue(kGroupName, "PressScale");
	kBlinkFrames = globalVariables->GetIntValue(kGroupName, "BlinkFrames");
}
