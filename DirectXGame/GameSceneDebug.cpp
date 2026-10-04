#define NOMINMAX
#include "GameInput.h"
#include "GameScene.h"
#include "GlobalVariables.h"
#include "LineDrawer.h"
#include "MathUtil.h"
#include "Sounds.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

using namespace KamataEngine;

// GameScene のデバッグ機能（ImGui の Game Debug ウィンドウ・出来事のログ・判定の可視化）。
// ImGui は Debug / Develop のみ（Release では USE_IMGUI が無いので中身が空になる）

namespace {
// 出来事のログの行数
const size_t kMaxEventLog = 14;
constexpr float kRadToDeg = 180.0f / std::numbers::pi_v<float>;
// デバッグカメラの上下の向きの限界（度）
const float kMaxOrbitPitchDeg = 89.0f;
} // namespace

void GameScene::LogEvent(const std::string& text) {
#ifdef USE_IMGUI
	eventLog_.push_front(std::format("[{:6}] {}", playFrame_, text));
	while (eventLog_.size() > kMaxEventLog) {
		eventLog_.pop_back();
	}
#else
	static_cast<void>(text);
#endif
}

void GameScene::DrawHitboxLines() {
	// 足元の判定（弧×高さの箱）を手前の縁に描く。スライム=緑（平らは水色）、敵=桃（出現中は灰・乗っている物は白）、弾=黄（味方は緑）
	const float layerZ = CurrentLayerZ();
	Field::DrawFootprintLines(player_.GetFootprint(), player_.GetZ(), player_.IsFlat() ? Vector4{0.3f, 1.0f, 1.0f, 1.0f} : Vector4{0.3f, 1.0f, 0.3f, 1.0f});
	for (const auto& enemy : enemies_) {
		if (!enemy->IsInUse()) {
			continue;
		}
		Vector4 color = {1.0f, 0.4f, 0.9f, 1.0f};
		if (enemy->GetState() == Enemy::State::kSpawning) {
			color = {0.5f, 0.5f, 0.5f, 1.0f};
		} else if (enemy->GetState() != Enemy::State::kActive) {
			color = {1.0f, 1.0f, 1.0f, 1.0f};
		}
		Field::DrawFootprintLines(enemy->GetFootprint(), layerZ, color);
	}
	for (const auto& projectile : projectiles_) {
		const Projectile::State state = projectile->GetState();
		if (state == Projectile::State::kInactive || state == Projectile::State::kAir || state == Projectile::State::kToBoss) {
			continue; // 空中にある物は判定なし
		}
		Vector4 color = {1.0f, 0.9f, 0.2f, 1.0f};
		if (projectile->IsFriendly()) {
			color = {0.3f, 1.0f, 0.4f, 1.0f};
		} else if (state != Projectile::State::kSurface) {
			color = {1.0f, 1.0f, 1.0f, 1.0f};
		}
		Field::DrawFootprintLines(projectile->GetFootprint(), layerZ, color);
	}
}

void GameScene::UpdateImGui() {
#ifdef USE_IMGUI
	// 調整項目の表示・編集ウィンドウ（各グループのSaveボタンでJSON保存できる）
	GlobalVariables::GetInstance()->Update();

	// ゲームの状態確認・デバッグ操作用ウィンドウ（初回だけ右側に置く。以降は動かした位置が imgui.ini に残る。
	// 高さは下の操作説明の帯（y=1016～）に重ならない大きさ）
	ImGui::SetNextWindowPos(ImVec2(1480.0f, 280.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(420.0f, 720.0f), ImGuiCond_FirstUseEver);
	ImGui::Begin("Game Debug");

	// --- 状態 ---
	static const char* const kStateNames[] = {"Intro", "Battle", "Cleared", "Advance", "Victory", "Defeat"};
	ImGui::Text("frame %d   phase %d/%d %s   [%s] %.1fs", playFrame_, phaseIndex_ + 1, phaseData_.GetCount(), IsBossPhase() ? "BOSS" : "WAVE", kStateNames[static_cast<int>(phaseState_)], stateTimer_);
	ImGui::Text("player: %.0f deg  speed %.2f  hp %d  %s", player_.GetTheta() * kRadToDeg, player_.GetSpeed(), player_.GetHp(), player_.IsFlat() ? "FLAT" : "normal");
	static const char* const kBossStateNames[] = {"absent", "enter", "attack", "hit", "flee", "idle far", "defeated"};
	ImGui::Text("boss: %s  hp %d/%d  lv %d", kBossStateNames[static_cast<int>(boss_.GetState())], boss_.GetHp(), boss_.GetMaxHp(), boss_.GetLevel());
	ImGui::Text(
	    "enemies %d/%d  projectiles %d/%d  particles %d", CountEnemiesInUse(), static_cast<int>(enemies_.size()), CountProjectilesInUse(), static_cast<int>(projectiles_.size()),
	    effects_.GetActiveCount());
	ImGui::Text("lines %d / %d (dropped %d)", LineDrawer::GetCount(), LineDrawer::kLineBudget, LineDrawer::GetDroppedCount());

	// --- 一時停止・コマ送り・無敵・判定の可視化 ---
	ImGui::Separator();
	ImGui::Checkbox("Pause", &sDebugPause);
	ImGui::SameLine();
	if (ImGui::Button("Step 1 Frame")) {
		sDebugPause = true;
		debugStepOnce_ = true;
	}
	ImGui::Checkbox("Invincible", &Player::sDebugInvincible);
	ImGui::SameLine();
	ImGui::Checkbox("Show Hitboxes", &sShowHitboxes);
	if (ImGui::Button("Kill All Enemies")) {
		KillAllEnemies(true);
		LogEvent("debug: kill all enemies");
	}
	ImGui::SameLine();
	if (ImGui::Button("Boss Damage +1")) {
		// 本物の当たりと同じく、攻撃中・のけぞり中だけ（飛来中に当てると攻撃の位置がずれるため）
		if (IsBossPhase() && boss_.IsTargetable()) {
			boss_.Hit();
			PlaySe(seBossHit);
			LogEvent(std::format("debug: boss hp {}/{}", boss_.GetHp(), boss_.GetMaxHp()));
		}
	}

	// --- フェーズ一覧と Jump・Reload Phases ---
	ImGui::Separator();
	ImGui::Text("Phases (%s)", phaseData_.IsUsingDefault() ? "built-in default" : "phases.json");
	for (int i = 0; i < phaseData_.GetCount(); i++) {
		const PhaseDef& phase = phaseData_.Get(i);
		ImGui::PushID(i);
		if (ImGui::SmallButton("Jump")) {
			JumpToPhase(i);
			LogEvent(std::format("debug: jump to phase {}", i + 1));
		}
		ImGui::SameLine();
		const std::string kind = phase.isBoss ? std::format("BOSS lv{} hp{}", phase.bossLevel, phase.bossHp) : std::string("WAVE");
		ImGui::Text("%s %d: %s  %s  (%d enemies)", (i == phaseIndex_) ? ">" : " ", i + 1, kind.c_str(), phase.name.c_str(), static_cast<int>(phase.enemies.size()));
		ImGui::PopID();
	}
	if (ImGui::Button("Reload Phases")) {
		if (phaseData_.Load()) {
			phaseReloadMessage_ = std::format("reloaded: {} phases", phaseData_.GetCount());
			// フェーズの数が変わっていても壊れないよう、今のフェーズ（の番号）からやり直す
			JumpToPhase(std::min(phaseIndex_, phaseData_.GetCount() - 1));
		} else {
			phaseReloadMessage_ = "RELOAD ERROR (kept current): " + phaseData_.GetError();
		}
	}
	if (!phaseReloadMessage_.empty()) {
		const bool isError = phaseReloadMessage_.find("ERROR") != std::string::npos;
		ImGui::PushStyleColor(ImGuiCol_Text, isError ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f) : ImVec4(0.5f, 1.0f, 0.5f, 1.0f));
		ImGui::TextWrapped("%s", phaseReloadMessage_.c_str());
		ImGui::PopStyleColor();
	}

	// --- 入力（Input.json の再読み込み） ---
	ImGui::Separator();
	{
		GameInput* input = GameInput::GetInstance();
		if (!input->GetBindingError().empty()) {
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "INPUT ERROR: %s", input->GetBindingError().c_str());
		}
		ImGui::Text(
		    "keys: menu=%02X decide=%02X flatten=%02X  pad: %s", input->GetKeyboardVk(GameInput::Action::kMenu), input->GetKeyboardVk(GameInput::Action::kDecide),
		    input->GetKeyboardVk(GameInput::Action::kFlatten), input->IsPadConnected() ? "connected" : "none");
		if (ImGui::Button("Reload Input")) {
			input->LoadBindings();
			LogEvent("debug: reload input");
		}
	}

	// --- デバッグカメラ（注視点のまわりを回る。筒を横から見る用） ---
	ImGui::Separator();
	GameCamera::DebugOrbit& orbit = gameCamera_.GetDebugOrbit();
	ImGui::Checkbox("Debug Camera (orbit)", &orbit.enabled);
	if (orbit.enabled) {
		ImGui::DragFloat("Yaw (deg)", &orbit.yawDeg, 0.5f);
		ImGui::DragFloat("Pitch (deg)", &orbit.pitchDeg, 0.5f, -kMaxOrbitPitchDeg, kMaxOrbitPitchDeg);
		ImGui::DragFloat("Distance", &orbit.distance, 0.1f, 1.0f, 900.0f);
		ImGui::DragFloat("Target Offset Z", &orbit.targetOffsetZ, 0.1f);
		ImGui::TextDisabled("right-drag: rotate / wheel: zoom (outside ImGui windows)");
		const ImGuiIO& io = ImGui::GetIO();
		if (!io.WantCaptureMouse) {
			if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
				orbit.yawDeg += io.MouseDelta.x * 0.3f;
				orbit.pitchDeg = std::clamp(orbit.pitchDeg + io.MouseDelta.y * 0.3f, -kMaxOrbitPitchDeg, kMaxOrbitPitchDeg);
			}
			if (io.MouseWheel != 0.0f) {
				orbit.distance = std::clamp(orbit.distance * ((io.MouseWheel > 0.0f) ? 0.9f : 1.1f), 1.0f, 900.0f);
			}
		}
	}

	// --- シーン遷移の確認用（結果を確定してフェードアウトする） ---
	ImGui::Separator();
	if (ImGui::Button("Force Clear")) {
		FinishScene(Result::kClear);
	}
	ImGui::SameLine();
	if (ImGui::Button("Force GameOver")) {
		FinishScene(Result::kGameOver);
	}
	ImGui::SameLine();
	if (ImGui::Button("To Title")) {
		FinishScene(Result::kToTitle);
	}

	// --- 出来事のログ（新しい順） ---
	ImGui::Separator();
	ImGui::Text("Event log");
	for (const std::string& line : eventLog_) {
		ImGui::TextUnformatted(line.c_str());
	}
	ImGui::End();
#endif
}
