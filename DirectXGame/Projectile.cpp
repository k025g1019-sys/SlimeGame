#define NOMINMAX
#include "Projectile.h"
#include "Easing.h"
#include "GameConfig.h"
#include "GameModels.h"
#include "GlobalVariables.h"
#include "LineDrawer.h"
#include "MathUtil.h"
#include <algorithm>
#include <cmath>
#include <numbers>

using namespace KamataEngine;

namespace {
// 調整項目のグループ名
const char* kGroupName = "Projectile";
constexpr float kPi = std::numbers::pi_v<float>;
// 予告マーカーの外側の円（2重にして太く見せる）・縮む内側の円の分割数（合計 Projectile::kMarkerLineCount 本）
const int kMarkerOuterSegments = 16;
const int kMarkerInnerSegments = 12;
static_assert(kMarkerOuterSegments * 2 + kMarkerInnerSegments == Projectile::kMarkerLineCount, "予告マーカーの線の本数を変えたら kMarkerLineCount（と LineDrawer.h の見積もり）も直す");
// 外側の2重の円の間隔（輪の上の長さ。線を太く見せる）
const float kMarkerDoubleGap = 0.07f;
// 吸い込まれたときの最小の大きさ（0にはしない＝逆行列が壊れるため）
const float kMinAbsorbScale = 0.12f;
} // namespace

void Projectile::Initialize(const GameModels* models) {
	models_ = models;
	for (WorldTransform& worldTransform : transforms_) {
		worldTransform.Initialize();
	}
	for (ObjectColor& color : colors_) {
		color.Initialize();
	}
}

float Projectile::GetRadius() const { return IsBomb() ? kBombRadius : kSpikeRadius; }

Footprint Projectile::GetFootprint() const {
	const float radius = GetRadius();
	return {theta_, radius, height_ - radius, height_ + radius};
}

void Projectile::Spawn(const ProjectileSpawn& spawn, float layerZ) {
	kind_ = spawn.kind;
	layerZ_ = layerZ;
	theta_ = Field::WrapAngle(spawn.theta);
	dir_ = (spawn.dir >= 0) ? 1 : -1;
	// 地上を進むときの中心の高さ（トゲは少し浮いて滑る、爆弾は接地して転がる）
	height_ = IsBomb() ? kBombRadius : kSpikeHeight;
	traveled_ = 0.0f;
	age_ = 0.0f;
	fuse_ = kBombFuse;
	landPause_ = 0.0f;
	flightRate_ = 0.0f;
	absorbScale_ = 1.0f;
	captureOffset_ = 0.0f;
	spin_ = 0.0f;
	if (spawn.fromAir) {
		state_ = State::kAir;
		flightStart_ = spawn.airStart;
		position_ = flightStart_;
	} else {
		state_ = State::kSurface;
		position_ = Field::PolarToWorld(theta_, height_, layerZ_);
	}
}

Projectile::Event Projectile::Update(float playerTheta, const Vector3& absorbTarget, const Vector3& bossTarget) {
	const float dt = GameConfig::kDeltaTime;
	age_ += dt;
	spin_ += dt * 9.0f;

	switch (state_) {
	case State::kAir: {
		// 出発点から着弾点へ直線で飛ぶ
		flightRate_ += dt / kAirTravelTime;
		const Vector3 landing = Field::PolarToWorld(theta_, height_, layerZ_);
		if (flightRate_ >= 1.0f) {
			// 着弾：以後は地上弾としてスライムに近い向きへ進む（重なっていれば GameScene の通常の判定で処理される）
			state_ = State::kSurface;
			position_ = landing;
			dir_ = Field::DirectionTo(theta_, playerTheta);
			traveled_ = 0.0f;
			age_ = 0.0f;
			fuse_ = kBombFuse;
			// トゲは少しの間その場で止まってから滑り出す（爆弾はすぐ転がる）
			landPause_ = IsBomb() ? 0.0f : kAirSpikeLandPause;
			return Event::kLanded;
		}
		position_ = Lerp(flightStart_, landing, flightRate_);
		return Event::kNone;
	}
	case State::kSurface:
	case State::kReflected: {
		// 輪に沿って進む（跳ね返された爆弾は速い。着地直後に止まっている間は進まない）
		float speed = IsBomb() ? kBombRollSpeed : kSpikeSpeed;
		if (state_ == State::kReflected) {
			speed *= kReflectSpeedScale;
		}
		if (landPause_ > 0.0f) {
			landPause_ = std::max(landPause_ - dt, 0.0f);
			speed = 0.0f;
		}
		theta_ = Field::WrapAngle(theta_ + static_cast<float>(dir_) * speed * dt / Field::kRingRadius);
		traveled_ += speed * dt;
		position_ = Field::PolarToWorld(theta_, height_, layerZ_);
		if (IsBomb()) {
			fuse_ -= dt;
			return (fuse_ <= 0.0f) ? Event::kFuseOut : Event::kNone;
		}
		// トゲは半周進むか寿命で消える
		if (traveled_ >= kPi * Field::kRingRadius || age_ >= kSpikeLifetime) {
			return Event::kExpired;
		}
		return Event::kNone;
	}
	case State::kCaptured:
		// 位置は GameScene が SetRiderPose で決める。爆弾の導火線は乗っている間も進む
		if (IsBomb()) {
			fuse_ -= dt;
			return (fuse_ <= 0.0f) ? Event::kFuseOut : Event::kNone;
		}
		return Event::kNone;
	case State::kAbsorbing: {
		// 縮みながらスライムの中心へ吸い込まれる
		flightRate_ = std::min(flightRate_ + dt / kAbsorbTime, 1.0f);
		position_ = Lerp(flightStart_, absorbTarget, EaseOutCubic(flightRate_));
		absorbScale_ = Lerp(1.0f, kMinAbsorbScale, flightRate_);
		return (flightRate_ >= 1.0f) ? Event::kAbsorbed : Event::kNone;
	}
	case State::kToBoss: {
		// ボスの体へ直線で飛ぶ（ボスは揺れているので目標は毎フレーム更新）
		flightRate_ = std::min(flightRate_ + dt / kReflectToBossTime, 1.0f);
		position_ = Lerp(flightStart_, bossTarget, flightRate_);
		return (flightRate_ >= 1.0f) ? Event::kArrived : Event::kNone;
	}
	case State::kInactive:
	default:
		return Event::kNone;
	}
}

void Projectile::Capture(float offset) {
	state_ = State::kCaptured;
	captureOffset_ = offset;
}

void Projectile::SetRiderPose(float theta, float height) {
	theta_ = Field::WrapAngle(theta);
	height_ = height;
	position_ = Field::PolarToWorld(theta_, height_, layerZ_);
}

void Projectile::StartAbsorb() {
	state_ = State::kAbsorbing;
	flightStart_ = Field::PolarToWorld(theta_, height_, layerZ_);
	flightRate_ = 0.0f;
}

void Projectile::Reflect() {
	state_ = State::kReflected;
	dir_ = -dir_;
	height_ = kBombRadius;
	// 跳ね返した直後に足元で爆発しないよう、導火線を半分までは戻す
	fuse_ = std::max(fuse_, kBombFuse * 0.5f);
}

void Projectile::LaunchToBoss() {
	state_ = State::kToBoss;
	flightStart_ = position_;
	flightRate_ = 0.0f;
}

void Projectile::UpdateVisual() {
	if (!IsInUse()) {
		return;
	}
	const float radius = GetRadius() * absorbScale_;
	// 「上」（輪の内面から軸へ向かう向き）
	const Vector3 up = {-std::cos(theta_), -std::sin(theta_), 0.0f};
	const float surfaceRotation = Field::SurfaceRotationZ(theta_);

	WorldTransform& body = transforms_[kPartBody];
	body.translation_ = position_;
	if (IsBomb()) {
		body.scale_ = {radius, radius, radius};
		body.rotation_ = {0.0f, 0.0f, surfaceRotation};
	} else {
		// トゲ：細長い四角すい。地上では進む向きへ先端を向けてきりもみ、空中では回転、乗っている間は上向き
		body.scale_ = {radius * 1.1f, radius * 2.6f, radius * 1.1f};
		switch (state_) {
		case State::kAir:
			body.rotation_ = {spin_, 0.0f, spin_ * 0.7f};
			break;
		case State::kCaptured:
		case State::kAbsorbing:
			body.rotation_ = {0.0f, spin_, surfaceRotation};
			break;
		default:
			// 四角すいの先端（ローカル+Y）を進む向きへ：+1 なら −90°、−1 なら +90° 余分に回す
			body.rotation_ = {0.0f, spin_, surfaceRotation + ((dir_ > 0) ? -kPi * 0.5f : kPi * 0.5f)};
			break;
		}
	}
	UpdateWorldTransform(body);

	// 爆弾の導火線と火花（上向きに付ける）
	WorldTransform& fuse = transforms_[kPartFuse];
	fuse.translation_ = position_ + up * (radius * 1.15f);
	fuse.rotation_ = {0.0f, 0.0f, surfaceRotation + 0.35f};
	fuse.scale_ = {radius * 0.22f, radius * 0.75f, radius * 0.22f};
	UpdateWorldTransform(fuse);
	WorldTransform& spark = transforms_[kPartSpark];
	const float flicker = 0.75f + 0.25f * std::sin(age_ * 40.0f);
	spark.translation_ = position_ + up * (radius * 1.55f) + Field::Tangent(theta_) * (radius * 0.25f);
	spark.scale_ = Vector3{1.0f, 1.0f, 1.0f} * (radius * kBombSparkScale * flicker);
	UpdateWorldTransform(spark);
	// 爆弾の縁取り（体より少し大きい球。裏側だけ描くので体のまわりに輪郭として見える）
	WorldTransform& outline = transforms_[kPartOutline];
	outline.translation_ = position_;
	outline.scale_ = Vector3{1.0f, 1.0f, 1.0f} * (radius * kBombOutlineScale);
	UpdateWorldTransform(outline);

	// 色
	if (IsBomb()) {
		Vector3 bodyColor = IsFriendly() ? kFriendlyBombColor : kBombColor;
		// 導火線の残りが少ないと赤く点滅する（残りが減るほど速く）
		if (!IsFriendly() && state_ != State::kAir && fuse_ < kBombBlinkTime && kBombBlinkTime > 0.0f) {
			const float urgency = 1.0f - std::clamp(fuse_ / kBombBlinkTime, 0.0f, 1.0f);
			const float blink = std::fmod(age_ * (4.0f + urgency * 10.0f), 1.0f);
			if (blink < 0.5f) {
				bodyColor = kBombBlinkColor;
			}
		}
		colors_[kPartBody].SetColor({bodyColor.x, bodyColor.y, bodyColor.z, 1.0f});
		colors_[kPartFuse].SetColor({0.45f, 0.3f, 0.15f, 1.0f});
		const Vector4 sparkColor = IsFriendly() ? Vector4{0.3f, 1.0f, 0.4f, 1.0f} : Vector4{1.0f, 0.75f, 0.2f, 1.0f};
		colors_[kPartSpark].SetColor(sparkColor);
		const Vector4 outlineColor = IsFriendly() ? Vector4{0.3f, 1.0f, 0.4f, 1.0f} : Vector4{kBombOutlineColor.x, kBombOutlineColor.y, kBombOutlineColor.z, 1.0f};
		colors_[kPartOutline].SetColor(outlineColor);
	} else {
		colors_[kPartBody].SetColor({kSpikeColor.x, kSpikeColor.y, kSpikeColor.z, 1.0f});
	}
}

void Projectile::Draw(const Camera& camera) const {
	if (!IsInUse()) {
		return;
	}
	if (IsBomb()) {
		models_->sphere->Draw(transforms_[kPartBody], camera, &colors_[kPartBody]);
		models_->box->Draw(transforms_[kPartFuse], camera, &colors_[kPartFuse]);
	} else {
		models_->spike->Draw(transforms_[kPartBody], camera, &colors_[kPartBody]);
	}
}

void Projectile::DrawOutline(const Camera& camera) const {
	if (!IsInUse() || !IsBomb() || kBombOutlineScale <= 1.0f) {
		return;
	}
	models_->glowSphere->Draw(transforms_[kPartOutline], camera, &colors_[kPartOutline]);
}

void Projectile::DrawAdditive(const Camera& camera) const {
	if (!IsInUse() || !IsBomb()) {
		return;
	}
	models_->glowSphere->Draw(transforms_[kPartSpark], camera, &colors_[kPartSpark]);
}

void Projectile::DrawMarkerLines() const {
	if (state_ != State::kAir) {
		return;
	}
	// 着弾点に輪の内面に沿った円（接線方向 × Z方向の面）を描く。爆弾=赤、トゲ=黄。内側の円は着弾までに縮む
	const Vector4 color = IsBomb() ? Vector4{1.0f, 0.2f, 0.15f, 1.0f} : Vector4{1.0f, 0.9f, 0.2f, 1.0f};
	const Vector3 center = Field::PolarToWorld(theta_, 0.05f, layerZ_);
	const Vector3 tangent = Field::Tangent(theta_);
	const Vector3 depth = {0.0f, 0.0f, 1.0f};
	auto drawCircle = [&](float radius, int segments) {
		for (int i = 0; i < segments; i++) {
			const float a0 = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(segments);
			const float a1 = 2.0f * kPi * static_cast<float>(i + 1) / static_cast<float>(segments);
			const Vector3 p0 = center + tangent * (std::cos(a0) * radius) + depth * (std::sin(a0) * radius);
			const Vector3 p1 = center + tangent * (std::cos(a1) * radius) + depth * (std::sin(a1) * radius);
			LineDrawer::Draw(p0, p1, color);
		}
	};
	drawCircle(kMarkerRadius, kMarkerOuterSegments);
	drawCircle(std::max(kMarkerRadius - kMarkerDoubleGap, 0.01f), kMarkerOuterSegments);
	const float innerRadius = kMarkerRadius * (1.0f - std::clamp(flightRate_, 0.0f, 1.0f));
	if (innerRadius > 0.05f) {
		drawCircle(innerRadius, kMarkerInnerSegments);
	}
}

void Projectile::RegisterGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	globalVariables->CreateGroup(kGroupName);
	globalVariables->AddItem(kGroupName, "SpikeSpeed", kSpikeSpeed);
	globalVariables->AddItem(kGroupName, "SpikeHeight", kSpikeHeight);
	globalVariables->AddItem(kGroupName, "SpikeRadius", kSpikeRadius);
	globalVariables->AddItem(kGroupName, "SpikeLifetime", kSpikeLifetime);
	globalVariables->AddItem(kGroupName, "SpikeColor", kSpikeColor);
	globalVariables->AddItem(kGroupName, "BombRadius", kBombRadius);
	globalVariables->AddItem(kGroupName, "BombRollSpeed", kBombRollSpeed);
	globalVariables->AddItem(kGroupName, "BombFuse", kBombFuse);
	globalVariables->AddItem(kGroupName, "BombBlinkTime", kBombBlinkTime);
	globalVariables->AddItem(kGroupName, "BlastRadius", kBlastRadius);
	globalVariables->AddItem(kGroupName, "BombColor", kBombColor);
	globalVariables->AddItem(kGroupName, "BombBlinkColor", kBombBlinkColor);
	globalVariables->AddItem(kGroupName, "FriendlyBombColor", kFriendlyBombColor);
	globalVariables->AddItem(kGroupName, "BombOutlineScale", kBombOutlineScale);
	globalVariables->AddItem(kGroupName, "BombOutlineColor", kBombOutlineColor);
	globalVariables->AddItem(kGroupName, "BombSparkScale", kBombSparkScale);
	globalVariables->AddItem(kGroupName, "ReflectSpeedScale", kReflectSpeedScale);
	globalVariables->AddItem(kGroupName, "ReflectToBossTime", kReflectToBossTime);
	globalVariables->AddItem(kGroupName, "AirTravelTime", kAirTravelTime);
	globalVariables->AddItem(kGroupName, "MarkerRadius", kMarkerRadius);
	globalVariables->AddItem(kGroupName, "AirSpikeLandPause", kAirSpikeLandPause);
	globalVariables->AddItem(kGroupName, "AbsorbTime", kAbsorbTime);
}

void Projectile::ApplyGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	kSpikeSpeed = std::max(globalVariables->GetFloatValue(kGroupName, "SpikeSpeed"), 0.0f);
	kSpikeHeight = std::max(globalVariables->GetFloatValue(kGroupName, "SpikeHeight"), 0.0f);
	// 半径は大きさ（拡縮）にも使うので0にしない
	kSpikeRadius = std::max(globalVariables->GetFloatValue(kGroupName, "SpikeRadius"), 0.05f);
	kSpikeLifetime = std::max(globalVariables->GetFloatValue(kGroupName, "SpikeLifetime"), 0.1f);
	kSpikeColor = globalVariables->GetVector3Value(kGroupName, "SpikeColor");
	kBombRadius = std::max(globalVariables->GetFloatValue(kGroupName, "BombRadius"), 0.05f);
	kBombRollSpeed = std::max(globalVariables->GetFloatValue(kGroupName, "BombRollSpeed"), 0.0f);
	kBombFuse = std::max(globalVariables->GetFloatValue(kGroupName, "BombFuse"), 0.1f);
	kBombBlinkTime = std::max(globalVariables->GetFloatValue(kGroupName, "BombBlinkTime"), 0.0f);
	kBlastRadius = std::max(globalVariables->GetFloatValue(kGroupName, "BlastRadius"), 0.0f);
	kBombColor = globalVariables->GetVector3Value(kGroupName, "BombColor");
	kBombBlinkColor = globalVariables->GetVector3Value(kGroupName, "BombBlinkColor");
	kFriendlyBombColor = globalVariables->GetVector3Value(kGroupName, "FriendlyBombColor");
	// 縁取りは体より大きくないと見えない（1以下は描かない）。火花は0にしない（拡縮0で逆行列が壊れる）
	kBombOutlineScale = std::clamp(globalVariables->GetFloatValue(kGroupName, "BombOutlineScale"), 1.0f, 2.0f);
	kBombOutlineColor = globalVariables->GetVector3Value(kGroupName, "BombOutlineColor");
	kBombSparkScale = std::max(globalVariables->GetFloatValue(kGroupName, "BombSparkScale"), 0.05f);
	kReflectSpeedScale = std::max(globalVariables->GetFloatValue(kGroupName, "ReflectSpeedScale"), 0.0f);
	// 時間は割り算に使うので下限を付ける（空中弾の飛ぶ時間は上限も付ける＝空中の弾と予告マーカーが増えすぎないように）
	kReflectToBossTime = std::max(globalVariables->GetFloatValue(kGroupName, "ReflectToBossTime"), 0.05f);
	kAirTravelTime = std::clamp(globalVariables->GetFloatValue(kGroupName, "AirTravelTime"), 0.05f, kMaxAirTravelTime);
	kMarkerRadius = std::max(globalVariables->GetFloatValue(kGroupName, "MarkerRadius"), 0.05f);
	kAirSpikeLandPause = std::max(globalVariables->GetFloatValue(kGroupName, "AirSpikeLandPause"), 0.0f);
	kAbsorbTime = std::max(globalVariables->GetFloatValue(kGroupName, "AbsorbTime"), 0.02f);
}
