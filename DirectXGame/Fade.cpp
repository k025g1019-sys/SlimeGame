#define NOMINMAX
#include "Fade.h"
#include "GameConfig.h"
#include <algorithm>
using namespace KamataEngine;

Fade::~Fade() { delete sprite_; }

void Fade::Initialize() {
	// 白い1pxテクスチャを読み込み、色を黒くして画面全体に引き伸ばして使う
	textureHandle_ = TextureManager::Load("white1x1.png");
	sprite_ = Sprite::Create(textureHandle_, {0.0f, 0.0f});
	// 画面全体を覆うサイズにする
	sprite_->SetSize({static_cast<float>(GameConfig::kScreenWidth), static_cast<float>(GameConfig::kScreenHeight)});
	// 初期は黒・不透明
	sprite_->SetColor({0.0f, 0.0f, 0.0f, 1.0f});
}

void Fade::Start(Status status, float duration) {
	status_ = status;
	duration_ = duration;
	counter_ = 0.0f;
}

void Fade::Stop() { status_ = Status::kNone; }

void Fade::Update() {
	switch (status_) {
	case Status::kNone:
		// フェードなしの間は何もしない
		return;
	case Status::kFadeIn:
		// 経過時間を進める（規定時間で頭打ち）
		counter_ += 1.0f / 60.0f;
		if (counter_ >= duration_) {
			counter_ = duration_;
		}
		// 黒→透明（アルファ 1→0）
		sprite_->SetColor({0.0f, 0.0f, 0.0f, std::clamp(1.0f - counter_ / duration_, 0.0f, 1.0f)});
		break;
	case Status::kFadeOut:
		// 経過時間を進める（規定時間で頭打ち）
		counter_ += 1.0f / 60.0f;
		if (counter_ >= duration_) {
			counter_ = duration_;
		}
		// 透明→黒（アルファ 0→1）
		sprite_->SetColor({0.0f, 0.0f, 0.0f, std::clamp(counter_ / duration_, 0.0f, 1.0f)});
		break;
	}
}

void Fade::Draw() {
	// フェードなし（＝スプライトが透明）の間は描画しない
	if (status_ == Status::kNone) {
		return;
	}

	Sprite::PreDraw(DirectXCommon::GetInstance()->GetCommandList());
	sprite_->Draw();
	Sprite::PostDraw();
}

bool Fade::IsFinished() const {
	switch (status_) {
	case Status::kFadeIn:
	case Status::kFadeOut:
		// 規定時間に達したら終了
		return counter_ >= duration_;
	case Status::kNone:
	default:
		return true;
	}
}
