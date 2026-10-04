#pragma once
#include "Boss.h"
#include "Effects.h"
#include "Enemy.h"
#include "Fade.h"
#include "Field.h"
#include "GameCamera.h"
#include "GameModels.h"
#include "Hud.h"
#include "KamataEngine.h"
#include "PauseMenu.h"
#include "PhaseData.h"
#include "Player.h"
#include "Projectile.h"
#include <deque>
#include <memory>
#include <string>
#include <vector>

/// <summary>
/// ゲームシーン（スライム（仮題）の本体）。フェーズの進行・カメラ・当たり判定（取り込み・跳ね返し）・描画をまとめる。
/// 見通しのため、メンバ関数の実装を3つのファイルに分けている：
/// 　GameScene.cpp            … 初期化・フェーズの進行（状態遷移）・描画・調整項目
/// 　GameSceneInteraction.cpp … 1フレームの物の更新と当たり判定（乗る・取り込む・跳ね返す・爆発）
/// 　GameSceneDebug.cpp       … ImGui の Game Debug ウィンドウ（Debug/Develop のみ）
///
/// 【フェーズの状態】kIntro（PHASE n / WARNING のバナー。敵は delay に従って出現）→ kBattle →
/// 　kCleared（CLEAR!。弾は消える）→ kAdvance（カメラとスライムが次の層へ。スライムは開始角度（下）へ寄っていき、
/// 　ボスフェーズの後ならボスが同時に奥へ逃げる）→ 次の kIntro。どのフェーズもスライムは開始角度から始まる（Jump と同じ）。
/// 　最後のフェーズをクリア → kVictory → Result::kClear。HP0 → kDefeat（つぶれる演出）→ Result::kGameOver
/// 【描画の順番】背景の板 → 不透明の3D → 爆弾の縁取り（表面カリング）→ 線（輪の縁・縦線）→ 半透明（スライムの体）→
/// 　線（予告マーカー・判定の可視化。スライムに隠れないよう半透明の後）→ 加算（爆発）→ HUD → フェード
/// </summary>
class GameScene {
public:
	// シーン終了時の結果
	enum class Result {
		kNone,     // まだ終了していない
		kGameOver, // ゲームオーバー → ゲームオーバーシーンへ
		kClear,    // クリア → ゲームクリアシーンへ
		kToTitle,  // メニューからタイトルへ戻る
	};

	~GameScene();

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// 更新
	/// </summary>
	void Update();

	/// <summary>
	/// 描画
	/// </summary>
	void Draw();

	// シーンが終了したか（mainが結果に応じて次のシーンへ切り替える）
	bool IsFinished() const { return isFinished_; }
	// シーン終了時の結果
	Result GetResult() const { return result_; }

	// 調整項目を登録（既定値をGlobalVariablesへ追加する）
	static void RegisterGlobalVariables();
	// 調整項目を適用（GlobalVariablesの現在値を静的メンバへ反映する）
	static void ApplyGlobalVariables();

	// 敵・弾のプールの上限（敵は1フェーズの最大数と同じ。線の本数の見積もり（LineDrawer.h）にも使う）
	static constexpr int kMaxEnemies = PhaseData::kMaxEnemiesPerPhase;
	static constexpr int kMaxProjectiles = 64;
	// 1フレームに描く着弾予告マーカーの数の上限（空中の弾がこれより多いときは古い順に描く。線の本数の見積もりに使う）
	static constexpr int kMaxMarkersPerFrame = 24;

private:
	// シーンの進行（フェードと結果の受け渡し）
	enum class SceneStep {
		kFadeIn,  // フェードイン中（ゲームは動き出している）
		kPlay,    // プレイ中
		kFadeOut, // フェードアウト中（結果は result_ に確定済み）
	};
	// フェーズの状態
	enum class PhaseState {
		kIntro,   // バナー表示中（敵は出現し始める）
		kBattle,  // 戦闘中（クリア条件を見る）
		kCleared, // クリアのバナー
		kAdvance, // 次の層へ移動中
		kVictory, // 最後のフェーズをクリア（ゲームクリアのバナー）
		kDefeat,  // HP0（つぶれる演出）
	};
	// フェーズの雑魚の出現待ち
	struct PendingSpawn {
		EnemySpawnDef def;
		int serial = 0; // フェーズ内の通し番号（攻撃のタイミングを決定的にずらす）
		bool spawned = false;
	};

	// ===== GameScene.cpp =====
	// ゲーム中に使う全クラスの調整項目を反映する（毎フレーム。ImGuiの編集が即反映される）
	void ApplyAllGlobalVariables();
	// ゲーム本体の1フレーム分の進行（ポーズ中・デバッグの一時停止中は呼ばれない）
	void UpdatePlay();
	// フェーズの状態の時間経過と遷移
	void UpdatePhaseState();
	// フェーズを始める（kIntro へ）
	void StartPhase(int index);
	// 今のフェーズをクリアした
	void OnPhaseCleared();
	// HP が0になった
	void OnPlayerDefeated();
	// フェーズのクリア条件を満たしたか（吸い込み中の弾があるうちはクリアにしない＝飲み込んだ爆弾のダメージを消さない）
	bool IsPhaseClearConditionMet() const;
	// ボスを奥へ逃がす（次の層への移動の開始と同時。次のボスフェーズの層で待つ。以降に無ければ消える）
	void StartBossFlee();
	// 指定のフェーズの層から始め直す（開始時・デバッグの Jump。HP は満タンに戻る）
	void JumpToPhase(int index);
	// フェーズに合わせた BGM（ボスフェーズは bgm_boss。無ければ bgm_main のまま）
	void PlayPhaseBgm();
	// 見た目（行列・色）とカメラの更新（一時停止中も毎フレーム）
	void UpdateVisuals();
	// HUD に渡す内容を作る
	void UpdateHud();
	// 線を描く（Model の PreDraw～PostDraw の外で）：輪の縁・縦線（半透明の前）／予告マーカー・判定の可視化（半透明の後）
	void DrawFieldLines();
	void DrawOverlayLines();
	// 結果を確定してフェードアウトを始める（2回目以降は無視）
	void FinishScene(Result result);
	// 今のフェーズ・ボスフェーズか・今の層の z
	const PhaseDef& CurrentPhase() const { return phaseData_.Get(phaseIndex_); }
	bool IsBossPhase() const { return CurrentPhase().isBoss; }
	float CurrentLayerZ() const { return Field::LayerZ(phaseIndex_); }

	// ===== GameSceneInteraction.cpp =====
	// スライム・ボス・敵・弾を1フレーム進める（弾の出来事＝爆発・着弾なども処理する）
	void UpdateActors();
	// 出現時間が来た雑魚を出す（スライムに近すぎる位置は Enemy::kSpawnSafeAngleDeg までずらす）
	void SpawnPendingEnemies();
	// 当たり判定（乗る・取り込む・接触・跳ね返した爆弾）
	void ResolveInteractions();
	// 乗っている物をスライムに追従させる
	void UpdateRiders();
	// 平らなスライムに重なった物を乗せる
	void CaptureOnFlat();
	// 元に戻った瞬間：乗っている物と足元に重なっている物を取り込み始める
	void AbsorbOnRestore();
	// 通常状態の接触（敵・トゲ＝被弾、爆弾＝跳ね返す）
	void HandleNormalContacts();
	// 跳ね返した爆弾が敵に当たったら爆発
	void HandleFriendlyBombs();
	// 吸い込み中の敵の更新（吸い込み終わったら倒す）
	void UpdateAbsorbingEnemies();
	// 弾の出来事の処理
	void HandleProjectileEvent(Projectile& projectile, Projectile::Event event);
	// 爆弾を爆発させる（敵の爆弾ならスライムに、味方の爆弾なら敵に爆風が当たる）
	void ExplodeBomb(Projectile& bomb);
	// 爆弾を跳ね返す（ボスが攻撃中ならボスへ、それ以外は輪に沿って逆向きへ）
	void ReflectBomb(Projectile& bomb);
	// 敵を倒す（absorbed：取り込みで倒したか。エフェクトが変わる）
	void KillEnemy(Enemy& enemy, bool absorbed);
	// スライムにダメージ（cause：ログに出す原因）
	void DamagePlayer(int amount, const std::string& cause);
	// 積まれた弾の生成要求から弾を出す（更新ループの後で呼ぶ）
	void ApplySpawnRequests();
	// 弾をすべて消す（withEffect：煙の火花を出す）
	void ClearProjectiles(bool withEffect);
	// 敵をすべて倒す（includePending：まだ出ていない敵も出なかったことにする）
	void KillAllEnemies(bool includePending);
	// プールから空いている敵・弾を借りる（無ければ上限まで作る。上限なら nullptr）
	Enemy* AcquireEnemy();
	Projectile* AcquireProjectile();
	// 使用中の敵・弾の数・吸い込み中の弾の数
	int CountEnemiesInUse() const;
	int CountProjectilesInUse() const;
	int CountProjectilesAbsorbing() const;

	// ===== GameSceneDebug.cpp =====
	// ImGui（Global Variables ウィンドウと Game Debug ウィンドウ）
	void UpdateImGui();
	// 出来事のログ（Game Debug に最近のものを表示する。Release では何もしない）
	void LogEvent(const std::string& text);
	// 判定の可視化の線（Game Debug の Show Hitboxes）
	void DrawHitboxLines();

	// --- 共有のモデル・ライト（このクラスが作って破棄する） ---
	GameModels models_;

	// --- ゲームの物 ---
	Field field_;
	GameCamera gameCamera_;
	Player player_;
	Boss boss_;
	Effects effects_;
	// 敵・弾のプール（必要になった分だけ作り、使い回す。WorldTransform を持つので unique_ptr）
	std::vector<std::unique_ptr<Enemy>> enemies_;
	std::vector<std::unique_ptr<Projectile>> projectiles_;
	// 弾の生成要求（敵・ボスの更新中に積まれ、ループの後でまとめて弾にする）
	std::vector<ProjectileSpawn> spawnRequests_;

	// --- フェーズ ---
	PhaseData phaseData_;
	int phaseIndex_ = 0;
	PhaseState phaseState_ = PhaseState::kIntro;
	// 今の状態になってからの時間・フェーズが始まってからの時間（秒）
	float stateTimer_ = 0.0f;
	float phaseTimer_ = 0.0f;
	// このフェーズの雑魚の出現待ち（delay の順）
	std::vector<PendingSpawn> pendingSpawns_;
	// 層の移動中のスライムの角度（移動の開始時の角度と、開始角度までの差＝短い向き）
	float advanceFromTheta_ = 0.0f;
	float advanceDeltaTheta_ = 0.0f;

	// --- UI ---
	Hud hud_;
	Fade fade_;
	PauseMenu pauseMenu_;
	// 背景の板（画面全体。色は Field > BackgroundColor）
	KamataEngine::Sprite* backgroundSprite_ = nullptr;

	// --- シーン ---
	SceneStep sceneStep_ = SceneStep::kFadeIn;
	bool isFinished_ = false;
	Result result_ = Result::kNone;
	// プレイ開始からの経過フレーム（ポーズ中は進まない）
	int32_t playFrame_ = 0;

	// --- デバッグ（ImGui） ---
	// 一時停止（シーンを作り直しても保つため静的。JSONには保存しない）
	static inline bool sDebugPause = false;
	// 判定の可視化（足元の判定を線で描く）
	static inline bool sShowHitboxes = false;
	// コマ送り（一時停止中に1フレームだけ進める）
	bool debugStepOnce_ = false;
	// 出来事のログ（新しい順。Game Debug に表示）
	std::deque<std::string> eventLog_;
	// Reload Phases の結果の表示
	std::string phaseReloadMessage_;

	// --- 調整項目（GlobalVariables の "Phase" グループ） ---
	// フェードの時間（秒）
	static inline float kFadeDuration = 0.7f;
	// 各状態の時間（秒）：PHASE n / WARNING のバナー・CLEAR! のバナー・ゲームクリアの演出・敗北の演出
	// （最後がボスのときの GAME CLEAR のバナーは、ボスの撃破演出（Boss > DefeatTime）が終わってから出る）
	static inline float kIntroTime = 2.0f;
	static inline float kClearedTime = 1.5f;
	static inline float kVictoryTime = 5.5f;
	static inline float kDefeatTime = 2.2f;
};
