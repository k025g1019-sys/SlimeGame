#include "PhaseData.h"
#include <climits>
#include <cmath>
#include <cstdint>
#include <format>
#include <fstream>
#include <initializer_list>
#include <nlohmann/json.hpp>
#include <sstream>

using json = nlohmann::json;

namespace {
// 書いてよい項目名（これ以外の名前は書き間違いとしてエラーにする。省略できる項目の書き間違いが既定値のまま黙って動くのを防ぐ）
const std::initializer_list<const char*> kRootKeys = {"phases"};
const std::initializer_list<const char*> kPhaseKeys = {"type", "name", "bossLevel", "hp", "enemies"};
const std::initializer_list<const char*> kEnemyKeys = {"type", "angleDeg", "delay", "dir"};

// オブジェクトに知らない項目名があれば、その名前を返す（無ければ空）
std::string FindUnknownKey(const json& object, const std::initializer_list<const char*>& allowed) {
	for (const auto& item : object.items()) {
		bool known = false;
		for (const char* key : allowed) {
			if (item.key() == key) {
				known = true;
				break;
			}
		}
		if (!known) {
			return item.key();
		}
	}
	return std::string();
}

// 有限の数を範囲つきで読む（float にすると無限大になる大きな数を弾く。範囲外・数でないなら false）
bool ReadFiniteNumber(const json& value, double minValue, double maxValue, float* out) {
	if (!value.is_number()) {
		return false;
	}
	const double number = value.get<double>();
	if (!std::isfinite(number) || number < minValue || number > maxValue) {
		return false;
	}
	*out = static_cast<float>(number);
	return true;
}

// 整数を範囲つきで読む（int に入らない大きな数が回り込んで範囲内に見えるのを防ぐため、64ビットのまま比べる）
bool ReadInteger(const json& value, int minValue, int maxValue, int* out) {
	if (!value.is_number_integer()) {
		return false;
	}
	int64_t number = 0;
	if (value.is_number_unsigned()) {
		const uint64_t unsignedNumber = value.get<uint64_t>();
		if (unsignedNumber > static_cast<uint64_t>(INT_MAX)) {
			return false;
		}
		number = static_cast<int64_t>(unsignedNumber);
	} else {
		number = value.get<int64_t>();
	}
	if (number < minValue || number > maxValue) {
		return false;
	}
	*out = static_cast<int>(number);
	return true;
}

// 敵の種類名 → EnemyType（phases.json の "type"）
bool ParseEnemyType(const std::string& name, EnemyType* out) {
	if (name == "walker") {
		*out = EnemyType::kWalker;
		return true;
	}
	if (name == "shooter") {
		*out = EnemyType::kShooter;
		return true;
	}
	if (name == "bomber") {
		*out = EnemyType::kBomber;
		return true;
	}
	return false;
}

// 雑魚敵1体の配置を作る（MakeDefault を短く書くため）
EnemySpawnDef MakeEnemy(EnemyType type, float angleDeg, float delay, int dir = -1) {
	EnemySpawnDef def;
	def.type = type;
	def.angleDeg = angleDeg;
	def.delay = delay;
	def.dir = dir;
	return def;
}
} // namespace

bool PhaseData::Load() {
	std::string loadError;
	std::vector<PhaseDef> loaded;
	std::ifstream file(kFilePath);
	if (!file.is_open()) {
		loadError = std::string("open failed: ") + kFilePath;
	} else {
		std::stringstream stream;
		stream << file.rdbuf();
		if (Parse(stream.str(), &loaded, &loadError)) {
			// 全部検証できたときだけ差し替える
			phases_ = std::move(loaded);
			error_.clear();
			usingDefault_ = false;
			return true;
		}
	}
	// 失敗：今の構成のまま（まだ何も無ければ組み込みの既定構成）。エラーは Game Debug に表示する
	error_ = loadError;
	if (phases_.empty()) {
		phases_ = MakeDefault();
		usingDefault_ = true;
	}
	return false;
}

bool PhaseData::Parse(const std::string& text, std::vector<PhaseDef>* out, std::string* error) {
	// コメントを許可して読む（第4引数 ignore_comments = true）。書き間違いは例外で受け止めてエラー文にする
	json root;
	try {
		root = json::parse(text, nullptr, true, true);
	} catch (const json::exception& exception) {
		*error = std::string("json: ") + exception.what();
		return false;
	}
	if (!root.is_object() || !root.contains("phases") || !root.at("phases").is_array()) {
		*error = "root: must be { \"phases\": [ ... ] }";
		return false;
	}
	if (const std::string unknown = FindUnknownKey(root, kRootKeys); !unknown.empty()) {
		*error = "root: unknown key \"" + unknown + "\" (only \"phases\")";
		return false;
	}
	const json& phases = root.at("phases");
	if (phases.empty()) {
		*error = "phases: needs at least 1 phase";
		return false;
	}
	if (phases.size() > static_cast<size_t>(kMaxPhases)) {
		*error = std::format("phases: too many ({} > {})", phases.size(), kMaxPhases);
		return false;
	}

	std::vector<PhaseDef> result;
	for (size_t i = 0; i < phases.size(); i++) {
		const json& phase = phases.at(i);
		const std::string path = std::format("phases[{}]", i);
		if (!phase.is_object()) {
			*error = path + ": must be an object { \"type\": ... }";
			return false;
		}
		if (const std::string unknown = FindUnknownKey(phase, kPhaseKeys); !unknown.empty()) {
			*error = path + ": unknown key \"" + unknown + "\" (type / name / bossLevel / hp / enemies)";
			return false;
		}
		PhaseDef def;

		// 種類（wave / boss）
		if (!phase.contains("type") || !phase.at("type").is_string()) {
			*error = path + ".type: must be \"wave\" or \"boss\"";
			return false;
		}
		const std::string type = phase.at("type").get<std::string>();
		if (type == "wave") {
			def.isBoss = false;
		} else if (type == "boss") {
			def.isBoss = true;
		} else {
			*error = path + ".type: unknown \"" + type + "\" (wave / boss)";
			return false;
		}

		// 表示名（省略可）
		if (phase.contains("name")) {
			if (!phase.at("name").is_string()) {
				*error = path + ".name: must be a string";
				return false;
			}
			def.name = phase.at("name").get<std::string>();
			if (def.name.size() > static_cast<size_t>(kMaxNameLength)) {
				*error = std::format("{}.name: too long ({} > {} characters)", path, def.name.size(), kMaxNameLength);
				return false;
			}
		} else {
			def.name = std::format("{} {}", def.isBoss ? "BOSS" : "WAVE", i + 1);
		}

		// ボスの強さと体力（ボスのときだけ。省略可）
		if (def.isBoss) {
			if (phase.contains("bossLevel") && !ReadInteger(phase.at("bossLevel"), 1, kMaxBossLevel, &def.bossLevel)) {
				*error = std::format("{}.bossLevel: must be an integer 1-{}", path, kMaxBossLevel);
				return false;
			}
			if (phase.contains("hp") && !ReadInteger(phase.at("hp"), 1, kMaxBossHp, &def.bossHp)) {
				*error = std::format("{}.hp: must be an integer 1-{}", path, kMaxBossHp);
				return false;
			}
		}

		// 雑魚（wave は1体以上必要。boss は省略可）
		if (phase.contains("enemies")) {
			const json& enemies = phase.at("enemies");
			if (!enemies.is_array()) {
				*error = path + ".enemies: must be an array [ {...}, ... ]";
				return false;
			}
			if (enemies.size() > static_cast<size_t>(kMaxEnemiesPerPhase)) {
				*error = std::format("{}.enemies: too many ({} > {})", path, enemies.size(), kMaxEnemiesPerPhase);
				return false;
			}
			for (size_t j = 0; j < enemies.size(); j++) {
				const json& enemy = enemies.at(j);
				const std::string enemyPath = std::format("{}.enemies[{}]", path, j);
				if (!enemy.is_object()) {
					*error = enemyPath + ": must be an object { \"type\": ..., \"angleDeg\": ... }";
					return false;
				}
				if (const std::string unknown = FindUnknownKey(enemy, kEnemyKeys); !unknown.empty()) {
					*error = enemyPath + ": unknown key \"" + unknown + "\" (type / angleDeg / delay / dir)";
					return false;
				}
				EnemySpawnDef spawn;
				if (!enemy.contains("type") || !enemy.at("type").is_string() || !ParseEnemyType(enemy.at("type").get<std::string>(), &spawn.type)) {
					*error = enemyPath + ".type: must be \"walker\" / \"shooter\" / \"bomber\"";
					return false;
				}
				if (!enemy.contains("angleDeg") || !ReadFiniteNumber(enemy.at("angleDeg"), -kMaxAngleDeg, kMaxAngleDeg, &spawn.angleDeg)) {
					*error = std::format("{}.angleDeg: must be a number -{}..{} (degrees. 0=right, 90=top)", enemyPath, kMaxAngleDeg, kMaxAngleDeg);
					return false;
				}
				if (enemy.contains("delay") && !ReadFiniteNumber(enemy.at("delay"), 0.0, kMaxDelay, &spawn.delay)) {
					*error = std::format("{}.delay: must be a number 0..{} (seconds)", enemyPath, kMaxDelay);
					return false;
				}
				if (enemy.contains("dir") && (!ReadInteger(enemy.at("dir"), -1, 1, &spawn.dir) || spawn.dir == 0)) {
					*error = enemyPath + ".dir: must be 1 or -1";
					return false;
				}
				def.enemies.push_back(spawn);
			}
		}
		if (!def.isBoss && def.enemies.empty()) {
			*error = path + ".enemies: a wave needs at least 1 enemy";
			return false;
		}
		result.push_back(std::move(def));
	}

	*out = std::move(result);
	return true;
}

std::vector<PhaseDef> PhaseData::MakeDefault() {
	// phases.json の初期内容と同じ（ファイルが無い・壊れているときに使う）
	std::vector<PhaseDef> phases;

	// clang-format off: 1体1行のまま並べる（phases.json と見比べやすいように）
	PhaseDef wave1;
	wave1.name = "WAVE 1";
	wave1.enemies = {
	    MakeEnemy(EnemyType::kWalker, 45.0f, 1.5f),
	    MakeEnemy(EnemyType::kShooter, 90.0f, 1.0f),
	    MakeEnemy(EnemyType::kBomber, 120.0f, 3.0f),
	    MakeEnemy(EnemyType::kWalker, 200.0f, 4.0f),
	};
	phases.push_back(wave1);

	PhaseDef boss1;
	boss1.isBoss = true;
	boss1.name = "BOSS 1";
	boss1.bossLevel = 1;
	boss1.bossHp = 3;
	phases.push_back(boss1);

	PhaseDef wave2;
	wave2.name = "WAVE 2";
	wave2.enemies = {
	    MakeEnemy(EnemyType::kWalker, 30.0f, 0.0f),
	    MakeEnemy(EnemyType::kBomber, 90.0f, 1.0f),
	    MakeEnemy(EnemyType::kShooter, 150.0f, 0.5f),
	    MakeEnemy(EnemyType::kShooter, 300.0f, 2.0f),
	    MakeEnemy(EnemyType::kBomber, 230.0f, 3.0f),
	};
	phases.push_back(wave2);

	PhaseDef boss2;
	boss2.isBoss = true;
	boss2.name = "BOSS 2";
	boss2.bossLevel = 2;
	boss2.bossHp = 4;
	boss2.enemies = {
	    MakeEnemy(EnemyType::kWalker, 90.0f, 3.0f),
	};
	phases.push_back(boss2);

	PhaseDef wave3;
	wave3.name = "WAVE 3";
	wave3.enemies = {
	    MakeEnemy(EnemyType::kWalker, 0.0f, 0.0f),
	    MakeEnemy(EnemyType::kWalker, 180.0f, 1.0f, 1),
	    MakeEnemy(EnemyType::kShooter, 60.0f, 1.5f),
	    MakeEnemy(EnemyType::kBomber, 120.0f, 2.0f),
	    MakeEnemy(EnemyType::kShooter, 240.0f, 2.5f),
	    MakeEnemy(EnemyType::kBomber, 300.0f, 4.0f),
	};
	phases.push_back(wave3);

	PhaseDef boss3;
	boss3.isBoss = true;
	boss3.name = "BOSS 3";
	boss3.bossLevel = 3;
	boss3.bossHp = 5;
	boss3.enemies = {
	    MakeEnemy(EnemyType::kShooter, 90.0f, 2.0f),
	    MakeEnemy(EnemyType::kBomber, 200.0f, 4.0f),
	};
	phases.push_back(boss3);
	// clang-format on

	return phases;
}
