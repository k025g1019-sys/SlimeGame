#pragma once
#include "Enemy.h"
#include <string>
#include <vector>

// 雑魚敵1体の配置（phases.json の enemies の1要素）
struct EnemySpawnDef {
	EnemyType type = EnemyType::kWalker;
	float angleDeg = 0.0f; // 出現する角度（度。0=右、90=上、反時計回り。どのフェーズもスライムは -90=下 から始まる）
	float delay = 0.0f;    // フェーズ開始から出現までの秒数
	int dir = -1;          // walker の歩く向き（+1=反時計回り、-1=時計回り＝スライムと向かい合う）
};

// フェーズ1つ分（phases.json の phases の1要素）
struct PhaseDef {
	bool isBoss = false;
	std::string name;                   // 表示名（ASCII・kMaxNameLength 文字まで。日本語は DebugText で出ないので '?' になる）
	int bossLevel = 1;                  // ボスの強さ（1～3。攻撃の間隔と弾数が変わる）
	int bossHp = 3;                     // ボスの体力
	std::vector<EnemySpawnDef> enemies; // 雑魚（ボスフェーズでも出せる）
};

/// <summary>
/// フェーズ構成（Resources/GameData/Stage/phases.json）。配列の順にフェーズが進み、フェーズ k は層 k の輪で戦う。
/// ・コメント（// と /* */）を書ける JSON として読む
/// ・読み込みは「全部検証してから差し替える」：壊れた JSON や書き間違い（知らない項目名・範囲外・無限大になる数など）があっても
/// 　今の構成は壊さず、エラー文を残す
/// ・最初の読み込みに失敗したときは組み込みの既定構成（MakeDefault。ファイルの初期内容と同じ）で動く
/// 書き方は README の「フェーズ構成」の節と、phases.json の先頭のコメントを参照
/// </summary>
class PhaseData {
public:
	// フェーズ数・1フェーズの敵の数の上限（敵のプールの大きさ）
	static constexpr int kMaxPhases = 32;
	static constexpr int kMaxEnemiesPerPhase = 32;
	// ボスのレベルの範囲・ボスの体力の上限
	static constexpr int kMaxBossLevel = 3;
	static constexpr int kMaxBossHp = 99;
	// 表示名の長さの上限（文字数。HUD の文字＝スプライトの数と画面の幅のため）
	static constexpr int kMaxNameLength = 24;
	// 雑魚の出現角度の範囲（度。±10周まで）と出現までの秒数の上限
	static constexpr double kMaxAngleDeg = 3600.0;
	static constexpr double kMaxDelay = 3600.0;

	/// <summary>
	/// ファイルから読み込む（戻り値：成功したか。失敗したら今の構成のまま。まだ何も無ければ既定構成になる）
	/// </summary>
	bool Load();

	// フェーズの数・1つ分
	int GetCount() const { return static_cast<int>(phases_.size()); }
	const PhaseDef& Get(int index) const { return phases_[static_cast<size_t>(index)]; }

	// 最後の読み込みのエラー（成功なら空）
	const std::string& GetError() const { return error_; }
	// 今の構成が組み込みの既定構成か（true=MakeDefault、false=phases.json から読んだ構成）
	bool IsUsingDefault() const { return usingDefault_; }

	// 組み込みの既定構成（ファイルが無い・壊れているとき用。phases.json の初期内容と同じ）
	static std::vector<PhaseDef> MakeDefault();

	// JSON の文字列を検証しながら読む（成功したら out に入れて true。失敗したら error に理由）
	static bool Parse(const std::string& text, std::vector<PhaseDef>* out, std::string* error);

	// ファイルのパス（作業フォルダ DirectXGame/ からの相対）
	static constexpr const char* kFilePath = "Resources/GameData/Stage/phases.json";

private:
	std::vector<PhaseDef> phases_;
	std::string error_;
	bool usingDefault_ = false;
};
