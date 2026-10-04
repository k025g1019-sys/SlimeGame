#define NOMINMAX
#include "GameConfig.h"
#include "GameInput.h"
#include "GameScene.h"
#include "MathUtil.h"
#include "Sounds.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>

using namespace KamataEngine;

// GameScene の「物の更新と当たり判定」。このゲームの核（平らで乗せる → 離して取り込む／通常状態で爆弾を跳ね返す）はここ。
//
// 【接触の早見表】（左＝通常状態のスライムに当たったとき ／ 右＝平らなスライムに重なって乗り、離した瞬間に取り込んだとき）
// 　雑魚敵 : 被弾（walker は弾き飛ばされて向きを反転） ／ 乗る → 倒す
// 　トゲ弾 : 被弾（弾は消える）                        ／ 乗る → 食べる（回復 kSpikeEatHeal）
// 　爆弾   : 跳ね返す（無敵中でも）                    ／ 乗る → 体内で爆発してダメージ（乗せたまま導火線が尽きても、その場で爆発して同じダメージ）
// 　跳ね返した爆弾は味方の弾：ボスが攻撃中ならボスへ飛んで1ダメージ、それ以外は逆向きに転がって敵に当たると爆発（爆風の敵を倒す）

namespace {
constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;

// ログ用の名前
const char* GetEnemyTypeName(EnemyType type) {
	switch (type) {
	case EnemyType::kShooter:
		return "shooter";
	case EnemyType::kBomber:
		return "bomber";
	case EnemyType::kWalker:
	default:
		return "walker";
	}
}
} // namespace

// ===== 物の更新 =====

void GameScene::UpdateActors() {
	GameInput* input = GameInput::GetInstance();

	// スライム（A/D で速度調整、スペースを押している間は平ら）
	player_.Update(input->GetMove().x, input->IsPressed(GameInput::Action::kFlatten), true);
	if (player_.IsFlattenTriggered()) {
		PlaySe(seFlatten);
	}
	if (player_.IsRestoreTriggered()) {
		PlaySe(seRestore);
	}

	// ボス（攻撃はボスフェーズの kBattle 中だけ。逃げる・飛来は常に進む）
	boss_.Update(phaseState_ == PhaseState::kBattle && IsBossPhase(), player_.GetTheta(), player_.GetAngularSpeed(), &spawnRequests_);

	// 雑魚（攻撃は弾の生成要求を積むだけ）
	const bool canAttack = phaseState_ == PhaseState::kIntro || phaseState_ == PhaseState::kBattle;
	for (auto& enemy : enemies_) {
		if (enemy->IsInUse()) {
			enemy->Update(player_.GetTheta(), canAttack, &spawnRequests_);
		}
	}

	// 弾（爆発・着弾などの出来事はその場で処理する。弾のコンテナ自体は増減させない）
	const Vector3 absorbTarget = player_.GetCenterWorld();
	const Vector3 bossTarget = boss_.GetBodyCenter();
	for (auto& projectile : projectiles_) {
		if (!projectile->IsInUse()) {
			continue;
		}
		const Projectile::Event event = projectile->Update(player_.GetTheta(), absorbTarget, bossTarget);
		if (event != Projectile::Event::kNone) {
			HandleProjectileEvent(*projectile, event);
		}
	}
}

void GameScene::SpawnPendingEnemies() {
	const float playerTheta = player_.GetTheta();
	const float safeAngle = Enemy::kSpawnSafeAngleDeg * kDegToRad;
	for (PendingSpawn& pending : pendingSpawns_) {
		if (pending.spawned || phaseTimer_ < pending.def.delay) {
			continue;
		}
		Enemy* enemy = AcquireEnemy();
		if (!enemy) {
			break; // プールが一杯（次のフレームにまた試す）
		}
		pending.spawned = true;
		// スライムのすぐ近くに出る敵は、安全な角度の位置までずらす（出た瞬間に重なって理不尽に当たらないように。
		// スライムより前なら前へ、後ろなら後ろへ。ちょうど同じ位置なら前へ）
		float theta = pending.def.angleDeg * kDegToRad;
		const float difference = Field::WrapAngle(theta - playerTheta);
		if (std::abs(difference) < safeAngle) {
			theta = playerTheta + ((difference >= 0.0f) ? safeAngle : -safeAngle);
			LogEvent(std::format("spawn moved away from the slime ({:.0f} -> {:.0f} deg)", pending.def.angleDeg, Field::WrapAngle(theta) / kDegToRad));
		}
		enemy->Spawn(pending.def.type, theta, pending.def.dir, pending.serial, CurrentLayerZ());
	}
}

void GameScene::HandleProjectileEvent(Projectile& projectile, Projectile::Event event) {
	switch (event) {
	case Projectile::Event::kFuseOut:
		// 導火線が尽きた：その場で爆発（スライムに乗っていればスライムが被弾する）
		ExplodeBomb(projectile);
		break;
	case Projectile::Event::kExpired:
		// トゲが半周進んだ・寿命：小さな煙を出して消える
		effects_.SpawnSparks(projectile.GetWorldPosition(), {0.5f, 0.5f, 0.5f}, 4, 4.0f);
		projectile.Kill();
		break;
	case Projectile::Event::kArrived:
		// 跳ね返した爆弾がボスに着いた
		effects_.SpawnExplosion(projectile.GetWorldPosition(), Projectile::kBlastRadius * 1.3f);
		PlaySe(seExplosion);
		if (boss_.IsTargetable()) {
			boss_.Hit();
			PlaySe(seBossHit);
			effects_.SpawnSparks(projectile.GetWorldPosition(), {1.0f, 0.6f, 0.15f}, 12, 12.0f);
			gameCamera_.Shake(GameCamera::kShakeOnBossHit);
			LogEvent(std::format("BOSS HIT! boss hp {}/{}", boss_.GetHp(), boss_.GetMaxHp()));
		}
		projectile.Kill();
		break;
	case Projectile::Event::kAbsorbed:
		// 吸い込み終わった：トゲは食べる、爆弾は体内で爆発してダメージ
		if (projectile.IsBomb()) {
			effects_.SpawnExplosion(player_.GetCenterWorld(), Projectile::kBlastRadius * 0.7f);
			PlaySe(seExplosion);
			LogEvent("absorbed a BOMB");
			DamagePlayer(Player::kBombAbsorbDamage, "bomb exploded inside");
		} else {
			effects_.SpawnAbsorb(player_.GetCenterWorld(), {0.9f, 1.0f, 0.3f});
			PlaySe(seEat);
			player_.Heal(Player::kSpikeEatHeal);
			LogEvent("ate a spike");
		}
		projectile.Kill();
		break;
	case Projectile::Event::kLanded:
	case Projectile::Event::kNone:
	default:
		break;
	}
}

// ===== 当たり判定 =====

void GameScene::ResolveInteractions() {
	// 1. 乗っている物をスライムに追従させる（スライムが動いた後の位置へ）
	UpdateRiders();
	// 2. 平らなら、重なった敵・弾を乗せる
	if (player_.IsFlat()) {
		CaptureOnFlat();
	}
	// 3. 元に戻った瞬間：乗っている物＋足元に重なっている物をすべて取り込み始める
	if (player_.IsRestoreTriggered()) {
		AbsorbOnRestore();
	}
	// 4. 通常状態の接触（敵・トゲ＝被弾、爆弾＝跳ね返す）
	if (!player_.IsFlat()) {
		HandleNormalContacts();
	}
	// 5. 跳ね返した爆弾と敵
	HandleFriendlyBombs();
	// 6. 吸い込み中の敵（終わったら倒す。弾の吸い込みは Projectile::Update の kAbsorbed で処理済み）
	UpdateAbsorbingEnemies();
}

void GameScene::UpdateRiders() {
	// 乗っている物は スライムの角度＋乗った位置のずれ、高さはスライムの見た目の上面（バネで揺れる）
	const float playerTheta = player_.GetTheta();
	const float riderHeight = player_.GetRiderHeight();
	const float halfAngle = player_.GetRiderHalfAngle();
	for (auto& enemy : enemies_) {
		if (enemy->GetState() == Enemy::State::kCaptured) {
			const float offset = std::clamp(enemy->GetCaptureOffset(), -halfAngle, halfAngle);
			enemy->SetRiderPose(playerTheta + offset, riderHeight);
		}
	}
	for (auto& projectile : projectiles_) {
		if (projectile->GetState() == Projectile::State::kCaptured) {
			const float offset = std::clamp(projectile->GetCaptureOffset(), -halfAngle, halfAngle);
			projectile->SetRiderPose(playerTheta + offset, riderHeight + projectile->GetRadius());
		}
	}
}

void GameScene::CaptureOnFlat() {
	const Footprint flat = player_.GetFlatFootprint();
	const float playerTheta = player_.GetTheta();
	const float riderHeight = player_.GetRiderHeight();
	const float halfAngle = player_.GetRiderHalfAngle();
	// 乗った位置のずれ（スライムの上に収まる範囲で）
	auto offsetOf = [&](float theta) { return std::clamp(Field::WrapAngle(theta - playerTheta), -halfAngle, halfAngle); };

	// 敵（kEnemyStickToFlat が false なら乗らない＝離した瞬間に重なっていた分だけ取り込む）
	if (Player::kEnemyStickToFlat) {
		for (auto& enemy : enemies_) {
			if (enemy->IsActive() && Field::Overlaps(flat, enemy->GetFootprint())) {
				const float offset = offsetOf(enemy->GetTheta());
				enemy->Capture(offset);
				enemy->SetRiderPose(playerTheta + offset, riderHeight);
				LogEvent(std::format("captured a {}", GetEnemyTypeName(enemy->GetType())));
			}
		}
	}
	// 敵の弾（トゲは kSpikeStickToFlat、爆弾は常に乗る）
	for (auto& projectile : projectiles_) {
		if (projectile->GetState() != Projectile::State::kSurface) {
			continue;
		}
		if (!projectile->IsBomb() && !Player::kSpikeStickToFlat) {
			continue;
		}
		if (Field::Overlaps(flat, projectile->GetFootprint())) {
			const float offset = offsetOf(projectile->GetTheta());
			projectile->Capture(offset);
			projectile->SetRiderPose(playerTheta + offset, riderHeight + projectile->GetRadius());
			LogEvent(projectile->IsBomb() ? "captured a BOMB (release = damage!)" : "captured a spike");
		}
	}
}

void GameScene::AbsorbOnRestore() {
	const Footprint flat = player_.GetFlatFootprint();
	int count = 0;
	for (auto& enemy : enemies_) {
		const bool captured = enemy->GetState() == Enemy::State::kCaptured;
		if (captured || (enemy->IsActive() && Field::Overlaps(flat, enemy->GetFootprint()))) {
			enemy->StartAbsorb();
			count++;
		}
	}
	for (auto& projectile : projectiles_) {
		const Projectile::State state = projectile->GetState();
		const bool captured = state == Projectile::State::kCaptured;
		if (captured || (state == Projectile::State::kSurface && Field::Overlaps(flat, projectile->GetFootprint()))) {
			projectile->StartAbsorb();
			count++;
		}
	}
	if (count > 0) {
		LogEvent(std::format("restore: absorbing {} object(s)", count));
	}
}

void GameScene::HandleNormalContacts() {
	const Footprint body = player_.GetFootprint();
	// 敵に触れた：被弾（無敵中は無し）。walker は向きを反転して離れる
	for (auto& enemy : enemies_) {
		if (enemy->IsActive() && Field::Overlaps(body, enemy->GetFootprint())) {
			DamagePlayer(1, std::format("touched a {}", GetEnemyTypeName(enemy->GetType())));
			enemy->BounceOffPlayer(player_.GetTheta(), body.halfWidth);
		}
	}
	// 敵の弾に触れた：トゲは被弾して消える、爆弾は跳ね返す（無敵中でも跳ね返す）
	for (auto& projectile : projectiles_) {
		if (projectile->GetState() != Projectile::State::kSurface || !Field::Overlaps(body, projectile->GetFootprint())) {
			continue;
		}
		if (projectile->IsBomb()) {
			ReflectBomb(*projectile);
		} else {
			DamagePlayer(1, "hit by a spike");
			effects_.SpawnSparks(projectile->GetWorldPosition(), Projectile::kSpikeColor, 5, 6.0f);
			projectile->Kill();
		}
	}
}

void GameScene::HandleFriendlyBombs() {
	// 輪に沿って転がる味方の爆弾が敵に当たったら爆発（爆風の中の敵を倒す）
	for (auto& projectile : projectiles_) {
		if (projectile->GetState() != Projectile::State::kReflected) {
			continue;
		}
		for (auto& enemy : enemies_) {
			if (enemy->IsActive() && Field::Overlaps(projectile->GetFootprint(), enemy->GetFootprint())) {
				ExplodeBomb(*projectile);
				break;
			}
		}
	}
}

void GameScene::UpdateAbsorbingEnemies() {
	const Vector3 target = player_.GetCenterWorld();
	for (auto& enemy : enemies_) {
		if (enemy->GetState() == Enemy::State::kAbsorbing && enemy->UpdateAbsorb(target)) {
			LogEvent(std::format("absorbed a {}", GetEnemyTypeName(enemy->GetType())));
			KillEnemy(*enemy, true);
		}
	}
}

// ===== 出来事の処理 =====

void GameScene::ExplodeBomb(Projectile& bomb) {
	const Vector3 position = bomb.GetWorldPosition();
	effects_.SpawnExplosion(position, Projectile::kBlastRadius);
	effects_.SpawnSparks(position, {1.0f, 0.6f, 0.15f}, 8, 10.0f);
	PlaySe(seExplosion);
	// 爆風（輪の上の円。弧の差と高さで判定）
	const float theta = bomb.GetTheta();
	const float height = bomb.GetHeight();
	if (bomb.IsFriendly()) {
		// 味方の爆弾：爆風の中の敵を倒す（スライムにはダメージ無し）
		int killed = 0;
		for (auto& enemy : enemies_) {
			if (enemy->IsActive() && Field::CircleOverlaps(theta, height, Projectile::kBlastRadius, enemy->GetFootprint())) {
				KillEnemy(*enemy, false);
				killed++;
			}
		}
		LogEvent(std::format("friendly bomb exploded: {} enemy(s) down", killed));
	} else if (bomb.GetState() == Projectile::State::kCaptured) {
		// 平らなスライムに乗せたまま導火線が尽きた：飲み込んだときと同じダメージ（爆風の半径に関係なく。爆弾は跳ね返すのが唯一の正解）
		LogEvent("enemy bomb exploded ON the slime");
		DamagePlayer(Player::kBombAbsorbDamage, "bomb exploded on the slime");
	} else {
		// 敵の爆弾：爆風の中にスライムがいれば被弾
		LogEvent("enemy bomb exploded");
		if (Field::CircleOverlaps(theta, height, Projectile::kBlastRadius, player_.GetFootprint())) {
			DamagePlayer(1, "caught in a bomb blast");
		}
	}
	bomb.Kill();
}

void GameScene::ReflectBomb(Projectile& bomb) {
	if (IsBossPhase() && phaseState_ == PhaseState::kBattle && boss_.IsTargetable()) {
		// ボスが攻撃中：ボスへ直線で飛ばす
		bomb.LaunchToBoss();
		LogEvent("BOUNCED a bomb -> to the boss");
	} else {
		// それ以外：輪に沿って逆向きに速く転がす
		bomb.Reflect();
		LogEvent("BOUNCED a bomb");
	}
	// 弾力の演出（縦に伸びてぷるんと戻る）
	player_.AddSpringImpulse(Player::kBounceImpulse);
	PlaySe(seBounce);
	effects_.SpawnSparks(bomb.GetWorldPosition(), {0.5f, 1.0f, 0.6f}, 6, 6.0f);
}

void GameScene::KillEnemy(Enemy& enemy, bool absorbed) {
	const Vector3 center = enemy.GetCenterWorld();
	if (absorbed) {
		effects_.SpawnAbsorb(center, Player::kColor);
		PlaySe(seAbsorbEnemy);
	} else {
		effects_.SpawnExplosion(center, Enemy::kRadius * 1.6f);
		effects_.SpawnSparks(center, Enemy::kWalkerColor, 8, 8.0f);
	}
	enemy.Kill();
}

void GameScene::DamagePlayer(int amount, const std::string& cause) {
	if (player_.Damage(amount)) {
		PlaySe(seDamage);
		effects_.SpawnSparks(player_.GetCenterWorld(), {1.0f, 0.15f, 0.1f}, 10, 9.0f);
		gameCamera_.Shake(GameCamera::kShakeOnDamage);
		LogEvent(std::format("player damaged ({}): hp {}", cause, player_.GetHp()));
	}
}

// ===== プール =====

void GameScene::ApplySpawnRequests() {
	for (const ProjectileSpawn& request : spawnRequests_) {
		Projectile* projectile = AcquireProjectile();
		if (!projectile) {
			break; // プールが一杯（出さない）
		}
		projectile->Spawn(request, CurrentLayerZ());
	}
	spawnRequests_.clear();
}

void GameScene::ClearProjectiles(bool withEffect) {
	for (auto& projectile : projectiles_) {
		if (!projectile->IsInUse()) {
			continue;
		}
		if (withEffect) {
			effects_.SpawnSparks(projectile->GetWorldPosition(), {0.6f, 0.6f, 0.6f}, 3, 4.0f);
		}
		projectile->Kill();
	}
}

void GameScene::KillAllEnemies(bool includePending) {
	for (auto& enemy : enemies_) {
		if (enemy->IsInUse()) {
			KillEnemy(*enemy, false);
		}
	}
	if (includePending) {
		for (PendingSpawn& pending : pendingSpawns_) {
			pending.spawned = true;
		}
	}
}

Enemy* GameScene::AcquireEnemy() {
	for (auto& enemy : enemies_) {
		if (!enemy->IsInUse()) {
			return enemy.get();
		}
	}
	if (static_cast<int>(enemies_.size()) >= kMaxEnemies) {
		return nullptr;
	}
	auto enemy = std::make_unique<Enemy>();
	enemy->Initialize(&models_);
	enemies_.push_back(std::move(enemy));
	return enemies_.back().get();
}

Projectile* GameScene::AcquireProjectile() {
	for (auto& projectile : projectiles_) {
		if (!projectile->IsInUse()) {
			return projectile.get();
		}
	}
	if (static_cast<int>(projectiles_.size()) >= kMaxProjectiles) {
		return nullptr;
	}
	auto projectile = std::make_unique<Projectile>();
	projectile->Initialize(&models_);
	projectiles_.push_back(std::move(projectile));
	return projectiles_.back().get();
}

int GameScene::CountEnemiesInUse() const {
	return static_cast<int>(std::count_if(enemies_.begin(), enemies_.end(), [](const std::unique_ptr<Enemy>& enemy) { return enemy->IsInUse(); }));
}

int GameScene::CountProjectilesInUse() const {
	return static_cast<int>(std::count_if(projectiles_.begin(), projectiles_.end(), [](const std::unique_ptr<Projectile>& projectile) { return projectile->IsInUse(); }));
}

int GameScene::CountProjectilesAbsorbing() const {
	return static_cast<int>(
	    std::count_if(projectiles_.begin(), projectiles_.end(), [](const std::unique_ptr<Projectile>& projectile) { return projectile->GetState() == Projectile::State::kAbsorbing; }));
}
