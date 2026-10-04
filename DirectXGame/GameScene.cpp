#define NOMINMAX
#include "GameScene.h"
#include "GameConfig.h"
#include "GameInput.h"
#include "GlobalVariables.h"
#include "LineDrawer.h"
#include "MathUtil.h"
#include "Sounds.h"
#include <algorithm>
#include <format>

using namespace KamataEngine;

// 初期化・フェーズの進行（状態遷移）・描画・調整項目。
// 物の更新と当たり判定は GameSceneInteraction.cpp、ImGui は GameSceneDebug.cpp

namespace {
// 調整項目のグループ名（フェーズの進行の時間）
const char* kGroupName = "Phase";
} // namespace

// 1フレームの線の本数の見積もり（LineDrawer.h の冒頭）が上限に収まることを確かめる（プールや上限の定数を増やしたらここで止まる）。
// 輪の縁＋層の間の縦線＋予告マーカー＋判定の可視化（スライム・敵・弾。マーカーと判定は同じ弾で同時には出ないが、ここでは足し合わせて多めに見る）
static_assert(
    Field::kMaxVisibleLayers * 2 * Field::kMaxRingSegments + (Field::kMaxVisibleLayers - 1) * Field::kMaxTubeLines + GameScene::kMaxMarkersPerFrame * Projectile::kMarkerLineCount +
            (1 + GameScene::kMaxEnemies + GameScene::kMaxProjectiles) * Field::kFootprintLineCount <=
        LineDrawer::kLineBudget,
    "線の本数の見積もりが LineDrawer::kLineBudget を超える（LineDrawer.h の見積もりと上限の定数を見直す）");

GameScene::~GameScene() {
	// PrimitiveDrawer はカメラをポインタで覚えているので、破棄するカメラを指したままにしない
	PrimitiveDrawer::GetInstance()->SetCamera(nullptr);
	delete backgroundSprite_;
	// 共有のモデルとライト（各物はポインタを持つだけ）。WorldTransform などのメンバはこの後に自動で破棄される
	models_.Destroy();
}

void GameScene::Initialize() {
	// --- 共有のモデル・カメラ・ゲームの物 ---
	models_.Create();
	gameCamera_.Initialize();
	field_.Initialize(&models_);
	player_.Initialize(&models_);
	effects_.Initialize(&models_);
	boss_.Initialize(&models_, &effects_);

	// --- UI ---
	hud_.Initialize();
	backgroundSprite_ = Sprite::Create(TextureManager::Load("white1x1.png"), {0.0f, 0.0f});
	backgroundSprite_->SetSize({static_cast<float>(GameConfig::kScreenWidth), static_cast<float>(GameConfig::kScreenHeight)});
	pauseMenu_.Initialize();
	fade_.Initialize();
	fade_.Start(Fade::Status::kFadeIn, kFadeDuration);

	// --- フェーズ構成（読めなければ組み込みの既定構成。エラーは Game Debug に出る） ---
	if (!phaseData_.Load()) {
		phaseReloadMessage_ = "LOAD ERROR (using built-in default): " + phaseData_.GetError();
	}

	// 最初のフェーズから始める
	JumpToPhase(0);

	// 最初のフレームから正しい位置で描けるよう、一度反映しておく
	UpdateVisuals();
}

void GameScene::ApplyAllGlobalVariables() {
	// ImGuiでの変更を毎フレームゲームへ反映する（ゲーム中に使うクラスを増やしたらここに足す）
	GameScene::ApplyGlobalVariables();
	Field::ApplyGlobalVariables();
	GameCamera::ApplyGlobalVariables();
	Player::ApplyGlobalVariables();
	Enemy::ApplyGlobalVariables();
	Projectile::ApplyGlobalVariables();
	Boss::ApplyGlobalVariables();
	Effects::ApplyGlobalVariables();
	Hud::ApplyGlobalVariables();
	PauseMenu::ApplyGlobalVariables();
	ApplySoundGlobalVariables();
}

void GameScene::Update() {
	ApplyAllGlobalVariables();
	UpdateImGui();

	fade_.Update();

	switch (sceneStep_) {
	case SceneStep::kFadeIn:
		// フェードイン中からゲームは動き出す（メニューはまだ開けない）
		UpdatePlay();
		if (fade_.IsFinished()) {
			sceneStep_ = SceneStep::kPlay;
		}
		break;
	case SceneStep::kPlay:
		if (pauseMenu_.IsOpen()) {
			// ポーズ中：ゲームの更新を止めてメニューだけ動かす
			switch (pauseMenu_.Update()) {
			case PauseMenu::Choice::kToTitle:
				FinishScene(Result::kToTitle);
				break;
			case PauseMenu::Choice::kResume:
				// 決定（スペース＝平らと同じキー）で戻ったときだけ、そのまま平らにならないようにする。
				// ESC で戻ったときは押しっぱなしの平らをそのまま続ける（強制的に戻すと乗せている爆弾を飲み込んでしまう）
				if (GameInput::GetInstance()->IsTriggered(GameInput::Action::kDecide)) {
					player_.RequireFlattenRelease();
				}
				break;
			case PauseMenu::Choice::kNone:
				break;
			}
		} else if (GameInput::GetInstance()->IsTriggered(GameInput::Action::kMenu)) {
			pauseMenu_.Open();
		} else {
#ifdef USE_IMGUI
			// デバッグの一時停止中は、コマ送り（Step 1 Frame）が押されたフレームだけ進める
			if (!sDebugPause || debugStepOnce_) {
				debugStepOnce_ = false;
				UpdatePlay();
			}
#else
			UpdatePlay();
#endif
		}
		break;
	case SceneStep::kFadeOut:
		// 結果は確定済み。フェードアウトが終わったらシーン終了
		if (fade_.IsFinished()) {
			isFinished_ = true;
		}
		break;
	}

	// カメラ → 各物の見た目（一時停止中も毎フレーム。ImGuiでの調整がすぐ見えるように）
	UpdateVisuals();
}

void GameScene::UpdatePlay() {
	playFrame_++;

	// フェーズの時間経過と遷移（バナー → 戦闘 → クリア → 次の層へ …）
	UpdatePhaseState();

	const bool isWorldActive = phaseState_ == PhaseState::kIntro || phaseState_ == PhaseState::kBattle || phaseState_ == PhaseState::kCleared || phaseState_ == PhaseState::kAdvance;
	if (isWorldActive) {
		if (phaseState_ == PhaseState::kIntro || phaseState_ == PhaseState::kBattle) {
			SpawnPendingEnemies();
		}
		UpdateActors();
		ResolveInteractions();
		ApplySpawnRequests();
	} else {
		// ゲームクリア・敗北の演出中は、スライムとボス（撃破演出）だけ動かす（敵と弾は止める）
		player_.Update(0.0f, false, phaseState_ == PhaseState::kVictory);
		boss_.Update(false, player_.GetTheta(), player_.GetAngularSpeed(), &spawnRequests_);
		spawnRequests_.clear();
	}

	effects_.Update();
	gameCamera_.Update();
	// 層の移動中はスライムもカメラと一緒に次の層へ。角度は開始角度へ寄せる（どのフェーズも同じ位置から始まる＝Jump と同じ。
	// phases.json の angleDeg を「スライムは -90 から」として書ける）
	if (phaseState_ == PhaseState::kAdvance) {
		player_.SetZ(gameCamera_.GetFocusZ());
		player_.SetTheta(advanceFromTheta_ + advanceDeltaTheta_ * gameCamera_.GetMoveProgress());
	}

	// 結果の判定（HP0 → 敗北、クリア条件 → クリア）
	if (phaseState_ != PhaseState::kDefeat && phaseState_ != PhaseState::kVictory && player_.IsDead()) {
		OnPlayerDefeated();
	} else if (phaseState_ == PhaseState::kBattle && IsPhaseClearConditionMet()) {
		OnPhaseCleared();
	}
}

void GameScene::UpdatePhaseState() {
	const float dt = GameConfig::kDeltaTime;
	stateTimer_ += dt;
	phaseTimer_ += dt;

	switch (phaseState_) {
	case PhaseState::kIntro:
		// バナーが終わったら戦闘開始（ボスはここから攻撃する）
		if (stateTimer_ >= kIntroTime) {
			phaseState_ = PhaseState::kBattle;
			stateTimer_ = 0.0f;
		}
		break;
	case PhaseState::kCleared:
		// CLEAR! の後、カメラとスライムが次の層へ移動する（スライムは開始角度へ短い向きで寄っていく。ボスは同時に奥へ逃げる）
		if (stateTimer_ >= kClearedTime) {
			phaseState_ = PhaseState::kAdvance;
			stateTimer_ = 0.0f;
			gameCamera_.StartMove(CurrentLayerZ(), Field::LayerZ(phaseIndex_ + 1));
			advanceFromTheta_ = player_.GetTheta();
			advanceDeltaTheta_ = Field::WrapAngle(Player::GetStartTheta() - advanceFromTheta_);
			if (IsBossPhase()) {
				StartBossFlee();
			}
			LogEvent("advance to next layer");
		}
		break;
	case PhaseState::kAdvance:
		// カメラの移動が終わったら次のフェーズ
		if (!gameCamera_.IsMoving()) {
			StartPhase(phaseIndex_ + 1);
		}
		break;
	case PhaseState::kVictory:
		if (stateTimer_ >= kVictoryTime) {
			FinishScene(Result::kClear);
		}
		break;
	case PhaseState::kDefeat:
		if (stateTimer_ >= kDefeatTime) {
			FinishScene(Result::kGameOver);
		}
		break;
	case PhaseState::kBattle:
	default:
		break;
	}
}

void GameScene::StartPhase(int index) {
	phaseIndex_ = std::clamp(index, 0, phaseData_.GetCount() - 1);
	phaseState_ = PhaseState::kIntro;
	stateTimer_ = 0.0f;
	phaseTimer_ = 0.0f;
	player_.SetZ(CurrentLayerZ());

	// 雑魚の出現待ち（delay の順。同じ delay は書いた順）
	const PhaseDef& phase = CurrentPhase();
	pendingSpawns_.clear();
	for (size_t i = 0; i < phase.enemies.size(); i++) {
		PendingSpawn pending;
		pending.def = phase.enemies[i];
		pending.serial = static_cast<int>(i);
		pendingSpawns_.push_back(pending);
	}
	std::stable_sort(pendingSpawns_.begin(), pendingSpawns_.end(), [](const PendingSpawn& a, const PendingSpawn& b) { return a.def.delay < b.def.delay; });

	if (phase.isBoss) {
		// ボス登場（その層で待っていればそのまま、いなければ奥から飛来）
		boss_.StartPhase(CurrentLayerZ(), phase.bossLevel, phase.bossHp);
		PlaySe(seBossWarning);
	}
	PlayPhaseBgm();
	LogEvent(std::format("phase {} start: {}", phaseIndex_ + 1, phase.name));
}

bool GameScene::IsPhaseClearConditionMet() const {
	// 吸い込み中の弾（飲み込んだ爆弾・トゲ）があるうちは待つ（クリアで弾を消すと、飲み込んだ爆弾のダメージが無かったことになるため。0.2秒ほど）
	if (CountProjectilesAbsorbing() > 0) {
		return false;
	}
	if (IsBossPhase()) {
		// ボスフェーズ：ボスの体力0のみ（残った雑魚はクリア時にまとめて消す）
		return boss_.GetHp() <= 0;
	}
	// 雑魚フェーズ：全員出現させ、全員倒した
	for (const PendingSpawn& pending : pendingSpawns_) {
		if (!pending.spawned) {
			return false;
		}
	}
	return CountEnemiesInUse() == 0;
}

void GameScene::OnPhaseCleared() {
	const bool isLastPhase = (phaseIndex_ + 1 >= phaseData_.GetCount());
	// 残った雑魚と弾はまとめて消す
	KillAllEnemies(true);
	ClearProjectiles(true);
	PlaySe(sePhaseClear);
	LogEvent(std::format("phase {} cleared", phaseIndex_ + 1));

	if (isLastPhase) {
		// 最後のフェーズ：ゲームクリアへ（ボスなら撃破演出＝爆発の連鎖）
		phaseState_ = PhaseState::kVictory;
		stateTimer_ = 0.0f;
		if (IsBossPhase()) {
			boss_.StartDefeat();
		}
		return;
	}

	// ボスフェーズなら、ボスは CLEAR! の間はのけぞったまま見えていて、次の層への移動と同時に逃げる（StartBossFlee）
	phaseState_ = PhaseState::kCleared;
	stateTimer_ = 0.0f;
	player_.Heal(Player::kPhaseClearHeal);
}

void GameScene::StartBossFlee() {
	// ボスは次のボスフェーズの層へ逃げて待つ（以降にボスフェーズが無ければ消える）
	int nextBoss = -1;
	for (int i = phaseIndex_ + 1; i < phaseData_.GetCount(); i++) {
		if (phaseData_.Get(i).isBoss) {
			nextBoss = i;
			break;
		}
	}
	boss_.StartFlee(nextBoss >= 0, Field::LayerZ(std::max(nextBoss, 0)));
}

void GameScene::OnPlayerDefeated() {
	phaseState_ = PhaseState::kDefeat;
	stateTimer_ = 0.0f;
	player_.StartDefeat();
	effects_.SpawnSparks(player_.GetCenterWorld(), Player::kColor, 16, 9.0f);
	LogEvent("player defeated");
}

void GameScene::JumpToPhase(int index) {
	index = std::clamp(index, 0, phaseData_.GetCount() - 1);

	// 物をすべて片付けて、スライム（HP満タン）とカメラをその層に置く
	for (auto& enemy : enemies_) {
		enemy->Kill();
	}
	for (auto& projectile : projectiles_) {
		projectile->Kill();
	}
	spawnRequests_.clear();
	effects_.Clear();
	player_.Reset(Field::LayerZ(index));
	gameCamera_.SnapTo(Field::LayerZ(index));

	// ボスの置き場所：通常の進行と同じにする（前にボスフェーズがあれば、次のボスフェーズの層で待っている。
	// 前に無ければ、次のボスフェーズで奥から飛来する）
	bool hasEarlierBoss = false;
	int nextBoss = -1;
	for (int i = 0; i < phaseData_.GetCount(); i++) {
		if (!phaseData_.Get(i).isBoss) {
			continue;
		}
		if (i < index) {
			hasEarlierBoss = true;
		} else if (nextBoss < 0) {
			nextBoss = i;
		}
	}
	if (hasEarlierBoss && nextBoss >= 0) {
		boss_.PlaceIdle(Field::LayerZ(nextBoss));
	} else {
		boss_.Hide();
	}

	StartPhase(index);
}

void GameScene::PlayPhaseBgm() {
	// ボスフェーズはボスBGM（素材が無ければメインBGMのまま。同じ曲なら鳴らし直さない）
	if (IsBossPhase() && bgmBoss != kInvalidSound) {
		PlayBgm(bgmBoss);
	} else {
		PlayBgm(bgmMain);
	}
}

void GameScene::FinishScene(Result result) {
	// すでに結果が決まっていれば何もしない（フェードアウト中の二重確定を防ぐ）
	if (result_ != Result::kNone) {
		return;
	}
	result_ = result;
	pauseMenu_.Close();
	sceneStep_ = SceneStep::kFadeOut;
	fade_.Start(Fade::Status::kFadeOut, kFadeDuration);
}

void GameScene::UpdateVisuals() {
	// カメラ（一時停止中も調整項目・デバッグカメラが効くように毎フレーム）
	gameCamera_.UpdateMatrix();
	const Camera& camera = gameCamera_.GetCamera();
	Field::UpdateLightGroup(models_.lightGroup, camera.translation_);

	// 輪（描く層はゲームのカメラの位置で決める＝デバッグカメラで横から見ても同じ層が出る）
	field_.Update(gameCamera_.GetFocusZ() - GameCamera::kDistance, gameCamera_.GetFocusZ(), phaseData_.GetCount());

	// 物の行列・色
	player_.UpdateVisual();
	boss_.UpdateVisual();
	for (auto& enemy : enemies_) {
		enemy->UpdateVisual();
	}
	for (auto& projectile : projectiles_) {
		projectile->UpdateVisual();
	}
	effects_.UpdateVisual();

	backgroundSprite_->SetColor({Field::kBackgroundColor.x, Field::kBackgroundColor.y, Field::kBackgroundColor.z, 1.0f});
	UpdateHud();
}

void GameScene::UpdateHud() {
	Hud::Info info;
	info.hp = player_.GetHp();
	info.maxHp = Player::kMaxHp;
	info.phaseNumber = phaseIndex_ + 1;
	info.phaseCount = phaseData_.GetCount();
	info.isBossPhase = IsBossPhase();
	info.phaseName = CurrentPhase().name;
	info.showBossBar = IsBossPhase() && boss_.IsVisible() && (phaseState_ == PhaseState::kIntro || phaseState_ == PhaseState::kBattle);
	info.bossHp = boss_.GetHp();
	info.bossMaxHp = boss_.GetMaxHp();
	info.bossLevel = boss_.GetLevel();
	info.bannerTime = stateTimer_;
	// ボスフェーズの WARNING / CLEAR! / GAME CLEAR は、画面中央のボスに重ならない位置に出す
	info.isBossBanner = IsBossPhase();
	switch (phaseState_) {
	case PhaseState::kIntro:
		if (IsBossPhase()) {
			info.bannerText = "WARNING";
			info.bannerColor = {1.0f, 0.2f, 0.15f};
		} else {
			info.bannerText = std::format("PHASE {}", phaseIndex_ + 1);
			info.bannerColor = {0.3f, 0.55f, 1.0f};
		}
		info.bannerSubText = CurrentPhase().name;
		break;
	case PhaseState::kCleared:
		info.bannerText = "CLEAR!";
		info.bannerColor = {0.3f, 1.0f, 0.4f};
		break;
	case PhaseState::kVictory: {
		// 最後がボスなら、撃破演出（爆発の連鎖）が終わってから出す（見せ場を隠さない）
		const float delay = IsBossPhase() ? Boss::kDefeatTime : 0.0f;
		if (stateTimer_ >= delay) {
			info.bannerText = "GAME CLEAR";
			info.bannerColor = {1.0f, 0.85f, 0.2f};
			info.bannerTime = stateTimer_ - delay;
		}
		break;
	}
	case PhaseState::kDefeat:
		info.bannerText = "DEFEAT";
		info.bannerColor = {0.6f, 0.1f, 0.1f};
		info.isBossBanner = false;
		break;
	case PhaseState::kBattle:
	case PhaseState::kAdvance:
	default:
		break;
	}
	hud_.Update(info);
}

void GameScene::DrawFieldLines() {
	// 線を描く前に必ずカメラを渡す（未設定のまま描くと落ちる）。本数は LineDrawer が数えて上限で止める
	LineDrawer::BeginFrame(&gameCamera_.GetCamera());
	field_.DrawLines();
}

void GameScene::DrawOverlayLines() {
	// 着弾予告マーカー（スライムの体に隠れないよう半透明の後に描く。数は kMaxMarkersPerFrame まで）
	int markerCount = 0;
	for (const auto& projectile : projectiles_) {
		if (projectile->GetState() != Projectile::State::kAir) {
			continue;
		}
		if (markerCount >= kMaxMarkersPerFrame) {
			break;
		}
		projectile->DrawMarkerLines();
		markerCount++;
	}
	if (sShowHitboxes) {
		DrawHitboxLines();
	}
}

void GameScene::Draw() {
	ID3D12GraphicsCommandList* commandList = DirectXCommon::GetInstance()->GetCommandList();
	const Camera& camera = gameCamera_.GetCamera();

	// --- 背景の板（3Dより奥。描いた後に深度をクリアして3Dを上に描く）---
	Sprite::PreDraw(commandList);
	backgroundSprite_->Draw();
	Sprite::PostDraw();
	DirectXCommon::GetInstance()->ClearDepthBuffer();

	// --- 3D（不透明）。自作の OBJ は面の向き（巻き順）に頼らないよう、カリング無しで描く（閉じた形なので深度で正しく隠れる）---
	Model::PreDraw(Model::CullingMode::kNone);
	field_.Draw(camera);
	boss_.Draw(camera);
	for (const auto& enemy : enemies_) {
		enemy->Draw(camera);
	}
	for (const auto& projectile : projectiles_) {
		projectile->Draw(camera);
	}
	player_.DrawOpaque(camera);
	effects_.DrawOpaque(camera);
	Model::PostDraw();

	// --- 3D（不透明・表面カリング）：爆弾の縁取り（少し大きい光る球の裏側だけが、体のまわりに輪郭として見える）---
	Model::PreDraw(Model::CullingMode::kFront);
	for (const auto& projectile : projectiles_) {
		projectile->DrawOutline(camera);
	}
	Model::PostDraw();

	// --- 3Dの線（輪の縁・筒の縦線。半透明のスライムの体の奥に見えるよう、半透明より先）---
	DrawFieldLines();

	// --- 3D（半透明。深度は読むだけ）：スライムの体・取り込みの粒 ---
	Model::PreDraw(Model::CullingMode::kBack, Model::BlendMode::kNormal, Model::DepthTestMode::kReadOnly);
	player_.DrawTranslucent(camera);
	effects_.DrawTranslucent(camera);
	Model::PostDraw();

	// --- 3Dの線（着弾予告マーカー・判定の可視化。スライムの体に色を混ぜられて見えにくくならないよう、半透明の後）---
	DrawOverlayLines();

	// --- 3D（加算）：爆発・爆弾の火花 ---
	Model::PreDraw(Model::CullingMode::kBack, Model::BlendMode::kAdd, Model::DepthTestMode::kReadOnly);
	for (const auto& projectile : projectiles_) {
		projectile->DrawAdditive(camera);
	}
	effects_.DrawAdditive(camera);
	Model::PostDraw();

	// --- スプライト（HUD・文字・ポーズメニュー）---
	Sprite::PreDraw(commandList);
	hud_.Draw();
	// ポーズメニューは開いていればオーバーレイ・項目を重ねる（それまでの文字はオーバーレイの奥に確定させてから描く）
	pauseMenu_.Draw();
	DebugText::GetInstance()->DrawAll();
	Sprite::PostDraw();

	// フェードは最前面
	fade_.Draw();
}

void GameScene::RegisterGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	globalVariables->CreateGroup(kGroupName);
	globalVariables->AddItem(kGroupName, "FadeDuration", kFadeDuration);
	globalVariables->AddItem(kGroupName, "IntroTime", kIntroTime);
	globalVariables->AddItem(kGroupName, "ClearedTime", kClearedTime);
	globalVariables->AddItem(kGroupName, "VictoryTime", kVictoryTime);
	globalVariables->AddItem(kGroupName, "DefeatTime", kDefeatTime);
}

void GameScene::ApplyGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	// 時間は0にすると割り算や遷移が壊れるので下限を付ける
	kFadeDuration = std::max(globalVariables->GetFloatValue(kGroupName, "FadeDuration"), 0.01f);
	kIntroTime = std::max(globalVariables->GetFloatValue(kGroupName, "IntroTime"), 0.0f);
	kClearedTime = std::max(globalVariables->GetFloatValue(kGroupName, "ClearedTime"), 0.0f);
	kVictoryTime = std::max(globalVariables->GetFloatValue(kGroupName, "VictoryTime"), 0.0f);
	kDefeatTime = std::max(globalVariables->GetFloatValue(kGroupName, "DefeatTime"), 0.0f);
}
