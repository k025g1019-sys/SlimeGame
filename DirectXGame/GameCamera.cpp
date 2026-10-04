#define NOMINMAX
#include "GameCamera.h"
#include "Easing.h"
#include "GameConfig.h"
#include "GlobalVariables.h"
#include "MathUtil.h"
#include <algorithm>
#include <cmath>
#include <numbers>

using namespace KamataEngine;

namespace {
// 調整項目のグループ名
const char* kGroupName = "Camera";
constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;
// カメラの上下の向きの限界（度。真上・真下を向くとエンジンのカメラの計算が壊れるため手前で止める）
const float kMaxPitchDeg = 89.0f;
} // namespace

void GameCamera::Initialize() {
	camera_.Initialize();
	UpdateMatrix();
}

void GameCamera::SnapTo(float layerZ) {
	focusZ_ = layerZ;
	isMoving_ = false;
	moveTimer_ = 0.0f;
	moveProgress_ = 1.0f;
	shakeTimer_ = 0.0f;
}

void GameCamera::StartMove(float fromLayerZ, float toLayerZ) {
	isMoving_ = true;
	moveFromZ_ = fromLayerZ;
	moveToZ_ = toLayerZ;
	moveTimer_ = 0.0f;
	moveProgress_ = 0.0f;
	focusZ_ = fromLayerZ;
}

void GameCamera::Update() {
	const float dt = GameConfig::kDeltaTime;
	// 層の移動（ゆっくり始まりゆっくり止まる）
	if (isMoving_) {
		moveTimer_ += dt;
		const float t = std::clamp(moveTimer_ / kAdvanceDuration, 0.0f, 1.0f);
		moveProgress_ = EaseInOut(t);
		focusZ_ = Lerp(moveFromZ_, moveToZ_, moveProgress_);
		if (t >= 1.0f) {
			isMoving_ = false;
			moveProgress_ = 1.0f;
			focusZ_ = moveToZ_;
		}
	}
	// 揺れの減衰
	shakeClock_ += dt;
	shakeTimer_ = std::max(shakeTimer_ - dt, 0.0f);
}

void GameCamera::Shake(float strength) {
	shakeTimer_ = kShakeTime;
	shakeStrength_ = strength;
}

void GameCamera::UpdateMatrix() {
	if (debugOrbit_.enabled) {
		// デバッグカメラ：注視点から「向きの逆方向へ距離ぶん」下がった位置に置き、注視点の方を向かせる
		const float pitch = std::clamp(debugOrbit_.pitchDeg, -kMaxPitchDeg, kMaxPitchDeg) * kDegToRad;
		const float yaw = debugOrbit_.yawDeg * kDegToRad;
		const Vector3 forward = {std::cos(pitch) * std::sin(yaw), -std::sin(pitch), std::cos(pitch) * std::cos(yaw)};
		const Vector3 target = {0.0f, 0.0f, focusZ_ + debugOrbit_.targetOffsetZ};
		camera_.translation_ = target - forward * debugOrbit_.distance;
		camera_.rotation_ = {pitch, yaw, 0.0f};
	} else {
		// 通常：軸上の層の手前に置いて奥（+Z）を向く。揺れは sin の組み合わせ（決定的。残り時間で減衰）
		Vector3 shake = {0.0f, 0.0f, 0.0f};
		if (shakeTimer_ > 0.0f && kShakeTime > 0.0f) {
			const float fade = shakeTimer_ / kShakeTime;
			const float amplitude = kShakeAmplitude * shakeStrength_ * fade * fade;
			const float phase = shakeClock_ * kShakeFrequency * 2.0f * std::numbers::pi_v<float>;
			shake = {std::sin(phase) * amplitude, std::sin(phase * 1.31f + 1.7f) * amplitude, 0.0f};
		}
		camera_.translation_ = Vector3{kOffset.x, kOffset.y, focusZ_ - kDistance} + shake;
		camera_.rotation_ = {0.0f, 0.0f, 0.0f};
	}
	camera_.fovAngleY = std::clamp(kFovDeg, 1.0f, 170.0f) * kDegToRad;
	camera_.farZ = kFarZ;
	// ビュー行列・射影行列を計算してGPUへ転送する（位置や向きを変えたら毎回呼ぶ）
	camera_.UpdateMatrix();
}

void GameCamera::RegisterGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	globalVariables->CreateGroup(kGroupName);
	globalVariables->AddItem(kGroupName, "Distance", kDistance);
	globalVariables->AddItem(kGroupName, "Offset", kOffset);
	globalVariables->AddItem(kGroupName, "FovDeg", kFovDeg);
	globalVariables->AddItem(kGroupName, "FarZ", kFarZ);
	globalVariables->AddItem(kGroupName, "AdvanceDuration", kAdvanceDuration);
	globalVariables->AddItem(kGroupName, "ShakeAmplitude", kShakeAmplitude);
	globalVariables->AddItem(kGroupName, "ShakeTime", kShakeTime);
	globalVariables->AddItem(kGroupName, "ShakeFrequency", kShakeFrequency);
	globalVariables->AddItem(kGroupName, "ShakeOnDamage", kShakeOnDamage);
	globalVariables->AddItem(kGroupName, "ShakeOnBossHit", kShakeOnBossHit);
}

void GameCamera::ApplyGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	kDistance = std::max(globalVariables->GetFloatValue(kGroupName, "Distance"), 1.0f);
	kOffset = globalVariables->GetVector3Value(kGroupName, "Offset");
	kFovDeg = globalVariables->GetFloatValue(kGroupName, "FovDeg");
	// farZ は nearZ(0.1) より十分大きくする（逆転すると射影行列が壊れる）
	kFarZ = std::max(globalVariables->GetFloatValue(kGroupName, "FarZ"), 10.0f);
	// 時間は0にすると割り算が壊れるので下限を付ける
	kAdvanceDuration = std::max(globalVariables->GetFloatValue(kGroupName, "AdvanceDuration"), 0.05f);
	kShakeAmplitude = std::max(globalVariables->GetFloatValue(kGroupName, "ShakeAmplitude"), 0.0f);
	kShakeTime = std::max(globalVariables->GetFloatValue(kGroupName, "ShakeTime"), 0.01f);
	kShakeFrequency = std::max(globalVariables->GetFloatValue(kGroupName, "ShakeFrequency"), 0.0f);
	kShakeOnDamage = std::max(globalVariables->GetFloatValue(kGroupName, "ShakeOnDamage"), 0.0f);
	kShakeOnBossHit = std::max(globalVariables->GetFloatValue(kGroupName, "ShakeOnBossHit"), 0.0f);
}
