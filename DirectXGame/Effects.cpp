#define NOMINMAX
#include "Effects.h"
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
const char* kGroupName = "Effect";
constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
// 拡縮の下限（0にすると逆行列が壊れる）
const float kMinScale = 0.01f;
// 取り込みの粒の数の上限
const int kMaxAbsorbParticles = 32;
} // namespace

void Effects::Initialize(const GameModels* models) {
	models_ = models;
	particles_.clear();
	sparkSerial_ = 0;
}

void Effects::Clear() {
	for (auto& particle : particles_) {
		particle->active = false;
	}
}

Effects::Particle* Effects::Acquire() {
	for (auto& particle : particles_) {
		if (!particle->active) {
			return particle.get();
		}
	}
	if (static_cast<int>(particles_.size()) >= kMaxParticles) {
		return nullptr;
	}
	auto particle = std::make_unique<Particle>();
	particle->worldTransform.Initialize();
	particle->objectColor.Initialize();
	particles_.push_back(std::move(particle));
	return particles_.back().get();
}

void Effects::SpawnExplosion(const Vector3& position, float radius) {
	Particle* particle = Acquire();
	if (!particle) {
		return;
	}
	particle->active = true;
	particle->kind = Kind::kExplosion;
	particle->position = position;
	particle->velocity = {0.0f, 0.0f, 0.0f};
	particle->color = kExplosionColor;
	particle->startSize = radius * 0.3f;
	particle->endSize = radius;
	particle->life = 0.0f;
	particle->maxLife = kExplosionTime;
	particle->spin = 0.0f;
}

void Effects::SpawnAbsorb(const Vector3& position, const Vector3& color) {
	// 画面と平行な面（XY）に輪っか状に広がる
	const int count = std::clamp(kAbsorbParticleCount, 1, kMaxAbsorbParticles);
	for (int i = 0; i < count; i++) {
		Particle* particle = Acquire();
		if (!particle) {
			return;
		}
		const float angle = kTwoPi * static_cast<float>(i) / static_cast<float>(count);
		particle->active = true;
		particle->kind = Kind::kOrb;
		particle->position = position;
		particle->velocity = Vector3{std::cos(angle), std::sin(angle), 0.0f} * kAbsorbSpeed;
		particle->color = color;
		particle->startSize = kAbsorbParticleSize;
		particle->endSize = kAbsorbParticleSize * 0.3f;
		particle->life = 0.0f;
		particle->maxLife = kAbsorbTime;
		particle->spin = 0.0f;
	}
}

void Effects::SpawnSparks(const Vector3& position, const Vector3& color, int count, float speed) {
	for (int i = 0; i < count; i++) {
		Particle* particle = Acquire();
		if (!particle) {
			return;
		}
		// 通し番号から黄金角でばらした向き（手前側＝−Z にも少し飛ばす）
		const float serial = static_cast<float>(sparkSerial_++);
		const float angle = serial * 2.3999632f;
		const float spread = 0.6f + 0.4f * std::fmod(serial * 0.618034f, 1.0f);
		particle->active = true;
		particle->kind = Kind::kSpark;
		particle->position = position;
		particle->velocity = Vector3{std::cos(angle) * spread, std::sin(angle) * spread, -0.35f} * speed;
		particle->color = color;
		particle->startSize = kSparkSize;
		particle->endSize = kSparkSize * 0.2f;
		particle->life = 0.0f;
		particle->maxLife = kSparkTime;
		particle->spin = angle;
	}
}

void Effects::Update() {
	const float dt = GameConfig::kDeltaTime;
	for (auto& particlePtr : particles_) {
		Particle& particle = *particlePtr;
		if (!particle.active) {
			continue;
		}
		particle.life += dt;
		if (particle.life >= particle.maxLife) {
			particle.active = false;
			continue;
		}
		if (particle.kind == Kind::kSpark) {
			// 火花は輪の外向き（軸から離れる向き）へ落ちる（筒の内側では「下」が外向き）
			const Vector3 outward = Normalize(Vector3{particle.position.x, particle.position.y, 0.0f});
			particle.velocity += outward * (kSparkGravity * dt);
			particle.spin += dt * 10.0f;
		} else if (particle.kind == Kind::kOrb) {
			// 取り込みの粒は減速する（1秒あたりの減衰の強さ。フレームの長さに依らないよう指数で）
			particle.velocity *= std::exp(-kAbsorbDamping * dt);
		}
		particle.position += particle.velocity * dt;
	}
}

void Effects::UpdateVisual() {
	for (auto& particlePtr : particles_) {
		Particle& particle = *particlePtr;
		if (!particle.active) {
			continue;
		}
		const float t = std::clamp(particle.life / particle.maxLife, 0.0f, 1.0f);
		float size = Lerp(particle.startSize, particle.endSize, (particle.kind == Kind::kExplosion) ? EaseOutCubic(t) : t);
		size = std::max(size, kMinScale);
		particle.worldTransform.translation_ = particle.position;
		particle.worldTransform.rotation_ = {particle.spin, particle.spin * 0.7f, 0.0f};
		particle.worldTransform.scale_ = {size, size, size};
		UpdateWorldTransform(particle.worldTransform);
		// 爆発・取り込みの粒は後半で薄くなる（加算は色を暗くすると消える）
		const float fade = 1.0f - t * t;
		if (particle.kind == Kind::kExplosion) {
			particle.objectColor.SetColor({particle.color.x * fade, particle.color.y * fade, particle.color.z * fade, 1.0f});
		} else if (particle.kind == Kind::kOrb) {
			particle.objectColor.SetColor({particle.color.x, particle.color.y, particle.color.z, 0.85f * fade});
		} else {
			particle.objectColor.SetColor({particle.color.x, particle.color.y, particle.color.z, 1.0f});
		}
	}
}

void Effects::DrawKind(const Camera& camera, Kind kind, Model* model) const {
	for (const auto& particle : particles_) {
		if (particle->active && particle->kind == kind) {
			model->Draw(particle->worldTransform, camera, &particle->objectColor);
		}
	}
}

void Effects::DrawOpaque(const Camera& camera) const { DrawKind(camera, Kind::kSpark, models_->box); }

void Effects::DrawTranslucent(const Camera& camera) const { DrawKind(camera, Kind::kOrb, models_->sphereLow); }

void Effects::DrawAdditive(const Camera& camera) const { DrawKind(camera, Kind::kExplosion, models_->glowSphere); }

int Effects::GetActiveCount() const {
	int count = 0;
	for (const auto& particle : particles_) {
		if (particle->active) {
			count++;
		}
	}
	return count;
}

void Effects::RegisterGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	globalVariables->CreateGroup(kGroupName);
	globalVariables->AddItem(kGroupName, "ExplosionTime", kExplosionTime);
	globalVariables->AddItem(kGroupName, "ExplosionColor", kExplosionColor);
	globalVariables->AddItem(kGroupName, "AbsorbParticleCount", kAbsorbParticleCount);
	globalVariables->AddItem(kGroupName, "AbsorbSpeed", kAbsorbSpeed);
	globalVariables->AddItem(kGroupName, "AbsorbTime", kAbsorbTime);
	globalVariables->AddItem(kGroupName, "AbsorbParticleSize", kAbsorbParticleSize);
	globalVariables->AddItem(kGroupName, "AbsorbDamping", kAbsorbDamping);
	globalVariables->AddItem(kGroupName, "SparkTime", kSparkTime);
	globalVariables->AddItem(kGroupName, "SparkSize", kSparkSize);
	globalVariables->AddItem(kGroupName, "SparkGravity", kSparkGravity);
}

void Effects::ApplyGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	// 時間は割り算に使うので下限を付ける。大きさは0にしない
	kExplosionTime = std::max(globalVariables->GetFloatValue(kGroupName, "ExplosionTime"), 0.02f);
	kExplosionColor = globalVariables->GetVector3Value(kGroupName, "ExplosionColor");
	kAbsorbParticleCount = std::clamp(globalVariables->GetIntValue(kGroupName, "AbsorbParticleCount"), 1, kMaxAbsorbParticles);
	kAbsorbSpeed = globalVariables->GetFloatValue(kGroupName, "AbsorbSpeed");
	kAbsorbTime = std::max(globalVariables->GetFloatValue(kGroupName, "AbsorbTime"), 0.02f);
	kAbsorbParticleSize = std::max(globalVariables->GetFloatValue(kGroupName, "AbsorbParticleSize"), 0.01f);
	kAbsorbDamping = std::max(globalVariables->GetFloatValue(kGroupName, "AbsorbDamping"), 0.0f);
	kSparkTime = std::max(globalVariables->GetFloatValue(kGroupName, "SparkTime"), 0.02f);
	kSparkSize = std::max(globalVariables->GetFloatValue(kGroupName, "SparkSize"), 0.01f);
	kSparkGravity = globalVariables->GetFloatValue(kGroupName, "SparkGravity");
}
