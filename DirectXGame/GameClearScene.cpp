#define NOMINMAX
#include "GameClearScene.h"
#include "DebugTextUtil.h"
#include "GameConfig.h"
#include "GameInput.h"
#include "GlobalVariables.h"
#include "Sounds.h"
#include <algorithm>
#include <cmath>
#include <numbers>
using namespace KamataEngine;

namespace {
// 調整項目のグループ名
const char* kGroupName = "Clear";
} // namespace

GameClearScene::~GameClearScene() { delete backgroundSprite_; }

void GameClearScene::Initialize() {
	// 画面全体を覆う背景の板（白い1pxテクスチャを引き伸ばして色を付ける）
	backgroundSprite_ = Sprite::Create(TextureManager::Load("white1x1.png"), {0.0f, 0.0f});
	backgroundSprite_->SetSize({static_cast<float>(GameConfig::kScreenWidth), static_cast<float>(GameConfig::kScreenHeight)});

	fade_.Initialize();
	fade_.Start(Fade::Status::kFadeIn, kFadeDuration);

	// クリアのファンファーレ（シーン開始時に1回だけ鳴らす。ファイルが無ければ鳴らないだけ）
	PlaySe(seFanfare);
}

void GameClearScene::Update() {
	// ImGuiでの変更を毎フレーム反映する
	ApplyGlobalVariables();

	frameCounter_++;

	// 背景色の反映（ImGuiで変えた瞬間に効くよう毎フレーム設定する）
	backgroundSprite_->SetColor({kBackgroundColor.x, kBackgroundColor.y, kBackgroundColor.z, 1.0f});

	fade_.Update();

	switch (phase_) {
	case Phase::kFadeIn:
		if (fade_.IsFinished()) {
			phase_ = Phase::kMain;
		}
		break;
	case Phase::kMain:
		// 決定でタイトルへ（フェードアウトを開始する）
		if (GameInput::GetInstance()->IsTriggered(GameInput::Action::kDecide)) {
			PlaySe(seDecide);
			phase_ = Phase::kFadeOut;
			fade_.Start(Fade::Status::kFadeOut, kFadeDuration);
		}
		break;
	case Phase::kFadeOut:
		if (fade_.IsFinished()) {
			isFinished_ = true;
		}
		break;
	}
}

void GameClearScene::Draw() {
	Sprite::PreDraw(DirectXCommon::GetInstance()->GetCommandList());
	backgroundSprite_->Draw();

	// 見出しはゆっくり上下に揺らす（2秒周期）
	const float bob = std::sin(static_cast<float>(frameCounter_) * (2.0f * std::numbers::pi_v<float> / 120.0f)) * kTitleBobAmplitude;
	PrintDebugTextCentered("GAME CLEAR", kTitlePosition.x, kTitlePosition.y + bob, kTitleScale);
	PrintDebugTextCentered("PRESS SPACE TO TITLE", kPressPosition.x, kPressPosition.y, kPressScale);
	DebugText::GetInstance()->DrawAll();
	Sprite::PostDraw();

	// フェードは最前面
	fade_.Draw();
}

void GameClearScene::RegisterGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	globalVariables->CreateGroup(kGroupName);
	globalVariables->AddItem(kGroupName, "FadeDuration", kFadeDuration);
	globalVariables->AddItem(kGroupName, "BackgroundColor", kBackgroundColor);
	globalVariables->AddItem(kGroupName, "TitlePosition", kTitlePosition);
	globalVariables->AddItem(kGroupName, "TitleScale", kTitleScale);
	globalVariables->AddItem(kGroupName, "TitleBobAmplitude", kTitleBobAmplitude);
	globalVariables->AddItem(kGroupName, "PressPosition", kPressPosition);
	globalVariables->AddItem(kGroupName, "PressScale", kPressScale);
}

void GameClearScene::ApplyGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	// フェード時間は0にすると割り算が壊れるので下限を付ける
	kFadeDuration = std::max(globalVariables->GetFloatValue(kGroupName, "FadeDuration"), 0.01f);
	kBackgroundColor = globalVariables->GetVector3Value(kGroupName, "BackgroundColor");
	kTitlePosition = globalVariables->GetVector3Value(kGroupName, "TitlePosition");
	kTitleScale = globalVariables->GetFloatValue(kGroupName, "TitleScale");
	kTitleBobAmplitude = globalVariables->GetFloatValue(kGroupName, "TitleBobAmplitude");
	kPressPosition = globalVariables->GetVector3Value(kGroupName, "PressPosition");
	kPressScale = globalVariables->GetFloatValue(kGroupName, "PressScale");
}
