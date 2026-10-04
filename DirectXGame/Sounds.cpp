#include "Sounds.h"
#include "GlobalVariables.h"
#include <filesystem>
#include <string>

using namespace KamataEngine;

uint32_t seDecide = kInvalidSound;
uint32_t seFanfare = kInvalidSound;
uint32_t seFlatten = kInvalidSound;
uint32_t seRestore = kInvalidSound;
uint32_t seEat = kInvalidSound;
uint32_t seAbsorbEnemy = kInvalidSound;
uint32_t seBounce = kInvalidSound;
uint32_t seExplosion = kInvalidSound;
uint32_t seDamage = kInvalidSound;
uint32_t seBossHit = kInvalidSound;
uint32_t sePhaseClear = kInvalidSound;
uint32_t seBossWarning = kInvalidSound;

uint32_t bgmMain = kInvalidSound;
uint32_t bgmBoss = kInvalidSound;

namespace {
// 調整項目のグループ名
const char* kGroupName = "Sound";
// サウンドファイルの置き場所（Audio::Initialize の基準 "Resources/" からの相対）
const char* kSoundDirectory = "sounds/";
// 全体音量（全サウンドに掛かる。0で無音）
float sMasterVolume = 1.0f;

// サウンド1件分（ハンドル・個別音量の調整キー・個別音量）。
// 音量の調整項目は "Volume<名前>" として Sound グループに並ぶ
// ※配列を空にすると要素数0の配列でコンパイルエラー（C2466）になるので、最低1件は残すこと
struct SoundEntry {
	uint32_t* handle;      // 対応するハンドル変数
	const char* volumeKey; // 調整項目のキー名
	float volume;          // 個別音量（MasterVolume と掛け算される）
};
// SEの既定は 0.8、BGMはループで流れ続けるので控えめの既定 0.5
SoundEntry sSoundEntries[] = {
    {&seDecide,      "VolumeDecide",      0.8f},
    {&seFanfare,     "VolumeFanfare",     0.8f},
    {&seFlatten,     "VolumeFlatten",     0.6f},
    {&seRestore,     "VolumeRestore",     0.6f},
    {&seEat,         "VolumeEat",         0.8f},
    {&seAbsorbEnemy, "VolumeAbsorbEnemy", 0.8f},
    {&seBounce,      "VolumeBounce",      0.8f},
    {&seExplosion,   "VolumeExplosion",   0.7f},
    {&seDamage,      "VolumeDamage",      0.8f},
    {&seBossHit,     "VolumeBossHit",     0.9f},
    {&sePhaseClear,  "VolumePhaseClear",  0.8f},
    {&seBossWarning, "VolumeBossWarning", 0.7f},
    {&bgmMain,       "VolumeBgmMain",     0.5f},
    {&bgmBoss,       "VolumeBgmBoss",     0.5f},
};

// 再生中のBGM（サウンドデータのハンドルと、停止・音量変更に使う再生ハンドル）
uint32_t sCurrentBgmData = kInvalidSound;
uint32_t sCurrentBgmVoice = 0;

// ハンドルに対応する個別音量を返す（見つからなければ1.0）
float GetSoundVolume(uint32_t soundDataHandle) {
	for (const SoundEntry& entry : sSoundEntries) {
		if (*entry.handle == soundDataHandle) {
			return entry.volume;
		}
	}
	return 1.0f;
}

// ファイルがあれば読み込み、無ければ kInvalidSound を返す。
// エンジンの LoadWave はファイルが無いとメッセージボックスを出して終了するため、
// サウンドが揃う前でも起動できるように先に存在を確認する
uint32_t LoadWaveIfExists(const char* fileName) {
	const std::string relativePath = std::string(kSoundDirectory) + fileName;
	if (!std::filesystem::exists(std::string("Resources/") + relativePath)) {
		return kInvalidSound;
	}
	return Audio::GetInstance()->LoadWave(relativePath);
}
} // namespace

void LoadAudio() {
	// SE（Resources/sounds/ に置く。wav形式・ファイル名は英語のみ。
	// mp3で素材が来たら Tools/ConvertSounds/convert_mp3_to_wav.ps1 で変換してから置く）
	seDecide = LoadWaveIfExists("se_decide.wav");   // 決定（エンジン付属の mokugyo.wav をコピーしたもの）
	seFanfare = LoadWaveIfExists("se_fanfare.wav"); // ファンファーレ（エンジン付属の fanfare.wav）
	// 以下は仮の合成音（Tools/PlaceholderSounds/generate_placeholder_sounds.ps1 で生成。本番の素材は同じ名前で上書きする）
	seFlatten = LoadWaveIfExists("se_flatten.wav");
	seRestore = LoadWaveIfExists("se_restore.wav");
	seEat = LoadWaveIfExists("se_eat.wav");
	seAbsorbEnemy = LoadWaveIfExists("se_absorb_enemy.wav");
	seBounce = LoadWaveIfExists("se_bounce.wav");
	seExplosion = LoadWaveIfExists("se_explosion.wav");
	seDamage = LoadWaveIfExists("se_damage.wav");
	seBossHit = LoadWaveIfExists("se_boss_hit.wav");
	sePhaseClear = LoadWaveIfExists("se_phase_clear.wav");
	seBossWarning = LoadWaveIfExists("se_boss_warning.wav");

	// BGM（ループ再生。タイトル・雑魚フェーズは main、ボスフェーズは GameScene が PlayBgm で切り替える）
	bgmMain = LoadWaveIfExists("bgm_main.wav"); // まだ素材が無い＝スキップされる
	bgmBoss = LoadWaveIfExists("bgm_boss.wav"); // まだ素材が無い＝スキップされる（ボスフェーズもメインBGMのまま）
}

void PlaySe(uint32_t soundDataHandle) {
	// 未読み込み（ファイルがまだ無い）なら何もしない
	if (soundDataHandle == kInvalidSound) {
		return;
	}
	Audio::GetInstance()->PlayWave(soundDataHandle, false, sMasterVolume * GetSoundVolume(soundDataHandle));
}

void PlayBgm(uint32_t soundDataHandle) {
	// 同じBGMが再生中なら鳴らし直さない（シーンのリトライなどで曲が頭に戻らないように）
	if (soundDataHandle == sCurrentBgmData && soundDataHandle != kInvalidSound) {
		return;
	}
	StopBgm();
	if (soundDataHandle == kInvalidSound) {
		return;
	}
	sCurrentBgmData = soundDataHandle;
	sCurrentBgmVoice = Audio::GetInstance()->PlayWave(soundDataHandle, true, sMasterVolume * GetSoundVolume(soundDataHandle));
}

void StopBgm() {
	if (sCurrentBgmData == kInvalidSound) {
		return;
	}
	Audio::GetInstance()->StopWave(sCurrentBgmVoice);
	sCurrentBgmData = kInvalidSound;
	sCurrentBgmVoice = 0;
}

void RegisterSoundGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	globalVariables->CreateGroup(kGroupName);
	globalVariables->AddItem(kGroupName, "MasterVolume", sMasterVolume);
	for (const SoundEntry& entry : sSoundEntries) {
		globalVariables->AddItem(kGroupName, entry.volumeKey, entry.volume);
	}
}

void ApplySoundGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	sMasterVolume = globalVariables->GetFloatValue(kGroupName, "MasterVolume");
	for (SoundEntry& entry : sSoundEntries) {
		entry.volume = globalVariables->GetFloatValue(kGroupName, entry.volumeKey);
	}
	// 再生中のBGMの音量も追従させる（ImGuiで動かした瞬間に効く）
	if (sCurrentBgmData != kInvalidSound) {
		Audio::GetInstance()->SetVolume(sCurrentBgmVoice, sMasterVolume * GetSoundVolume(sCurrentBgmData));
	}
}
