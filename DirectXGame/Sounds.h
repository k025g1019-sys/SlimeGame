#pragma once

#include <KamataEngine.h>

/// <summary>
/// サウンド（SE・BGM）のハンドル管理と再生ヘルパー。
/// ・LoadAudio() : main起動時に全サウンドを読み込む。ファイルが無いものはスキップされ、
/// 　　　　　　　　鳴らないだけでゲームは普通に動く（サウンドが揃う前でも起動できる）
/// ・PlaySe()   : SEを1回鳴らす（音量 = Sound > MasterVolume × そのSE個別のVolume）
/// ・PlayBgm()  : BGMをループ再生する（再生中のBGMは止まる。同じBGMなら何もしない＝鳴らし直さない）
/// ・StopBgm()  : BGMを止める
/// ・音量はサウンドごとに Sound グループの Volume〜 で個別調整できる（ImGuiで即反映。BGMは再生中でも効く）
/// ・ファイルは Resources/sounds/ に置く（**wav形式のみ**。mp3で素材が来たら
/// 　Tools/ConvertSounds/convert_mp3_to_wav.ps1 で変換してから使う。ファイル名は英語のみ）
///
/// 【サウンドの増やし方】
/// 　1. wav を Resources/sounds/ に置く
/// 　2. このファイルにハンドルの extern を1行、Sounds.cpp に実体（= kInvalidSound）を1行足す
/// 　3. Sounds.cpp の sSoundEntries に {&ハンドル, "Volume名前", 既定音量} を1行足す（個別音量の調整項目になる）
/// 　4. Sounds.cpp の LoadAudio に LoadWaveIfExists("ファイル名.wav") を1行足す
/// 　5. 鳴らしたい所で PlaySe(ハンドル) / PlayBgm(ハンドル)
/// </summary>

// 無効ハンドル（ファイルが無い等で読み込まれていない印。PlaySe/PlayBgm に渡しても何も起きない。
// ※エンジンの LoadWave は 0 も有効なハンドルとして返すため、0 ではなくこの値を使う）
constexpr uint32_t kInvalidSound = 0xffffffff;

// --- SEのハンドル（LoadAudio で読み込まれる。未読み込みは kInvalidSound のまま） ---
// ※se_decide / se_fanfare 以外は仮の合成音（Tools/PlaceholderSounds/generate_placeholder_sounds.ps1 で生成）。
// 　本番の素材は同じファイル名の wav で上書きすればよい

// 決定（タイトルの決定など。エンジン付属の mokugyo.wav）
extern uint32_t seDecide;
// ファンファーレ（ゲームクリア。エンジン付属の fanfare.wav）
extern uint32_t seFanfare;
// 平らになった
extern uint32_t seFlatten;
// 元に戻った（スペースを離した）
extern uint32_t seRestore;
// トゲ弾を食べた
extern uint32_t seEat;
// 敵を取り込んだ
extern uint32_t seAbsorbEnemy;
// 爆弾を跳ね返した
extern uint32_t seBounce;
// 爆発
extern uint32_t seExplosion;
// スライムの被弾
extern uint32_t seDamage;
// ボスに爆弾が当たった
extern uint32_t seBossHit;
// フェーズクリア
extern uint32_t sePhaseClear;
// ボスフェーズの開始（WARNING）
extern uint32_t seBossWarning;

// --- BGMのハンドル（PlayBgm で使う） ---

// メインBGM（sounds/bgm_main.wav。まだ素材が無い＝スキップされる。置けばタイトルと雑魚フェーズで流れる）
extern uint32_t bgmMain;
// ボスBGM（sounds/bgm_boss.wav。まだ素材が無い。置けばボスフェーズで流れる。無ければメインBGMのまま）
extern uint32_t bgmBoss;

// 読み込み関数
void LoadAudio();

// SEを1回鳴らす（音量 = MasterVolume × 個別Volume。未読み込みハンドルなら何もしない）
void PlaySe(uint32_t soundDataHandle);

// BGMをループ再生する（再生中のBGMは停止して切り替える。同じBGMが再生中なら何もしない）
void PlayBgm(uint32_t soundDataHandle);

// BGMを止める（BGM無しの画面用）
void StopBgm();

// 調整項目（音量）の登録（既定値をGlobalVariablesへ追加する。mainが起動時に呼ぶ）
void RegisterSoundGlobalVariables();
// 調整項目（音量）の適用（GlobalVariablesの現在値を反映する。毎フレーム呼ぶ→ImGui編集が即反映。
// 再生中のBGMの音量もここで追従する）
void ApplySoundGlobalVariables();
