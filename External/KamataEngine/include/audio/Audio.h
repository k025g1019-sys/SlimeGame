#pragma once
#include <memory>

#include <array>
#include <atomic>
#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include <wrl.h>
#include <xaudio2.h>

namespace KamataEngine {

/// <summary>
/// オーディオ
/// </summary>
class Audio {
public:
	// サウンドデータの最大数
	static const int kMaxSoundData = 256;

	// 音声データ
	struct SoundData {
		// 波形フォーマット
		WAVEFORMATEX wfex;
		// バッファ
		std::vector<uint8_t> buffer;
		// 名前
		std::string name;
	};

	// 再生データ
	class Voice {
	public:
		uint32_t handle = 0u;
		IXAudio2SourceVoice* sourceVoice = nullptr;
		std::atomic_bool isFinished = false;
		~Voice();
	};

	/// <summary>
	/// オーディオコールバック
	/// </summary>
	class XAudio2VoiceCallback : public IXAudio2VoiceCallback {
	public:
		// ボイス処理パスの開始時
		STDMETHOD_(void, OnVoiceProcessingPassStart)
		([[maybe_unused]] THIS_ UINT32 BytesRequired){};
		// ボイス処理パスの終了時
		STDMETHOD_(void, OnVoiceProcessingPassEnd)(THIS){};
		// バッファストリームの再生が終了した時
		STDMETHOD_(void, OnStreamEnd)(THIS){};
		// バッファの使用開始時
		STDMETHOD_(void, OnBufferStart)([[maybe_unused]] THIS_ void* pBufferContext){};
		// バッファの末尾に達した時
		STDMETHOD_(void, OnBufferEnd)(THIS_ void* pBufferContext);
		// 再生がループ位置に達した時
		STDMETHOD_(void, OnLoopEnd)([[maybe_unused]] THIS_ void* pBufferContext){};
		// ボイスの実行エラー時
		STDMETHOD_(void, OnVoiceError)
		([[maybe_unused]] THIS_ void* pBufferContext, [[maybe_unused]] HRESULT Error){};

		void SetAudio(Audio* audio) { audio_.store(audio); }

	private:
		std::atomic<Audio*> audio_ = nullptr;
	};

	static Audio* GetInstance();

	/// <summary>
	/// 静的終了処理
	/// </summary>
	static void Terminate();

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(const std::string& directoryPath = "Resources/");

	/// <summary>
	/// 毎フレーム処理
	/// </summary>
	void Update();

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize();

	/// <summary>
	/// WAV音声読み込み
	/// </summary>
	/// <param name="filename">WAVファイル名</param>
	/// <returns>サウンドデータハンドル</returns>
	uint32_t LoadWave(const std::string& filename);

	/// <summary>
	/// サウンドデータの解放
	/// </summary>
	/// <param name="soundData">サウンドデータ</param>
	void Unload(SoundData* soundData);

	/// <summary>
	/// 音声再生
	/// </summary>
	/// <param name="soundDataHandle">サウンドデータハンドル</param>
	/// <param name="loopFlag">ループ再生フラグ</param>
	/// <param name="volume">ボリューム
	/// 0で無音、1がデフォルト音量。あまり大きくしすぎると音割れする</param>
	/// <returns>再生ハンドル</returns>
	uint32_t PlayWave(uint32_t soundDataHandle, bool loopFlag = false, float volume = 1.0f);

	/// <summary>
	/// 音声停止
	/// </summary>
	/// <param name="voiceHandle">再生ハンドル</param>
	void StopWave(uint32_t voiceHandle);

	/// <summary>
	/// 音声再生中かどうか
	/// </summary>
	/// <param name="voiceHandle">再生ハンドル</param>
	/// <returns>音声再生中かどうか</returns>
	bool IsPlaying(uint32_t voiceHandle);

	/// <summary>
	/// 音声一時停止
	/// </summary>
	/// <param name="voiceHandle">再生ハンドル</param>
	void PauseWave(uint32_t voiceHandle);

	/// <summary>
	/// 音声一時停止からの再開
	/// </summary>
	/// <param name="voiceHandle">再生ハンドル</param>
	void ResumeWave(uint32_t voiceHandle);

	/// <summary>
	/// 音量設定
	/// </summary>
	/// <param name="voiceHandle">再生ハンドル</param>
	/// <param name="volume">ボリューム
	/// 0で無音、1がデフォルト音量。あまり大きくしすぎると音割れする</param>
	void SetVolume(uint32_t voiceHandle, float volume);

private:
	Audio(const Audio&) = delete;
	const Audio& operator=(const Audio&) = delete;

	static std::unique_ptr<Audio> sInstance_;

public:
	struct Passkey {
	private:
		friend Audio;
		Passkey() = default;
	};

	Audio(Passkey);

private:
	friend std::default_delete<Audio>;
	Audio() = default;
	~Audio();
	void RemoveFinishedVoices();

	// XAudio2のインスタンス
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
	// サウンドデータコンテナ
	std::array<SoundData, kMaxSoundData> soundDatas_;
	// 再生中データコンテナ
	// AudioのAPIとUpdateは同一スレッドから呼ばれる前提
	// std::unordered_map<uint32_t, IXAudio2SourceVoice*> voices_;
	std::vector<std::unique_ptr<Voice>> voices_;
	// サウンド格納ディレクトリ
	std::string directoryPath_;
	// 次に使うサウンドデータの番号
	uint32_t indexSoundData_ = 0u;
	// 次に使う再生中データの番号
	uint32_t indexVoice_ = 0u;
	// オーディオコールバック
	XAudio2VoiceCallback voiceCallback_;
	std::atomic_bool hasFinishedVoice_ = false;
};

} // namespace KamataEngine
