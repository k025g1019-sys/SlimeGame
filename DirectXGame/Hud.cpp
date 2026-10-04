#define NOMINMAX
#include "Hud.h"
#include "DebugTextUtil.h"
#include "Easing.h"
#include "GameConfig.h"
#include "GlobalVariables.h"
#include "MathUtil.h"
#include <algorithm>
#include <format>

using namespace KamataEngine;

namespace {
// 調整項目のグループ名
const char* kGroupName = "Hud";
// 操作説明（DebugText は ASCII のみ。爆弾は「通常状態で受けて跳ね返す」のが唯一の正解なので、それも書いておく）
const char* const kHelpText = "A/D: SPEED   SPACE(HOLD): FLATTEN   BOMB: STAY ROUND TO BOUNCE   ESC: PAUSE";
// 操作説明の帯の高さ
const float kHelpBarHeight = 64.0f;
// HPの板の余白
const float kPanelPadding = 14.0f;
// 文字の倍率と間隔（ピクセル）：「HP」の文字・フェーズ名・ボスのHPバーの下の文字、フェーズ名とバーの下の文字・バナーの副題の間隔
const float kHpLabelScale = 2.5f;
const float kPhaseNameScale = 2.0f;
const float kBossBarLabelScale = 2.0f;
const float kPhaseNameGap = 22.0f;
const float kBossBarLabelGap = 8.0f;
const float kBannerSubGap = 8.0f;

// DebugText で出せない文字（ASCII 以外・制御文字）を '?' に置き換える（phases.json の name に日本語を書かれても化けないように）
std::string ToDisplayText(const std::string& text) {
	std::string result;
	result.reserve(text.size());
	for (const char c : text) {
		const unsigned char u = static_cast<unsigned char>(c);
		if (u >= 0x80) {
			// UTF-8 の多バイト文字は先頭バイトだけ '?' にする（続きのバイトは捨てる）
			if (u >= 0xC0) {
				result.push_back('?');
			}
			continue;
		}
		result.push_back((u < 0x20) ? '?' : c);
	}
	return result;
}
} // namespace

Hud::~Hud() {
	delete hpPanelSprite_;
	for (Sprite* sprite : hpPipSprites_) {
		delete sprite;
	}
	delete bossBarBackSprite_;
	delete bossBarFillSprite_;
	delete helpBarSprite_;
	delete bannerBandSprite_;
}

void Hud::Initialize() {
	// すべて白い1pxテクスチャを引き伸ばして色を付ける（位置・大きさ・色は Update で毎フレーム反映）
	const uint32_t white = TextureManager::Load("white1x1.png");
	hpPanelSprite_ = Sprite::Create(white, {0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.45f});
	for (Sprite*& sprite : hpPipSprites_) {
		sprite = Sprite::Create(white, {0.0f, 0.0f});
	}
	bossBarBackSprite_ = Sprite::Create(white, {0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.6f});
	bossBarFillSprite_ = Sprite::Create(white, {0.0f, 0.0f});
	helpBarSprite_ = Sprite::Create(white, {0.0f, static_cast<float>(GameConfig::kScreenHeight) - kHelpBarHeight}, {0.0f, 0.0f, 0.0f, 0.45f});
	helpBarSprite_->SetSize({static_cast<float>(GameConfig::kScreenWidth), kHelpBarHeight});
	bannerBandSprite_ = Sprite::Create(white, {0.0f, 0.0f});
}

void Hud::Update(const Info& info) {
	info_ = info;
	const int maxHp = std::clamp(info_.maxHp, 1, kMaxHpPips);

	// HPの板と四角（"HP" の文字の右に並べる。文字2つ＋空き1つ分）
	const float labelWidth = kDebugTextCharWidth * kHpLabelScale * 3.0f;
	const float pipsX = kHpPosition.x + labelWidth;
	hpPanelSprite_->SetPosition({kHpPosition.x - kPanelPadding, kHpPosition.y - kPanelPadding});
	hpPanelSprite_->SetSize({labelWidth + kHpPipSpacing * static_cast<float>(maxHp - 1) + kHpPipSize + kPanelPadding * 2.0f, kHpPipSize + kPanelPadding * 2.0f});
	for (int i = 0; i < kMaxHpPips; i++) {
		Sprite* pip = hpPipSprites_[static_cast<size_t>(i)];
		pip->SetPosition({pipsX + kHpPipSpacing * static_cast<float>(i), kHpPosition.y});
		pip->SetSize({kHpPipSize, kHpPipSize});
		const Vector3& color = (i < info_.hp) ? kHpColor : kHpEmptyColor;
		pip->SetColor({color.x, color.y, color.z, 1.0f});
	}

	// ボスのHPバー（中身は残りの割合の幅）
	const float barX = (static_cast<float>(GameConfig::kScreenWidth) - kBossBarSize.x) * 0.5f;
	bossBarBackSprite_->SetPosition({barX - 4.0f, kBossBarY - 4.0f});
	bossBarBackSprite_->SetSize({kBossBarSize.x + 8.0f, kBossBarSize.y + 8.0f});
	const float rate = std::clamp(static_cast<float>(info_.bossHp) / static_cast<float>(std::max(info_.bossMaxHp, 1)), 0.0f, 1.0f);
	bossBarFillSprite_->SetPosition({barX, kBossBarY});
	bossBarFillSprite_->SetSize({std::max(kBossBarSize.x * rate, 1.0f), kBossBarSize.y});
	bossBarFillSprite_->SetColor({kBossBarColor.x, kBossBarColor.y, kBossBarColor.z, 1.0f});

	// バナーの帯（画面の横いっぱい。ボスの見せ場はボスに重ならない位置・薄い帯）
	const float bannerY = info_.isBossBanner ? kBossBannerY : kBannerY;
	const float bandHeight = info_.isBossBanner ? kBossBannerBandHeight : kBannerBandHeight;
	const float bandAlpha = info_.isBossBanner ? kBossBannerBandAlpha : kBannerBandAlpha;
	bannerBandSprite_->SetPosition({0.0f, bannerY - bandHeight * 0.5f});
	bannerBandSprite_->SetSize({static_cast<float>(GameConfig::kScreenWidth), bandHeight});
	bannerBandSprite_->SetColor({info_.bannerColor.x * 0.35f, info_.bannerColor.y * 0.35f, info_.bannerColor.z * 0.35f, bandAlpha});
}

void Hud::Draw() {
	DebugText* debugText = DebugText::GetInstance();
	const float centerX = static_cast<float>(GameConfig::kScreenWidth) * 0.5f;
	const int maxHp = std::clamp(info_.maxHp, 1, kMaxHpPips);

	// --- 左上：HP ---
	hpPanelSprite_->Draw();
	for (int i = 0; i < maxHp; i++) {
		hpPipSprites_[static_cast<size_t>(i)]->Draw();
	}
	debugText->Print("HP", kHpPosition.x, kHpPosition.y + (kHpPipSize - kDebugTextCharHeight * kHpLabelScale) * 0.5f, kHpLabelScale);

	// --- 上中央：フェーズ（とボスのHPバー） ---
	const std::string phaseLabel = std::format("PHASE {}/{}  {}", info_.phaseNumber, info_.phaseCount, info_.isBossPhase ? "BOSS" : "WAVE");
	PrintDebugTextCentered(phaseLabel, centerX, kPhaseLabelY, kPhaseLabelScale);
	PrintDebugTextCentered(ToDisplayText(info_.phaseName), centerX, kPhaseLabelY + kDebugTextCharHeight * kPhaseLabelScale * 0.5f + kPhaseNameGap, kPhaseNameScale);
	if (info_.showBossBar) {
		bossBarBackSprite_->Draw();
		bossBarFillSprite_->Draw();
		const float barX = (static_cast<float>(GameConfig::kScreenWidth) - kBossBarSize.x) * 0.5f;
		const float labelY = kBossBarY + kBossBarSize.y + kBossBarLabelGap;
		debugText->Print(std::format("BOSS LV{}", info_.bossLevel), barX, labelY, kBossBarLabelScale);
		const std::string hpText = std::format("{}/{}", info_.bossHp, info_.bossMaxHp);
		debugText->Print(hpText, barX + kBossBarSize.x - kDebugTextCharWidth * kBossBarLabelScale * static_cast<float>(hpText.size()), labelY, kBossBarLabelScale);
	}

	// --- 中央：バナー（出た瞬間に大きく → 元の大きさへ。ボスの見せ場はボスの頭より上に小さめに出す） ---
	if (!info_.bannerText.empty()) {
		bannerBandSprite_->Draw();
		const float bannerY = info_.isBossBanner ? kBossBannerY : kBannerY;
		const float bannerScale = info_.isBossBanner ? kBossBannerScale : kBannerScale;
		const float pop = (kBannerPopTime > 0.0f) ? std::clamp(info_.bannerTime / kBannerPopTime, 0.0f, 1.0f) : 1.0f;
		const float scale = bannerScale * Lerp(1.5f, 1.0f, EaseOutCubic(pop));
		const bool hasSub = !info_.bannerSubText.empty();
		const float mainY = hasSub ? bannerY - kDebugTextCharHeight * kBannerSubScale * 0.8f : bannerY;
		PrintDebugTextCentered(ToDisplayText(info_.bannerText), centerX, mainY, scale);
		if (hasSub) {
			PrintDebugTextCentered(ToDisplayText(info_.bannerSubText), centerX, bannerY + kDebugTextCharHeight * bannerScale * 0.5f + kBannerSubGap, kBannerSubScale);
		}
	}

	// --- 下：操作説明 ---
	helpBarSprite_->Draw();
	PrintDebugTextCentered(kHelpText, centerX, static_cast<float>(GameConfig::kScreenHeight) - kHelpBarHeight * 0.5f, kHelpScale);
}

void Hud::RegisterGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	globalVariables->CreateGroup(kGroupName);
	globalVariables->AddItem(kGroupName, "HpPosition", kHpPosition);
	globalVariables->AddItem(kGroupName, "HpPipSize", kHpPipSize);
	globalVariables->AddItem(kGroupName, "HpPipSpacing", kHpPipSpacing);
	globalVariables->AddItem(kGroupName, "HpColor", kHpColor);
	globalVariables->AddItem(kGroupName, "HpEmptyColor", kHpEmptyColor);
	globalVariables->AddItem(kGroupName, "PhaseLabelY", kPhaseLabelY);
	globalVariables->AddItem(kGroupName, "PhaseLabelScale", kPhaseLabelScale);
	globalVariables->AddItem(kGroupName, "BossBarY", kBossBarY);
	globalVariables->AddItem(kGroupName, "BossBarSize", kBossBarSize);
	globalVariables->AddItem(kGroupName, "BossBarColor", kBossBarColor);
	globalVariables->AddItem(kGroupName, "BannerY", kBannerY);
	globalVariables->AddItem(kGroupName, "BannerScale", kBannerScale);
	globalVariables->AddItem(kGroupName, "BannerSubScale", kBannerSubScale);
	globalVariables->AddItem(kGroupName, "BannerBandHeight", kBannerBandHeight);
	globalVariables->AddItem(kGroupName, "BannerBandAlpha", kBannerBandAlpha);
	globalVariables->AddItem(kGroupName, "BannerPopTime", kBannerPopTime);
	globalVariables->AddItem(kGroupName, "BossBannerY", kBossBannerY);
	globalVariables->AddItem(kGroupName, "BossBannerScale", kBossBannerScale);
	globalVariables->AddItem(kGroupName, "BossBannerBandHeight", kBossBannerBandHeight);
	globalVariables->AddItem(kGroupName, "BossBannerBandAlpha", kBossBannerBandAlpha);
	globalVariables->AddItem(kGroupName, "HelpScale", kHelpScale);
}

void Hud::ApplyGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	kHpPosition = globalVariables->GetVector3Value(kGroupName, "HpPosition");
	kHpPipSize = std::max(globalVariables->GetFloatValue(kGroupName, "HpPipSize"), 1.0f);
	kHpPipSpacing = std::max(globalVariables->GetFloatValue(kGroupName, "HpPipSpacing"), 0.0f);
	kHpColor = globalVariables->GetVector3Value(kGroupName, "HpColor");
	kHpEmptyColor = globalVariables->GetVector3Value(kGroupName, "HpEmptyColor");
	kPhaseLabelY = globalVariables->GetFloatValue(kGroupName, "PhaseLabelY");
	kPhaseLabelScale = std::max(globalVariables->GetFloatValue(kGroupName, "PhaseLabelScale"), 0.1f);
	kBossBarY = globalVariables->GetFloatValue(kGroupName, "BossBarY");
	kBossBarSize = globalVariables->GetVector3Value(kGroupName, "BossBarSize");
	kBossBarSize.x = std::max(kBossBarSize.x, 1.0f);
	kBossBarSize.y = std::max(kBossBarSize.y, 1.0f);
	kBossBarColor = globalVariables->GetVector3Value(kGroupName, "BossBarColor");
	kBannerY = globalVariables->GetFloatValue(kGroupName, "BannerY");
	kBannerScale = std::max(globalVariables->GetFloatValue(kGroupName, "BannerScale"), 0.1f);
	kBannerSubScale = std::max(globalVariables->GetFloatValue(kGroupName, "BannerSubScale"), 0.1f);
	kBannerBandHeight = std::max(globalVariables->GetFloatValue(kGroupName, "BannerBandHeight"), 0.0f);
	kBannerBandAlpha = std::clamp(globalVariables->GetFloatValue(kGroupName, "BannerBandAlpha"), 0.0f, 1.0f);
	kBannerPopTime = std::max(globalVariables->GetFloatValue(kGroupName, "BannerPopTime"), 0.0f);
	kBossBannerY = globalVariables->GetFloatValue(kGroupName, "BossBannerY");
	kBossBannerScale = std::max(globalVariables->GetFloatValue(kGroupName, "BossBannerScale"), 0.1f);
	kBossBannerBandHeight = std::max(globalVariables->GetFloatValue(kGroupName, "BossBannerBandHeight"), 0.0f);
	kBossBannerBandAlpha = std::clamp(globalVariables->GetFloatValue(kGroupName, "BossBannerBandAlpha"), 0.0f, 1.0f);
	kHelpScale = std::max(globalVariables->GetFloatValue(kGroupName, "HelpScale"), 0.1f);
}
