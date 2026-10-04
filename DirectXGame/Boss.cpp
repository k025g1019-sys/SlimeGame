#define NOMINMAX
#include "Boss.h"
#include "Easing.h"
#include "Effects.h"
#include "Field.h"
#include "GameConfig.h"
#include "GameModels.h"
#include "GlobalVariables.h"
#include "MathUtil.h"
#include "Sounds.h"
#include <algorithm>
#include <cmath>

using namespace KamataEngine;

namespace {
// 調整項目のグループ名
const char* kGroupName = "Boss";
// 1回に投げる弾の数の上限
const int kMaxVolley = 8;
// 攻撃の間隔の下限（秒。レベルで短くなっても、調整で小さくしても、これより速くは投げない）
const float kMinAttackInterval = 0.3f;
// 投げた直後に腕を振り下ろす時間（秒）
const float kThrowRecoverTime = 0.35f;
// 拡縮の下限（0にすると逆行列が壊れる）
const float kMinScale = 0.01f;
// 体の中心（root からの高さ。大きさの倍率を掛ける前）
const float kBodyCenterY = 0.6f;

// 部品の初期配置（root から見たローカル座標・大きさ。腕は肩の回転軸から見た位置）
struct PartLayout {
	Vector3 translation;
	Vector3 scale;
};
} // namespace

void Boss::Initialize(const GameModels* models, Effects* effects) {
	models_ = models;
	effects_ = effects;
	root_.Initialize();
	for (WorldTransform& part : parts_) {
		part.Initialize();
		part.parent_ = &root_;
	}
	// 腕は肩（回転軸）の子にする（肩を回すと腕と手が一緒に回る）
	parts_[kUpperArmL].parent_ = &parts_[kShoulderL];
	parts_[kHandL].parent_ = &parts_[kShoulderL];
	parts_[kUpperArmR].parent_ = &parts_[kShoulderR];
	parts_[kHandR].parent_ = &parts_[kShoulderR];

	// 箱の組み方（−Z がカメラ側＝正面）
	const PartLayout layouts[kPartCount] = {
	    {{0.0f, -1.1f, 0.0f},    {2.2f, 0.8f, 1.4f} }, // kPelvis
	    {{0.0f, 0.6f, 0.0f},     {3.0f, 2.4f, 1.8f} }, // kTorso
	    {{0.0f, 2.6f, -0.1f},    {1.7f, 1.3f, 1.5f} }, // kHead
	    {{-0.4f, 2.7f, -0.86f},  {0.38f, 0.2f, 0.1f}}, // kEyeL
	    {{0.4f, 2.7f, -0.86f},   {0.38f, 0.2f, 0.1f}}, // kEyeR
	    {{-2.0f, 1.4f, 0.0f},    {1.0f, 1.0f, 1.0f} }, // kShoulderL（描かない）
	    {{2.0f, 1.4f, 0.0f},     {1.0f, 1.0f, 1.0f} }, // kShoulderR（描かない）
	    {{0.0f, -1.0f, 0.0f},    {0.9f, 1.9f, 0.9f} }, // kUpperArmL
	    {{0.0f, -2.4f, 0.0f},    {1.2f, 1.1f, 1.2f} }, // kHandL
	    {{0.0f, -1.0f, 0.0f},    {0.9f, 1.9f, 0.9f} }, // kUpperArmR
	    {{0.0f, -2.4f, 0.0f},    {1.2f, 1.1f, 1.2f} }, // kHandR
	    {{-0.8f, -2.2f, 0.0f},   {0.9f, 1.4f, 1.0f} }, // kLegL
	    {{0.8f, -2.2f, 0.0f},    {0.9f, 1.4f, 1.0f} }, // kLegR
	    {{-0.8f, -3.1f, -0.25f}, {1.2f, 0.5f, 1.5f} }, // kFootL
	    {{0.8f, -3.1f, -0.25f},  {1.2f, 0.5f, 1.5f} }, // kFootR
	};
	for (int i = 0; i < kPartCount; i++) {
		parts_[i].translation_ = layouts[i].translation;
		parts_[i].scale_ = layouts[i].scale;
	}

	bodyColor_.Initialize();
	eyeColor_.Initialize();
	Hide();
}

Vector3 Boss::HomePosition(float layerZ) { return {0.0f, kHeight, layerZ + kDepthOffset}; }

void Boss::Hide() {
	state_ = State::kAbsent;
	stateTimer_ = 0.0f;
	visualScale_ = 1.0f;
}

void Boss::PlaceIdle(float layerZ) {
	state_ = State::kIdleFar;
	stateTimer_ = 0.0f;
	position_ = HomePosition(layerZ);
	visualScale_ = 1.0f;
}

void Boss::StartPhase(float layerZ, int level, int hp) {
	level_ = std::max(level, 1);
	maxHp_ = std::max(hp, 1);
	hp_ = maxHp_;
	attackCount_ = 0;
	attackTimer_ = kFirstAttackDelay;
	throwRecover_ = 0.0f;
	flashTimer_ = 0.0f;
	visualScale_ = 1.0f;
	stateTimer_ = 0.0f;
	const Vector3 home = HomePosition(layerZ);
	const bool waitingHere = state_ == State::kIdleFar && Length(position_ - home) < 1.0f;
	const bool fleeingHere = state_ == State::kFlee && fleeHasNext_ && Length(moveTo_ - home) < 1.0f;
	if (waitingHere) {
		// この層で待っていた：そのまま攻撃の構えへ（攻撃はフェーズの kBattle になってから）
		state_ = State::kAttack;
		position_ = home;
	} else if (fleeingHere) {
		// この層へ逃げている途中（ボスフェーズが続いたとき・逃げる時間が長いとき）：今の位置からそのまま飛来を続ける
		state_ = State::kEnter;
		moveFrom_ = position_;
		moveTo_ = home;
	} else {
		// 奥から飛来する
		state_ = State::kEnter;
		moveFrom_ = home + Vector3{0.0f, 0.0f, kEnterDistance};
		moveTo_ = home;
		position_ = moveFrom_;
	}
}

float Boss::GetAttackInterval() const {
	// レベルが1上がるごとに倍率を掛けて短くする
	const float interval = kAttackInterval * std::pow(kAttackIntervalLevelScale, static_cast<float>(level_ - 1));
	return std::max(interval, kMinAttackInterval);
}

void Boss::Update(bool battleActive, float playerTheta, float playerAngularSpeed, std::vector<ProjectileSpawn>* requests) {
	const float dt = GameConfig::kDeltaTime;
	clock_ += dt;
	stateTimer_ += dt;
	flashTimer_ = std::max(flashTimer_ - dt, 0.0f);
	throwRecover_ = std::max(throwRecover_ - dt, 0.0f);

	switch (state_) {
	case State::kEnter: {
		const float t = std::clamp(stateTimer_ / kEnterTime, 0.0f, 1.0f);
		position_ = Lerp(moveFrom_, moveTo_, EaseOutCubic(t));
		if (t >= 1.0f) {
			state_ = State::kAttack;
			stateTimer_ = 0.0f;
		}
		break;
	}
	case State::kAttack:
		if (battleActive) {
			attackTimer_ -= dt;
			if (attackTimer_ <= 0.0f) {
				Throw(playerTheta, playerAngularSpeed, requests);
				attackTimer_ = GetAttackInterval();
			}
		}
		break;
	case State::kHit:
		// のけぞりが終わったら攻撃へ戻る（HP0 のときは GameScene が逃走・撃破を始めるまでこのまま）
		if (stateTimer_ >= kHitTime && hp_ > 0) {
			state_ = State::kAttack;
			stateTimer_ = 0.0f;
		}
		break;
	case State::kFlee: {
		const float t = std::clamp(stateTimer_ / kFleeTime, 0.0f, 1.0f);
		position_ = Lerp(moveFrom_, moveTo_, EaseInOut(t));
		if (!fleeHasNext_) {
			// 次が無いときは奥へ去りながら縮んで消える
			visualScale_ = std::max(1.0f - t, kMinScale);
		}
		if (t >= 1.0f) {
			state_ = fleeHasNext_ ? State::kIdleFar : State::kAbsent;
			stateTimer_ = 0.0f;
			visualScale_ = 1.0f;
		}
		break;
	}
	case State::kDefeated: {
		// 爆発の連鎖（位置は決定的にばらす）→ 最後に大爆発して消える
		defeatExplosionTimer_ -= dt;
		const Vector3 center = GetBodyCenter();
		if (defeatExplosionTimer_ <= 0.0f && stateTimer_ < kDefeatTime) {
			const float i = static_cast<float>(defeatExplosionCount_);
			const Vector3 offset = Vector3{std::sin(i * 2.3f) * 2.5f, std::cos(i * 1.7f) * 3.0f, -1.5f} * kScale;
			if (effects_) {
				effects_->SpawnExplosion(center + offset, 2.5f * kScale);
			}
			PlaySe(seExplosion);
			defeatExplosionCount_++;
			defeatExplosionTimer_ = kDefeatExplosionInterval;
		}
		if (stateTimer_ >= kDefeatTime) {
			if (effects_) {
				effects_->SpawnExplosion(center, 6.0f * kScale);
				effects_->SpawnSparks(center, {1.0f, 0.55f, 0.1f}, 24, 14.0f);
			}
			PlaySe(seExplosion);
			Hide();
		}
		break;
	}
	case State::kAbsent:
	case State::kIdleFar:
	default:
		break;
	}
}

void Boss::Throw(float playerTheta, float playerAngularSpeed, std::vector<ProjectileSpawn>* requests) {
	const int count = std::clamp(level_ * kVolleyPerLevel, 1, kMaxVolley);
	// 着弾点の中心＝スライムの予測位置（今の角度＋角速度×飛ぶ時間）
	const float center = playerTheta + playerAngularSpeed * Projectile::kAirTravelTime;
	const float spread = kVolleySpread / Field::kRingRadius;
	// 爆弾を含めるか（N回に1回は必ず）と、何発目を爆弾にするか（決定的に順番に回す）
	const int every = std::max(kBombEveryNthAttack, 1);
	const bool includesBomb = (attackCount_ % every) == 0;
	const int bombIndex = (attackCount_ / every) % count;
	// 左右の手で交互に投げる
	const bool rightHand = (attackCount_ % 2) == 0;
	const Vector3 start = GetHandPosition(rightHand);
	for (int i = 0; i < count; i++) {
		ProjectileSpawn spawn;
		spawn.kind = (includesBomb && i == bombIndex) ? ProjectileKind::kBomb : ProjectileKind::kSpike;
		spawn.fromAir = true;
		spawn.theta = center + (static_cast<float>(i) - static_cast<float>(count - 1) * 0.5f) * spread;
		spawn.dir = -1;
		spawn.airStart = start;
		requests->push_back(spawn);
	}
	attackCount_++;
	throwRecover_ = kThrowRecoverTime;
}

bool Boss::Hit() {
	// 飛来中・逃走中などに当てると攻撃の位置がずれる（飛来の補間が終わらない）ので、攻撃中・のけぞり中だけ受ける
	if (!IsTargetable()) {
		return false;
	}
	hp_--;
	flashTimer_ = kHitFlashTime;
	state_ = State::kHit;
	stateTimer_ = 0.0f;
	return hp_ <= 0;
}

void Boss::StartFlee(bool hasNext, float nextLayerZ) {
	state_ = State::kFlee;
	stateTimer_ = 0.0f;
	fleeHasNext_ = hasNext;
	moveFrom_ = position_;
	moveTo_ = hasNext ? HomePosition(nextLayerZ) : position_ + Vector3{0.0f, 0.0f, kEnterDistance};
}

void Boss::StartDefeat() {
	state_ = State::kDefeated;
	stateTimer_ = 0.0f;
	defeatExplosionTimer_ = 0.0f;
	defeatExplosionCount_ = 0;
}

Vector3 Boss::GetBodyCenter() const { return position_ + Vector3{0.0f, kBodyCenterY * kScale * visualScale_, 0.0f}; }

Vector3 Boss::GetHandPosition(bool rightHand) const {
	// 手の部品のワールド行列の平行移動成分（前フレームの UpdateVisual の結果）
	const Matrix4x4& matrix = parts_[rightHand ? kHandR : kHandL].matWorld_;
	return {matrix.m[3][0], matrix.m[3][1], matrix.m[3][2]};
}

void Boss::UpdateVisual() {
	if (!IsVisible()) {
		return;
	}
	// --- 全体：位置・ゆっくりした揺れ・のけぞり・撃破中の震え ---
	Vector3 jitter = {0.0f, std::sin(clock_ * 1.7f) * 0.3f, 0.0f};
	float lean = 0.0f;
	if (state_ == State::kHit) {
		lean = 0.35f * (1.0f - std::clamp(stateTimer_ / kHitTime, 0.0f, 1.0f));
	}
	if (state_ == State::kDefeated) {
		jitter += Vector3{std::sin(clock_ * 53.0f) * 0.35f, std::cos(clock_ * 47.0f) * 0.35f, 0.0f};
		lean = 0.25f;
	}
	root_.translation_ = position_ + jitter * kScale;
	root_.rotation_ = {lean, 0.0f, std::sin(clock_ * kSwaySpeed) * kSwayAmplitude};
	const float scale = std::max(kScale * visualScale_, kMinScale);
	root_.scale_ = {scale, scale, scale};
	UpdateWorldTransform(root_);

	// --- 腕：待機中は少し揺らす。投げる前に振りかぶり（予備動作）、投げた直後は前へ振り下ろす ---
	float windup = 0.0f;
	if (state_ == State::kAttack && attackTimer_ < kThrowWindup && kThrowWindup > 0.0f) {
		windup = 1.0f - attackTimer_ / kThrowWindup;
	}
	const float recover = throwRecover_ / kThrowRecoverTime;
	const bool nextRight = (attackCount_ % 2) == 0; // 次に投げる手
	const bool lastRight = !nextRight;              // 直前に投げた手
	for (int side = 0; side < 2; side++) {
		const bool isRight = (side == 1);
		const float sign = isRight ? 1.0f : -1.0f;
		float raise = 0.15f + 0.05f * std::sin(clock_ * 2.0f + static_cast<float>(side));
		float pitch = 0.0f;
		if (isRight == nextRight && windup > 0.0f) {
			raise = Lerp(raise, 2.4f, windup);
			pitch = -0.6f * windup;
		}
		if (isRight == lastRight && recover > 0.0f) {
			raise = Lerp(raise, 0.6f, recover);
			pitch = 1.0f * recover;
		}
		WorldTransform& shoulder = parts_[isRight ? kShoulderR : kShoulderL];
		shoulder.rotation_ = {pitch, 0.0f, sign * raise};
	}
	for (WorldTransform& part : parts_) {
		// 肩→腕の順（Part の並びで親が先に来る）
		UpdateWorldTransform(part);
	}

	// --- 色：被弾で白く光る。撃破中は点滅。投げる前は目が赤くなる ---
	Vector3 body = kBodyColor;
	if (flashTimer_ > 0.0f && kHitFlashTime > 0.0f) {
		body = Lerp(kBodyColor, Vector3{1.0f, 1.0f, 1.0f}, flashTimer_ / kHitFlashTime);
	}
	if (state_ == State::kDefeated && std::fmod(stateTimer_, 0.2f) < 0.1f) {
		body = {1.0f, 0.9f, 0.8f};
	}
	bodyColor_.SetColor({body.x, body.y, body.z, 1.0f});
	const Vector3 eye = Lerp(kEyeColor, Vector3{1.0f, 0.15f, 0.1f}, windup);
	eyeColor_.SetColor({eye.x, eye.y, eye.z, 1.0f});
	// ボス専用の箱のマテリアル（調整項目をすぐ反映する。値はフレーム内で変わらないので全部品で共有してよい）
	GameModels::SetMaterialLighting(models_->bossBox, kMaterialAmbient, kMaterialDiffuse);
}

void Boss::Draw(const Camera& camera) const {
	if (!IsVisible()) {
		return;
	}
	for (int i = 0; i < kPartCount; i++) {
		if (i == kShoulderL || i == kShoulderR) {
			continue; // 回転軸は描かない
		}
		const ObjectColor* color = (i == kEyeL || i == kEyeR) ? &eyeColor_ : &bodyColor_;
		models_->bossBox->Draw(parts_[i], camera, color);
	}
}

void Boss::RegisterGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	globalVariables->CreateGroup(kGroupName);
	globalVariables->AddItem(kGroupName, "Scale", kScale);
	globalVariables->AddItem(kGroupName, "DepthOffset", kDepthOffset);
	globalVariables->AddItem(kGroupName, "Height", kHeight);
	globalVariables->AddItem(kGroupName, "EnterTime", kEnterTime);
	globalVariables->AddItem(kGroupName, "EnterDistance", kEnterDistance);
	globalVariables->AddItem(kGroupName, "FleeTime", kFleeTime);
	globalVariables->AddItem(kGroupName, "AttackInterval", kAttackInterval);
	globalVariables->AddItem(kGroupName, "AttackIntervalLevelScale", kAttackIntervalLevelScale);
	globalVariables->AddItem(kGroupName, "VolleyPerLevel", kVolleyPerLevel);
	globalVariables->AddItem(kGroupName, "VolleySpread", kVolleySpread);
	globalVariables->AddItem(kGroupName, "BombEveryNthAttack", kBombEveryNthAttack);
	globalVariables->AddItem(kGroupName, "FirstAttackDelay", kFirstAttackDelay);
	globalVariables->AddItem(kGroupName, "ThrowWindup", kThrowWindup);
	globalVariables->AddItem(kGroupName, "HitTime", kHitTime);
	globalVariables->AddItem(kGroupName, "HitFlashTime", kHitFlashTime);
	globalVariables->AddItem(kGroupName, "DefeatTime", kDefeatTime);
	globalVariables->AddItem(kGroupName, "DefeatExplosionInterval", kDefeatExplosionInterval);
	globalVariables->AddItem(kGroupName, "SwayAmplitude", kSwayAmplitude);
	globalVariables->AddItem(kGroupName, "SwaySpeed", kSwaySpeed);
	globalVariables->AddItem(kGroupName, "BodyColor", kBodyColor);
	globalVariables->AddItem(kGroupName, "EyeColor", kEyeColor);
	globalVariables->AddItem(kGroupName, "MaterialAmbient", kMaterialAmbient);
	globalVariables->AddItem(kGroupName, "MaterialDiffuse", kMaterialDiffuse);
}

void Boss::ApplyGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	// 大きさは0にしない（逆行列が壊れる）。時間・間隔は割り算や連射を防ぐため下限を付ける
	kScale = std::max(globalVariables->GetFloatValue(kGroupName, "Scale"), 0.05f);
	kDepthOffset = globalVariables->GetFloatValue(kGroupName, "DepthOffset");
	kHeight = globalVariables->GetFloatValue(kGroupName, "Height");
	kEnterTime = std::max(globalVariables->GetFloatValue(kGroupName, "EnterTime"), 0.05f);
	kEnterDistance = std::max(globalVariables->GetFloatValue(kGroupName, "EnterDistance"), 0.0f);
	kFleeTime = std::max(globalVariables->GetFloatValue(kGroupName, "FleeTime"), 0.05f);
	kAttackInterval = std::max(globalVariables->GetFloatValue(kGroupName, "AttackInterval"), kMinAttackInterval);
	kAttackIntervalLevelScale = std::clamp(globalVariables->GetFloatValue(kGroupName, "AttackIntervalLevelScale"), 0.1f, 2.0f);
	kVolleyPerLevel = std::clamp(globalVariables->GetIntValue(kGroupName, "VolleyPerLevel"), 1, kMaxVolley);
	kVolleySpread = std::max(globalVariables->GetFloatValue(kGroupName, "VolleySpread"), 0.0f);
	kBombEveryNthAttack = std::max(globalVariables->GetIntValue(kGroupName, "BombEveryNthAttack"), 1);
	kFirstAttackDelay = std::max(globalVariables->GetFloatValue(kGroupName, "FirstAttackDelay"), 0.0f);
	kThrowWindup = std::max(globalVariables->GetFloatValue(kGroupName, "ThrowWindup"), 0.0f);
	kHitTime = std::max(globalVariables->GetFloatValue(kGroupName, "HitTime"), 0.01f);
	kHitFlashTime = std::max(globalVariables->GetFloatValue(kGroupName, "HitFlashTime"), 0.0f);
	kDefeatTime = std::max(globalVariables->GetFloatValue(kGroupName, "DefeatTime"), 0.1f);
	kDefeatExplosionInterval = std::max(globalVariables->GetFloatValue(kGroupName, "DefeatExplosionInterval"), 0.03f);
	kSwayAmplitude = globalVariables->GetFloatValue(kGroupName, "SwayAmplitude");
	kSwaySpeed = globalVariables->GetFloatValue(kGroupName, "SwaySpeed");
	kBodyColor = globalVariables->GetVector3Value(kGroupName, "BodyColor");
	kEyeColor = globalVariables->GetVector3Value(kGroupName, "EyeColor");
	kMaterialAmbient = std::clamp(globalVariables->GetFloatValue(kGroupName, "MaterialAmbient"), 0.0f, 2.0f);
	kMaterialDiffuse = std::clamp(globalVariables->GetFloatValue(kGroupName, "MaterialDiffuse"), 0.0f, 2.0f);
}
