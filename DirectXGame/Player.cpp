#define NOMINMAX
#include "Player.h"
#include "GameConfig.h"
#include "GameModels.h"
#include "GlobalVariables.h"
#include "Hud.h"
#include "MathUtil.h"
#include <algorithm>
#include <cmath>
#include <numbers>

using namespace KamataEngine;

namespace {
// 調整項目のグループ名
const char* kGroupName = "Player";
constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;
// 見た目の平らさの範囲（バネの揺れで行き過ぎても形が壊れないように）
const float kMinFlatness = -0.4f;
const float kMaxFlatness = 1.35f;
// 拡縮の下限（0にすると逆行列が壊れる）
const float kMinScale = 0.03f;
// 敗北演出：つぶれる時間・消える時間（秒）
const float kDefeatSquashTime = 0.5f;
const float kDefeatFadeTime = 1.2f;
} // namespace

void Player::Initialize(const GameModels* models) {
	models_ = models;
	root_.Initialize();
	body_.Initialize();
	core_.Initialize();
	for (WorldTransform& eye : eyes_) {
		eye.Initialize();
		eye.parent_ = &root_;
	}
	body_.parent_ = &root_;
	core_.parent_ = &root_;
	bodyColor_.Initialize();
	coreColor_.Initialize();
	eyeColor_.Initialize();
	Reset(0.0f);
}

float Player::GetStartTheta() { return Field::WrapAngle(kStartAngleDeg * kDegToRad); }

void Player::SetTheta(float theta) { theta_ = Field::WrapAngle(theta); }

void Player::Reset(float z) {
	theta_ = GetStartTheta();
	z_ = z;
	speed_ = kBaseSpeed;
	isFlat_ = false;
	flattenTriggered_ = false;
	restoreTriggered_ = false;
	flatness_ = 0.0f;
	flatnessVelocity_ = 0.0f;
	hp_ = kMaxHp;
	invincibleTimer_ = 0.0f;
	blinkFrame_ = 0;
	defeated_ = false;
	defeatTimer_ = 0.0f;
	// 開始時に平らボタン（=決定キー）が押しっぱなしなら、一度離すまで平らにしない
	waitRelease_ = true;
}

void Player::Update(float moveX, bool flattenHeld, bool canMove) {
	const float dt = GameConfig::kDeltaTime;
	blinkFrame_++;
	invincibleTimer_ = std::max(invincibleTimer_ - dt, 0.0f);

	// --- 平ら（押している間）／元に戻る（離した瞬間） ---
	if (!flattenHeld) {
		waitRelease_ = false;
	}
	const bool wasFlat = isFlat_;
	isFlat_ = flattenHeld && !waitRelease_ && !defeated_;
	flattenTriggered_ = isFlat_ && !wasFlat;
	restoreTriggered_ = !isFlat_ && wasFlat;

	// --- 速さ：基本＋入力で調整（下限があるので後退しない）。目標速度へなめらかに追従 ---
	float targetSpeed = 0.0f;
	if (canMove && !defeated_) {
		targetSpeed = std::max(kBaseSpeed + moveX * kSpeedAdjust, kMinSpeed);
		if (isFlat_) {
			targetSpeed *= kFlatSpeedScale;
		}
	}
	speed_ += (targetSpeed - speed_) * std::clamp(kSpeedResponse * dt, 0.0f, 1.0f);
	theta_ = Field::WrapAngle(theta_ + speed_ * dt / Field::kRingRadius);

	// --- 見た目の平らさ：バネで目標（通常0 / 平ら1）へ追従（行き過ぎてぷるんと揺れる） ---
	const float target = (isFlat_ || defeated_) ? 1.0f : 0.0f;
	const float acceleration = kSpringStiffness * (target - flatness_) - kSpringDamping * flatnessVelocity_;
	flatnessVelocity_ += acceleration * dt;
	flatness_ += flatnessVelocity_ * dt;
	if (flatness_ < kMinFlatness || flatness_ > kMaxFlatness) {
		flatness_ = std::clamp(flatness_, kMinFlatness, kMaxFlatness);
		flatnessVelocity_ = 0.0f;
	}

	if (defeated_) {
		defeatTimer_ += dt;
	}
}

float Player::GetAngularSpeed() const { return speed_ / Field::kRingRadius; }

Footprint Player::GetFootprint() const {
	if (isFlat_) {
		return GetFlatFootprint();
	}
	return {theta_, kRadius, 0.0f, kRadius * 2.0f * kNormalHeightScale};
}

Footprint Player::GetFlatFootprint() const { return {theta_, kRadius * kFlatWidthScale, 0.0f, kFlatTop}; }

Vector3 Player::GetBodyScale() const {
	// 通常 (r, r×縦倍率, r) と 平ら (r×横倍率, r×高さ倍率, r×横倍率) の間をバネの値で補間（範囲外は伸び縮みの揺れ）
	const Vector3 normal = {kRadius, kRadius * kNormalHeightScale, kRadius};
	const Vector3 flat = {kRadius * kFlatWidthScale, kRadius * kFlatHeightScale, kRadius * kFlatWidthScale};
	Vector3 scale = Lerp(normal, flat, flatness_);
	// 敗北演出：さらにつぶれる
	if (defeated_) {
		const float squash = std::clamp(defeatTimer_ / kDefeatSquashTime, 0.0f, 1.0f);
		scale.y *= Lerp(1.0f, 0.3f, squash);
		scale.x *= Lerp(1.0f, 1.3f, squash);
		scale.z *= Lerp(1.0f, 1.3f, squash);
	}
	return {std::max(scale.x, kMinScale), std::max(scale.y, kMinScale), std::max(scale.z, kMinScale)};
}

float Player::GetRiderHeight() const { return GetBodyScale().y * 2.0f; }

float Player::GetRiderHalfAngle() const { return GetBodyScale().x * kRiderHalfWidthScale / Field::kRingRadius; }

Vector3 Player::GetCenterWorld() const { return Field::PolarToWorld(theta_, GetBodyScale().y, z_); }

bool Player::Damage(int amount) {
	if (IsInvincible() || defeated_ || amount <= 0) {
		return false;
	}
	hp_ = std::max(hp_ - amount, 0);
	invincibleTimer_ = kInvincibleTime;
	flatnessVelocity_ += kDamageImpulse;
	return true;
}

void Player::Heal(int amount) {
	if (amount > 0) {
		hp_ = std::min(hp_ + amount, kMaxHp);
	}
}

void Player::StartDefeat() {
	defeated_ = true;
	defeatTimer_ = 0.0f;
	invincibleTimer_ = 0.0f;
}

void Player::UpdateVisual() {
	// --- root：輪の上の位置と向き（ローカル+Y=上、+X=進む向き、−Z=カメラ側） ---
	root_.translation_ = Field::PolarToWorld(theta_, 0.0f, z_);
	root_.rotation_ = {0.0f, 0.0f, Field::SurfaceRotationZ(theta_)};
	UpdateWorldTransform(root_);

	// --- 体（半径1の球を楕円体に。接地するよう中心を半分の高さに置く）と芯 ---
	const Vector3 bodyScale = GetBodyScale();
	body_.translation_ = {0.0f, bodyScale.y, 0.0f};
	body_.scale_ = bodyScale;
	UpdateWorldTransform(body_);
	core_.translation_ = body_.translation_;
	core_.scale_ = bodyScale * 0.45f;
	UpdateWorldTransform(core_);

	// --- 目：カメラ側の面のすぐ外に、進む向きへ少し寄せて置く（楕円体の表面の位置を計算する） ---
	const float eyeRadius = kRadius * kEyeSize;
	for (int i = 0; i < 2; i++) {
		const float side = (i == 0) ? -1.0f : 1.0f;
		const float localX = bodyScale.x * (0.3f + side * 0.22f);
		const float localY = bodyScale.y * 1.3f;
		// 楕円体の表面の z（中心からの位置 (x, y) での手前側の面）
		const float nx = localX / bodyScale.x;
		const float ny = (localY - bodyScale.y) / bodyScale.y;
		const float nz = std::sqrt(std::max(1.0f - nx * nx - ny * ny, 0.05f));
		WorldTransform& eye = eyes_[i];
		eye.translation_ = {localX, localY, -bodyScale.z * nz - eyeRadius * 0.2f};
		// 平らになると目も少しつぶれる
		const float squash = std::clamp(bodyScale.y / (kRadius * kNormalHeightScale), 0.5f, 1.2f);
		eye.scale_ = {eyeRadius, eyeRadius * squash, eyeRadius};
		UpdateWorldTransform(eye);
	}

	// --- 色（無敵中は点滅。敗北演出では薄くなって消える） ---
	float alpha = kAlpha;
	if (invincibleTimer_ > 0.0f && (blinkFrame_ / 4) % 2 == 0) {
		alpha *= 0.3f;
	}
	if (defeated_) {
		alpha *= 1.0f - std::clamp((defeatTimer_ - kDefeatSquashTime) / kDefeatFadeTime, 0.0f, 1.0f);
	}
	bodyColor_.SetColor({kColor.x, kColor.y, kColor.z, std::clamp(alpha, 0.0f, 1.0f)});
	coreColor_.SetColor({kCoreColor.x, kCoreColor.y, kCoreColor.z, 1.0f});
	eyeColor_.SetColor({0.02f, 0.02f, 0.03f, 1.0f});
}

void Player::DrawOpaque(const Camera& camera) const {
	// 無敵中の点滅で消えているフレームと、敗北演出で消えた後は描かない
	const bool blinkHidden = invincibleTimer_ > 0.0f && (blinkFrame_ / 4) % 2 == 0;
	if (blinkHidden || (defeated_ && defeatTimer_ > kDefeatSquashTime)) {
		return;
	}
	models_->sphere->Draw(core_, camera, &coreColor_);
	for (const WorldTransform& eye : eyes_) {
		models_->sphereLow->Draw(eye, camera, &eyeColor_);
	}
}

void Player::DrawTranslucent(const Camera& camera) const { models_->sphere->Draw(body_, camera, &bodyColor_); }

void Player::RegisterGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	globalVariables->CreateGroup(kGroupName);
	globalVariables->AddItem(kGroupName, "StartAngleDeg", kStartAngleDeg);
	globalVariables->AddItem(kGroupName, "Radius", kRadius);
	globalVariables->AddItem(kGroupName, "NormalHeightScale", kNormalHeightScale);
	globalVariables->AddItem(kGroupName, "FlatWidthScale", kFlatWidthScale);
	globalVariables->AddItem(kGroupName, "FlatHeightScale", kFlatHeightScale);
	globalVariables->AddItem(kGroupName, "FlatTop", kFlatTop);
	globalVariables->AddItem(kGroupName, "RiderHalfWidthScale", kRiderHalfWidthScale);
	globalVariables->AddItem(kGroupName, "BaseSpeed", kBaseSpeed);
	globalVariables->AddItem(kGroupName, "SpeedAdjust", kSpeedAdjust);
	globalVariables->AddItem(kGroupName, "MinSpeed", kMinSpeed);
	globalVariables->AddItem(kGroupName, "SpeedResponse", kSpeedResponse);
	globalVariables->AddItem(kGroupName, "FlatSpeedScale", kFlatSpeedScale);
	globalVariables->AddItem(kGroupName, "SpringStiffness", kSpringStiffness);
	globalVariables->AddItem(kGroupName, "SpringDamping", kSpringDamping);
	globalVariables->AddItem(kGroupName, "BounceImpulse", kBounceImpulse);
	globalVariables->AddItem(kGroupName, "DamageImpulse", kDamageImpulse);
	globalVariables->AddItem(kGroupName, "MaxHp", kMaxHp);
	globalVariables->AddItem(kGroupName, "InvincibleTime", kInvincibleTime);
	globalVariables->AddItem(kGroupName, "PhaseClearHeal", kPhaseClearHeal);
	globalVariables->AddItem(kGroupName, "SpikeEatHeal", kSpikeEatHeal);
	globalVariables->AddItem(kGroupName, "BombAbsorbDamage", kBombAbsorbDamage);
	globalVariables->AddItem(kGroupName, "EnemyStickToFlat", kEnemyStickToFlat);
	globalVariables->AddItem(kGroupName, "SpikeStickToFlat", kSpikeStickToFlat);
	globalVariables->AddItem(kGroupName, "Color", kColor);
	globalVariables->AddItem(kGroupName, "Alpha", kAlpha);
	globalVariables->AddItem(kGroupName, "CoreColor", kCoreColor);
	globalVariables->AddItem(kGroupName, "EyeSize", kEyeSize);
}

void Player::ApplyGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	kStartAngleDeg = globalVariables->GetFloatValue(kGroupName, "StartAngleDeg");
	// 大きさは0にしない（拡縮0で逆行列が壊れる・判定が消える）
	kRadius = std::max(globalVariables->GetFloatValue(kGroupName, "Radius"), 0.1f);
	kNormalHeightScale = std::max(globalVariables->GetFloatValue(kGroupName, "NormalHeightScale"), 0.05f);
	kFlatWidthScale = std::max(globalVariables->GetFloatValue(kGroupName, "FlatWidthScale"), 0.05f);
	kFlatHeightScale = std::max(globalVariables->GetFloatValue(kGroupName, "FlatHeightScale"), 0.05f);
	kFlatTop = std::max(globalVariables->GetFloatValue(kGroupName, "FlatTop"), 0.05f);
	kRiderHalfWidthScale = std::clamp(globalVariables->GetFloatValue(kGroupName, "RiderHalfWidthScale"), 0.0f, 1.5f);
	kBaseSpeed = std::max(globalVariables->GetFloatValue(kGroupName, "BaseSpeed"), 0.0f);
	kSpeedAdjust = std::max(globalVariables->GetFloatValue(kGroupName, "SpeedAdjust"), 0.0f);
	// 下限速度は0より大きく（仕様：後退はしない・止まらない）
	kMinSpeed = std::max(globalVariables->GetFloatValue(kGroupName, "MinSpeed"), 0.01f);
	kSpeedResponse = std::max(globalVariables->GetFloatValue(kGroupName, "SpeedResponse"), 0.0f);
	kFlatSpeedScale = std::max(globalVariables->GetFloatValue(kGroupName, "FlatSpeedScale"), 0.0f);
	// バネは硬すぎると 1/60 秒の積分が発散するので上限を付ける
	kSpringStiffness = std::clamp(globalVariables->GetFloatValue(kGroupName, "SpringStiffness"), 1.0f, 3000.0f);
	kSpringDamping = std::clamp(globalVariables->GetFloatValue(kGroupName, "SpringDamping"), 0.0f, 100.0f);
	kBounceImpulse = globalVariables->GetFloatValue(kGroupName, "BounceImpulse");
	kDamageImpulse = globalVariables->GetFloatValue(kGroupName, "DamageImpulse");
	// HP はHUDの四角の数（Hud::kMaxHpPips）まで（上限を変えるときは Hud 側の定数を変える）
	kMaxHp = std::clamp(globalVariables->GetIntValue(kGroupName, "MaxHp"), 1, Hud::kMaxHpPips);
	kInvincibleTime = std::max(globalVariables->GetFloatValue(kGroupName, "InvincibleTime"), 0.0f);
	kPhaseClearHeal = std::max(globalVariables->GetIntValue(kGroupName, "PhaseClearHeal"), 0);
	kSpikeEatHeal = std::max(globalVariables->GetIntValue(kGroupName, "SpikeEatHeal"), 0);
	kBombAbsorbDamage = std::max(globalVariables->GetIntValue(kGroupName, "BombAbsorbDamage"), 0);
	kEnemyStickToFlat = globalVariables->GetBoolValue(kGroupName, "EnemyStickToFlat");
	kSpikeStickToFlat = globalVariables->GetBoolValue(kGroupName, "SpikeStickToFlat");
	kColor = globalVariables->GetVector3Value(kGroupName, "Color");
	kAlpha = std::clamp(globalVariables->GetFloatValue(kGroupName, "Alpha"), 0.0f, 1.0f);
	kCoreColor = globalVariables->GetVector3Value(kGroupName, "CoreColor");
	kEyeSize = std::max(globalVariables->GetFloatValue(kGroupName, "EyeSize"), 0.01f);
}
