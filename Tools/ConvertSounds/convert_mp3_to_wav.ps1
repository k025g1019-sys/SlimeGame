# Resources/sounds/ 内の mp3 を、エンジン（KamataEngine の Audio::LoadWave）が読める wav(PCM) へ変換する。
# ffmpeg 不要（Windows 標準の MediaTranscoder を使用）。同名の .wav が既にあるものはスキップする。
# 使い方: PowerShell で実行するだけ（引数不要）
param(
	[string]$SoundDir = "$PSScriptRoot\..\..\DirectXGame\Resources\sounds"
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Runtime.WindowsRuntime

# WinRT 型のロード
$null = [Windows.Media.Transcoding.MediaTranscoder, Windows.Media, ContentType = WindowsRuntime]
$null = [Windows.Media.MediaProperties.MediaEncodingProfile, Windows.Media, ContentType = WindowsRuntime]
$null = [Windows.Storage.StorageFile, Windows.Storage, ContentType = WindowsRuntime]

# IAsyncOperation<T> を同期的に待つヘルパー
$asTaskGeneric = ([System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object {
	$_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1'
})[0]
function Await-Operation($winRtTask, $resultType) {
	$asTask = $asTaskGeneric.MakeGenericMethod($resultType)
	$netTask = $asTask.Invoke($null, @($winRtTask))
	$netTask.Wait() | Out-Null
	return $netTask.Result
}
# IAsyncActionWithProgress<double> を同期的に待つヘルパー（TranscodeAsync 用）
$asTaskAction = ([System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object {
	$_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncActionWithProgress`1'
})[0]
function Await-Action($winRtAction) {
	$asTask = $asTaskAction.MakeGenericMethod([double])
	$netTask = $asTask.Invoke($null, @($winRtAction))
	$netTask.Wait() | Out-Null
}

$SoundDir = (Resolve-Path $SoundDir).Path
$transcoder = New-Object Windows.Media.Transcoding.MediaTranscoder
$profile = [Windows.Media.MediaProperties.MediaEncodingProfile]::CreateWav([Windows.Media.MediaProperties.AudioEncodingQuality]::Medium)

foreach ($mp3 in Get-ChildItem "$SoundDir\*.mp3") {
	$wavPath = [System.IO.Path]::ChangeExtension($mp3.FullName, ".wav")
	if (Test-Path $wavPath) {
		Write-Output ("skip (exists): " + [System.IO.Path]::GetFileName($wavPath))
		continue
	}
	$srcFile = Await-Operation ([Windows.Storage.StorageFile]::GetFileFromPathAsync($mp3.FullName)) ([Windows.Storage.StorageFile])
	$dstFolder = Await-Operation ([Windows.Storage.StorageFolder]::GetFolderFromPathAsync($SoundDir)) ([Windows.Storage.StorageFolder])
	$dstFile = Await-Operation ($dstFolder.CreateFileAsync([System.IO.Path]::GetFileName($wavPath), [Windows.Storage.CreationCollisionOption]::ReplaceExisting)) ([Windows.Storage.StorageFile])
	$prepare = Await-Operation ($transcoder.PrepareFileTranscodeAsync($srcFile, $dstFile, $profile)) ([Windows.Media.Transcoding.PrepareTranscodeResult])
	if (-not $prepare.CanTranscode) {
		Write-Output ("FAILED: " + $mp3.Name + " (" + $prepare.FailureReason + ")")
		continue
	}
	Await-Action ($prepare.TranscodeAsync())
	$size = [Math]::Round((Get-Item $wavPath).Length / 1MB, 1)
	Write-Output ("converted: " + $mp3.Name + " -> " + [System.IO.Path]::GetFileName($wavPath) + " (" + $size + " MB)")
}
Write-Output "done"
