#include "PauseMenu.h"
#include "DebugTextUtil.h"
#include "GameConfig.h"
#include "GameInput.h"
#include "GlobalVariables.h"
using namespace KamataEngine;

namespace {
// 調整項目のグループ名
const char* kGroupName = "Pause";
// カーソル移動とみなす上下入力の閾値
const float kCursorMoveThreshold = 0.5f;
// 項目の表示名（DebugTextはASCIIのみ。並びはカーソル位置と同じ）
const char* const kItemLabels[] = {"RESUME", "TO TITLE"};
} // namespace

PauseMenu::~PauseMenu() {
	delete overlaySprite_;
	for (Sprite* sprite : itemSprites_) {
		delete sprite;
	}
}

void PauseMenu::Initialize() {
	// 白い1pxテクスチャを色付きの板に引き伸ばして使う（仮UI。本番は画像に差し替える）
	const uint32_t textureHandle = TextureManager::Load("white1x1.png");

	// 画面全体を暗くする黒いオーバーレイ
	overlaySprite_ = Sprite::Create(textureHandle, {0.0f, 0.0f});
	overlaySprite_->SetSize({static_cast<float>(GameConfig::kScreenWidth), static_cast<float>(GameConfig::kScreenHeight)});
	overlaySprite_->SetColor({0.0f, 0.0f, 0.0f, 0.55f});

	// 項目の背景の箱（中心基準。位置・大きさ・色は Update で毎フレーム反映する）
	for (Sprite*& sprite : itemSprites_) {
		sprite = Sprite::Create(textureHandle, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.5f, 0.5f});
	}
}

void PauseMenu::Open() {
	isOpen_ = true;
	cursorIndex_ = 0;
	// 開いた瞬間の入力を「押した瞬間」と誤検出しないように記録しておく
	previousUiMoveY_ = GameInput::GetInstance()->GetUiMoveY();
	// 開いたフレームから正しい配置で描けるよう、見た目をすぐ反映する
	UpdateLayout();
}

void PauseMenu::UpdateLayout() {
	// 選択中は少し大きく・明るい色、非選択は暗い色で透明度を下げる
	for (int32_t i = 0; i < kItemCount; i++) {
		const bool isSelected = (i == cursorIndex_);
		const float scale = isSelected ? kSelectedScale : 1.0f;
		const Vector3& color = isSelected ? kSelectedColor : kUnselectedColor;
		itemSprites_[i]->SetPosition({kItemPosition.x, kItemPosition.y + kItemSpacing * static_cast<float>(i)});
		itemSprites_[i]->SetSize({kItemSize.x * scale, kItemSize.y * scale});
		itemSprites_[i]->SetColor({color.x, color.y, color.z, isSelected ? 1.0f : kUnselectedAlpha});
	}
}

int32_t PauseMenu::GetCursorMove() {
	// 上下入力の「入れた瞬間」でカーソルを動かす（押しっぱなしでは動かない）
	const float uiMoveY = GameInput::GetInstance()->GetUiMoveY();
	int32_t move = 0;
	if (uiMoveY > kCursorMoveThreshold && previousUiMoveY_ <= kCursorMoveThreshold) {
		move = -1; // 上へ
	}
	if (uiMoveY < -kCursorMoveThreshold && previousUiMoveY_ >= -kCursorMoveThreshold) {
		move = +1; // 下へ
	}
	previousUiMoveY_ = uiMoveY;
	return move;
}

PauseMenu::Choice PauseMenu::Update() {
	if (!isOpen_) {
		return Choice::kNone;
	}

	GameInput* input = GameInput::GetInstance();

	// カーソル移動
	cursorIndex_ = (cursorIndex_ + GetCursorMove() + kItemCount) % kItemCount;

	// 項目の見た目を反映する（調整項目の変更もここで即反映される）
	UpdateLayout();

	// 決定でカーソル位置の操作を実行する
	if (input->IsTriggered(GameInput::Action::kDecide)) {
		switch (cursorIndex_) {
		case 0: // ゲームに戻る
			Close();
			return Choice::kResume;
		case 1: // タイトルへ戻る
			Close();
			return Choice::kToTitle;
		default:
			return Choice::kNone;
		}
	}

	// メニューボタンをもう一度押したら閉じる（つづける扱い）
	if (input->IsTriggered(GameInput::Action::kMenu)) {
		Close();
		return Choice::kResume;
	}

	return Choice::kNone;
}

void PauseMenu::Draw() {
	if (!isOpen_) {
		return;
	}

	// それまでに予約された文字（HUDなど）はオーバーレイより奥に描く
	DebugText::GetInstance()->DrawAll();

	// 画面を暗くしてから、項目の箱を重ねる
	overlaySprite_->Draw();
	for (Sprite* sprite : itemSprites_) {
		sprite->Draw();
	}

	// 見出しと項目の文字（箱より手前に出すため、ここで DrawAll して確定させる）
	PrintDebugTextCentered("PAUSE", kTitlePosition.x, kTitlePosition.y, kTitleScale);
	for (int32_t i = 0; i < kItemCount; i++) {
		const float labelScale = kLabelScale * ((i == cursorIndex_) ? kSelectedScale : 1.0f);
		PrintDebugTextCentered(kItemLabels[i], kItemPosition.x, kItemPosition.y + kItemSpacing * static_cast<float>(i), labelScale);
	}
	DebugText::GetInstance()->DrawAll();
}

void PauseMenu::RegisterGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	globalVariables->CreateGroup(kGroupName);
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

void PauseMenu::ApplyGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
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
