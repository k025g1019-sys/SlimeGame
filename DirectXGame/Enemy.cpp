#define NOMINMAX
#include "Enemy.h"
#include "Easing.h"
#include "GameConfig.h"
#include "GameModels.h"
#include "GlobalVariables.h"
#include "MathUtil.h"
#include <algorithm>
#include <cmath>
#include <numbers>

using namespace KamataEngine;

namespace {
// 調整項目のグループ名
const char* kGroupName = "Enemy";
// 体の高さ（半径に対する倍率。判定の高さもこれ）
const float kHeightScale = 1.6f;
// 拡縮の下限（0にすると逆行列が壊れる）
const float kMinScale = 0.05f;
// 押し出すときの余白
const float kPushMargin = 0.05f;
} // namespace

void Enemy::Initialize(const GameModels* models) {
	models_ = models;
	root_.Initialize();
	for (WorldTransform& part : parts_) {
		part.Initialize();
		// 部品はすべて root（輪の上の位置・向き・出現の大きさ）の子
		part.parent_ = &root_;
	}
	for (ObjectColor& color : colors_) {
		color.Initialize();
	}
}

void Enemy::Spawn(EnemyType type, float theta, int dir, int serial, float layerZ) {
	type_ = type;
	theta_ = Field::WrapAngle(theta);
	baseHeight_ = 0.0f;
	layerZ_ = layerZ;
	dir_ = (dir >= 0) ? 1 : -1;
	facing_ = dir_;
	state_ = State::kSpawning;
	stateTimer_ = 0.0f;
	knockbackTimer_ = 0.0f;
	walkPhase_ = 0.0f;
	captureOffset_ = 0.0f;
	absorbRate_ = 0.0f;
	// 最初の攻撃までの待ち：通し番号から黄金比で 0～1 を作り、Min～Max の割合を決める（乱数を使わず決定的にばらける）
	const float fraction = std::fmod(static_cast<float>(serial) * 0.6180339f, 1.0f);
	const float interval = (type_ == EnemyType::kShooter) ? kShooterInterval : kBomberInterval;
	attackTimer_ = interval * Lerp(kFirstAttackMin, kFirstAttackMax, fraction);
}

void Enemy::Update(float playerTheta, bool canAttack, std::vector<ProjectileSpawn>* requests) {
	const float dt = GameConfig::kDeltaTime;
	stateTimer_ += dt;

	switch (state_) {
	case State::kSpawning:
		// 出現演出が終わったら動き出す（それまでは無害）
		if (stateTimer_ >= kSpawnTime) {
			state_ = State::kActive;
			stateTimer_ = 0.0f;
		}
		break;
	case State::kActive:
		if (type_ == EnemyType::kWalker) {
			// 輪に沿って歩く（弾き飛ばされている間は速く滑る）
			float speed = kWalkerSpeed;
			if (knockbackTimer_ > 0.0f) {
				knockbackTimer_ = std::max(knockbackTimer_ - dt, 0.0f);
				speed = kKnockbackDistance / kKnockbackTime;
			}
			theta_ = Field::WrapAngle(theta_ + static_cast<float>(dir_) * speed * dt / Field::kRingRadius);
			walkPhase_ += kWalkerSpeed * dt;
			facing_ = dir_;
		} else {
			// その場でスライムの方を向き、間隔ごとに撃つ
			facing_ = Field::DirectionTo(theta_, playerTheta);
			walkPhase_ += dt * 1.5f;
			if (canAttack) {
				attackTimer_ -= dt;
				if (attackTimer_ <= 0.0f) {
					Attack(playerTheta, requests);
					attackTimer_ = (type_ == EnemyType::kShooter) ? kShooterInterval : kBomberInterval;
				}
			}
		}
		break;
	case State::kCaptured:
		// 乗せられている間は脚をばたつかせる（攻撃しない）
		walkPhase_ += dt * 8.0f;
		break;
	case State::kAbsorbing:
	case State::kInactive:
	default:
		break;
	}
}

void Enemy::Attack(float playerTheta, std::vector<ProjectileSpawn>* requests) {
	// スライムに近い向き（短い弧の側）へ、体の少し前から出す
	const int dir = Field::DirectionTo(theta_, playerTheta);
	ProjectileSpawn spawn;
	spawn.kind = (type_ == EnemyType::kShooter) ? ProjectileKind::kSpike : ProjectileKind::kBomb;
	spawn.fromAir = false;
	const float projectileRadius = (spawn.kind == ProjectileKind::kSpike) ? Projectile::kSpikeRadius : Projectile::kBombRadius;
	spawn.theta = theta_ + static_cast<float>(dir) * (kRadius + projectileRadius) / Field::kRingRadius;
	spawn.dir = dir;
	requests->push_back(spawn);
}

bool Enemy::UpdateAbsorb(const Vector3& target) {
	absorbTarget_ = target;
	absorbRate_ = std::min(absorbRate_ + GameConfig::kDeltaTime / Projectile::kAbsorbTime, 1.0f);
	return absorbRate_ >= 1.0f;
}

Footprint Enemy::GetFootprint() const { return {theta_, kRadius, baseHeight_, baseHeight_ + kRadius * kHeightScale}; }

Vector3 Enemy::GetCenterWorld() const {
	if (state_ == State::kAbsorbing) {
		return Lerp(absorbStart_, absorbTarget_, absorbRate_);
	}
	return Field::PolarToWorld(theta_, baseHeight_ + kRadius * kHeightScale * 0.5f, layerZ_);
}

void Enemy::BounceOffPlayer(float playerTheta, float playerHalfWidth) {
	if (type_ != EnemyType::kWalker) {
		return; // 動かない敵はそのまま（スライムが通り抜ける）
	}
	// スライムから離れる向きへ反転し、重ならない位置まで押し出してから弾き飛ばす（すぐまた当たり続けないように）
	const int away = Field::DirectionTo(playerTheta, theta_);
	dir_ = away;
	theta_ = Field::WrapAngle(playerTheta + static_cast<float>(away) * (playerHalfWidth + kRadius + kPushMargin) / Field::kRingRadius);
	knockbackTimer_ = kKnockbackTime;
}

void Enemy::Capture(float offset) {
	state_ = State::kCaptured;
	captureOffset_ = offset;
}

void Enemy::SetRiderPose(float theta, float baseHeight) {
	theta_ = Field::WrapAngle(theta);
	baseHeight_ = baseHeight;
}

void Enemy::StartAbsorb() {
	state_ = State::kAbsorbing;
	absorbStart_ = Field::PolarToWorld(theta_, baseHeight_, layerZ_);
	absorbTarget_ = absorbStart_;
	absorbRate_ = 0.0f;
}

void Enemy::UpdateVisual() {
	if (!IsInUse()) {
		return;
	}
	const float r = kRadius;

	// --- 全体（root）：輪の上の位置・向き・大きさ ---
	float scale = 1.0f;
	if (state_ == State::kSpawning) {
		// 出現：ぽんっと膨らむ
		scale = EaseOutBack(std::clamp(stateTimer_ / kSpawnTime, 0.0f, 1.0f));
	} else if (state_ == State::kAbsorbing) {
		scale = Lerp(1.0f, 0.12f, absorbRate_);
	} else if (state_ == State::kActive && type_ != EnemyType::kWalker && attackTimer_ < kAttackWindup && kAttackWindup > 0.0f) {
		// 撃つ直前に膨らむ（予備動作）
		scale = 1.0f + 0.18f * (1.0f - attackTimer_ / kAttackWindup);
	}
	scale = std::max(scale, kMinScale);
	if (state_ == State::kAbsorbing) {
		root_.translation_ = Lerp(absorbStart_, absorbTarget_, EaseOutCubic(absorbRate_));
	} else {
		// 弾き飛ばされている間は小さく跳ねる（見た目だけ）
		const float hop = (knockbackTimer_ > 0.0f) ? std::sin(std::numbers::pi_v<float> * knockbackTimer_ / kKnockbackTime) * r * 0.8f : 0.0f;
		root_.translation_ = Field::PolarToWorld(theta_, baseHeight_ + hop, layerZ_);
	}
	root_.rotation_ = {0.0f, 0.0f, Field::SurfaceRotationZ(theta_)};
	root_.scale_ = {scale, scale, scale};
	UpdateWorldTransform(root_);

	// --- 部品（root から見たローカル座標。+X=θの増える向き、+Y=上、−Z=カメラ側） ---
	const float facing = static_cast<float>(facing_);
	const float bob = std::sin(walkPhase_ * 4.0f) * 0.04f * r;
	WorldTransform& body = parts_[kPartBody];
	body.translation_ = {0.0f, r * 0.8f + bob, 0.0f};
	body.scale_ = {r * 1.15f, r * 0.8f, r * 0.95f};
	// 目（カメラ側の面から少し出す。進む向き（facing）へ寄せる）
	for (int i = 0; i < 2; i++) {
		const float side = (i == 0) ? -1.0f : 1.0f;
		const float eyeX = facing * r * 0.35f + side * r * 0.25f;
		WorldTransform& white = parts_[kPartEyeWhiteL + i];
		white.translation_ = {eyeX, r * 1.05f + bob, -r * 0.8f};
		white.scale_ = Vector3{1.0f, 1.0f, 1.0f} * (r * 0.22f);
		WorldTransform& pupil = parts_[kPartPupilL + i];
		pupil.translation_ = {eyeX + facing * r * 0.06f, r * 1.05f + bob, -r * 0.97f};
		pupil.scale_ = Vector3{1.0f, 1.0f, 1.0f} * (r * 0.11f);
	}
	// 脚（4本。歩くと交互に振れる）
	for (int i = 0; i < 4; i++) {
		const float x = (i % 2 == 0) ? r * 0.6f : -r * 0.6f;
		const float z = (i < 2) ? -r * 0.45f : r * 0.45f;
		WorldTransform& leg = parts_[kPartLegFL + i];
		leg.translation_ = {x, r * 0.18f, z};
		leg.scale_ = {r * 0.16f, r * 0.38f, r * 0.16f};
		leg.rotation_ = {0.0f, 0.0f, std::sin(walkPhase_ * 3.0f + static_cast<float>(i) * 1.6f) * 0.5f};
	}
	// 飾り（shooter：頭のトゲ ／ bomber：背中の爆弾）
	WorldTransform& accessory = parts_[kPartAccessory];
	if (type_ == EnemyType::kShooter) {
		accessory.translation_ = {0.0f, r * 1.75f + bob, 0.0f};
		accessory.scale_ = {r * 0.38f, r * 0.75f, r * 0.38f};
	} else {
		// 背中の爆弾は体の上に大きめに乗せる（体に隠れないように。色は爆弾と同じ）
		accessory.translation_ = {-facing * r * 0.35f, r * 1.6f + bob, 0.0f};
		accessory.scale_ = Vector3{1.0f, 1.0f, 1.0f} * (r * 0.5f);
	}
	for (WorldTransform& part : parts_) {
		UpdateWorldTransform(part);
	}

	// --- 色 ---
	Vector3 bodyColor = kWalkerColor;
	if (type_ == EnemyType::kShooter) {
		bodyColor = kShooterColor;
	} else if (type_ == EnemyType::kBomber) {
		bodyColor = kBomberColor;
	}
	if (state_ == State::kSpawning) {
		// 出現中は白から元の色へ
		bodyColor = Lerp(Vector3{1.0f, 1.0f, 1.0f}, bodyColor, std::clamp(stateTimer_ / kSpawnTime, 0.0f, 1.0f));
	} else if (state_ == State::kAbsorbing) {
		// 吸い込まれながら緑に染まる
		bodyColor = Lerp(bodyColor, Vector3{0.3f, 0.9f, 0.35f}, absorbRate_);
	}
	colors_[kColorBody].SetColor({bodyColor.x, bodyColor.y, bodyColor.z, 1.0f});
	colors_[kColorEyeWhite].SetColor({1.0f, 1.0f, 0.9f, 1.0f});
	colors_[kColorPupil].SetColor({0.02f, 0.02f, 0.02f, 1.0f});
	if (type_ == EnemyType::kShooter) {
		colors_[kColorAccessory].SetColor({1.0f, 0.5f, 0.1f, 1.0f});
	} else {
		colors_[kColorAccessory].SetColor({Projectile::kBombColor.x, Projectile::kBombColor.y, Projectile::kBombColor.z, 1.0f});
	}
}

void Enemy::Draw(const Camera& camera) const {
	if (!IsInUse()) {
		return;
	}
	models_->sphere->Draw(parts_[kPartBody], camera, &colors_[kColorBody]);
	models_->sphereLow->Draw(parts_[kPartEyeWhiteL], camera, &colors_[kColorEyeWhite]);
	models_->sphereLow->Draw(parts_[kPartEyeWhiteR], camera, &colors_[kColorEyeWhite]);
	models_->sphereLow->Draw(parts_[kPartPupilL], camera, &colors_[kColorPupil]);
	models_->sphereLow->Draw(parts_[kPartPupilR], camera, &colors_[kColorPupil]);
	for (int i = kPartLegFL; i <= kPartLegBR; i++) {
		models_->box->Draw(parts_[i], camera, &colors_[kColorBody]);
	}
	if (type_ == EnemyType::kShooter) {
		models_->spike->Draw(parts_[kPartAccessory], camera, &colors_[kColorAccessory]);
	} else if (type_ == EnemyType::kBomber) {
		models_->sphere->Draw(parts_[kPartAccessory], camera, &colors_[kColorAccessory]);
	}
}

void Enemy::RegisterGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	globalVariables->CreateGroup(kGroupName);
	globalVariables->AddItem(kGroupName, "Radius", kRadius);
	globalVariables->AddItem(kGroupName, "SpawnTime", kSpawnTime);
	globalVariables->AddItem(kGroupName, "SpawnSafeAngleDeg", kSpawnSafeAngleDeg);
	globalVariables->AddItem(kGroupName, "WalkerSpeed", kWalkerSpeed);
	globalVariables->AddItem(kGroupName, "KnockbackDistance", kKnockbackDistance);
	globalVariables->AddItem(kGroupName, "KnockbackTime", kKnockbackTime);
	globalVariables->AddItem(kGroupName, "ShooterInterval", kShooterInterval);
	globalVariables->AddItem(kGroupName, "BomberInterval", kBomberInterval);
	globalVariables->AddItem(kGroupName, "FirstAttackMin", kFirstAttackMin);
	globalVariables->AddItem(kGroupName, "FirstAttackMax", kFirstAttackMax);
	globalVariables->AddItem(kGroupName, "AttackWindup", kAttackWindup);
	globalVariables->AddItem(kGroupName, "WalkerColor", kWalkerColor);
	globalVariables->AddItem(kGroupName, "ShooterColor", kShooterColor);
	globalVariables->AddItem(kGroupName, "BomberColor", kBomberColor);
}

void Enemy::ApplyGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	// 半径は大きさ（拡縮）にも使うので0にしない
	kRadius = std::max(globalVariables->GetFloatValue(kGroupName, "Radius"), 0.05f);
	// 時間・間隔は割り算や「0秒ごと」に無限に撃つのを防ぐため下限を付ける
	kSpawnTime = std::max(globalVariables->GetFloatValue(kGroupName, "SpawnTime"), 0.01f);
	kSpawnSafeAngleDeg = std::clamp(globalVariables->GetFloatValue(kGroupName, "SpawnSafeAngleDeg"), 0.0f, 180.0f);
	kWalkerSpeed = std::max(globalVariables->GetFloatValue(kGroupName, "WalkerSpeed"), 0.0f);
	kKnockbackDistance = std::max(globalVariables->GetFloatValue(kGroupName, "KnockbackDistance"), 0.0f);
	kKnockbackTime = std::max(globalVariables->GetFloatValue(kGroupName, "KnockbackTime"), 0.01f);
	kShooterInterval = std::max(globalVariables->GetFloatValue(kGroupName, "ShooterInterval"), 0.2f);
	kBomberInterval = std::max(globalVariables->GetFloatValue(kGroupName, "BomberInterval"), 0.2f);
	kFirstAttackMin = std::clamp(globalVariables->GetFloatValue(kGroupName, "FirstAttackMin"), 0.0f, 1.0f);
	kFirstAttackMax = std::clamp(globalVariables->GetFloatValue(kGroupName, "FirstAttackMax"), 0.0f, 1.0f);
	kAttackWindup = std::max(globalVariables->GetFloatValue(kGroupName, "AttackWindup"), 0.0f);
	kWalkerColor = globalVariables->GetVector3Value(kGroupName, "WalkerColor");
	kShooterColor = globalVariables->GetVector3Value(kGroupName, "ShooterColor");
	kBomberColor = globalVariables->GetVector3Value(kGroupName, "BomberColor");
}
