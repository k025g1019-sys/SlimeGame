<#
.SYNOPSIS
  仮の効果音（wav）を合成して DirectXGame/Resources/sounds/ に書き出す（素材が来るまでのつなぎ）。

.DESCRIPTION
  正弦波・矩形波・三角波・ノイズにエンベロープ（音量の変化）を掛けた短い音を、
  PCM 16bit / モノラル / 44.1kHz の wav として書き出す。外部ツール不要（PowerShell 5.1 だけで動く）。
  ・既にあるファイルは上書きしない（本番の素材を置いた後に誤って消さないため）。作り直すときは -Force
  ・se_decide.wav（決定）と se_fanfare.wav（クリア）はエンジン付属の音を使っているので、ここでは作らない
  ・本番の素材に差し替えるときは、同じファイル名の wav を Resources/sounds/ に置くだけ（mp3 は
    Tools/ConvertSounds/convert_mp3_to_wav.ps1 で wav にしてから）。音量は Global Variables の Sound グループ

.PARAMETER Force
  既にあるファイルも上書きする

.PARAMETER OutDir
  書き出し先（省略時はこのスクリプトから見た ..\..\DirectXGame\Resources\sounds）

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File Tools\PlaceholderSounds\generate_placeholder_sounds.ps1
  powershell -ExecutionPolicy Bypass -File Tools\PlaceholderSounds\generate_placeholder_sounds.ps1 -Force
#>
param(
	[switch]$Force,
	[string]$OutDir = ''
)

$ErrorActionPreference = 'Stop'

if (-not $OutDir) {
	$OutDir = Join-Path $PSScriptRoot '..\..\DirectXGame\Resources\sounds'
}
$OutDir = [IO.Path]::GetFullPath($OutDir)
if (-not (Test-Path $OutDir)) {
	New-Item -ItemType Directory -Force $OutDir | Out-Null
}

# サンプリング周波数（Hz）
$script:SampleRate = 44100
# ノイズの乱数（固定シード＝毎回同じ音になる）
$script:Random = New-Object System.Random 12345

# 指定の長さ（秒）の無音のバッファを作る
function New-SoundBuffer([double]$Seconds) {
	return New-Object 'double[]' ([int][Math]::Ceiling($Seconds * $script:SampleRate))
}

# 波形の1サンプル（phase はラジアン）
function Get-WaveValue([string]$Wave, [double]$Phase) {
	switch ($Wave) {
		'square' { if ([Math]::Sin($Phase) -ge 0) { return 1.0 } else { return -1.0 } }
		'triangle' { return (2.0 / [Math]::PI) * [Math]::Asin([Math]::Sin($Phase)) }
		default { return [Math]::Sin($Phase) }
	}
}

# 音（周波数を Start→End へ変化させる）を足し込む。
# 音量は Attack 秒で立ち上がり、その後 DecayRate（/秒）で指数的に減衰し、最後の 10ms でフェードアウトする。
# VibratoHz / VibratoDepth で周波数を揺らす（Depth は周波数に対する割合）
function Add-Tone {
	param(
		[double[]]$Buffer,
		[double]$Start,
		[double]$Duration,
		[double]$FreqStart,
		[double]$FreqEnd,
		[double]$Volume,
		[string]$Wave = 'sine',
		[double]$Attack = 0.005,
		[double]$DecayRate = 0.0,
		[double]$VibratoHz = 0.0,
		[double]$VibratoDepth = 0.0
	)
	$sr = $script:SampleRate
	$first = [int]($Start * $sr)
	$count = [int]($Duration * $sr)
	$phase = 0.0
	$release = [Math]::Min(0.01, $Duration * 0.3)
	for ($i = 0; $i -lt $count; $i++) {
		$index = $first + $i
		if ($index -ge $Buffer.Length) { break }
		$t = $i / $sr
		$rate = $t / $Duration
		$freq = $FreqStart + ($FreqEnd - $FreqStart) * $rate
		if ($VibratoHz -gt 0) {
			$freq *= 1.0 + $VibratoDepth * [Math]::Sin(2.0 * [Math]::PI * $VibratoHz * $t)
		}
		$phase += 2.0 * [Math]::PI * $freq / $sr
		$envelope = 1.0
		if ($t -lt $Attack) { $envelope = $t / $Attack }
		if ($DecayRate -gt 0) { $envelope *= [Math]::Exp(-$DecayRate * $t) }
		$remain = $Duration - $t
		if ($remain -lt $release) { $envelope *= $remain / $release }
		$Buffer[$index] += $Volume * $envelope * (Get-WaveValue $Wave $phase)
	}
}

# ノイズを足し込む（Smooth は 0～1：小さいほど高い音を削ってこもった音にする＝1次のローパス）
function Add-Noise {
	param(
		[double[]]$Buffer,
		[double]$Start,
		[double]$Duration,
		[double]$Volume,
		[double]$Smooth = 1.0,
		[double]$DecayRate = 0.0
	)
	$sr = $script:SampleRate
	$first = [int]($Start * $sr)
	$count = [int]($Duration * $sr)
	$filtered = 0.0
	$release = [Math]::Min(0.01, $Duration * 0.3)
	for ($i = 0; $i -lt $count; $i++) {
		$index = $first + $i
		if ($index -ge $Buffer.Length) { break }
		$t = $i / $sr
		$white = $script:Random.NextDouble() * 2.0 - 1.0
		$filtered += ($white - $filtered) * $Smooth
		$envelope = 1.0
		if ($DecayRate -gt 0) { $envelope = [Math]::Exp(-$DecayRate * $t) }
		$remain = $Duration - $t
		if ($remain -lt $release) { $envelope *= $remain / $release }
		$Buffer[$index] += $Volume * $envelope * $filtered
	}
}

# PCM 16bit / モノラルの wav として書き出す（-1～+1 に収まらない分は切り詰める）
function Write-Wave([string]$FileName, [double[]]$Buffer) {
	$path = Join-Path $OutDir $FileName
	if ((Test-Path $path) -and -not $Force) {
		Write-Output ("skip (exists): " + $FileName)
		return
	}
	$sr = $script:SampleRate
	$dataBytes = $Buffer.Length * 2
	$stream = New-Object System.IO.MemoryStream
	$writer = New-Object System.IO.BinaryWriter $stream
	$writer.Write([Text.Encoding]::ASCII.GetBytes('RIFF'))
	$writer.Write([int](36 + $dataBytes))
	$writer.Write([Text.Encoding]::ASCII.GetBytes('WAVE'))
	$writer.Write([Text.Encoding]::ASCII.GetBytes('fmt '))
	$writer.Write([int]16)           # fmt チャンクの大きさ
	$writer.Write([int16]1)          # PCM
	$writer.Write([int16]1)          # モノラル
	$writer.Write([int]$sr)          # サンプリング周波数
	$writer.Write([int]($sr * 2))    # 1秒あたりのバイト数
	$writer.Write([int16]2)          # 1サンプルのバイト数
	$writer.Write([int16]16)         # ビット数
	$writer.Write([Text.Encoding]::ASCII.GetBytes('data'))
	$writer.Write([int]$dataBytes)
	foreach ($value in $Buffer) {
		$clamped = [Math]::Max(-1.0, [Math]::Min(1.0, $value))
		$writer.Write([int16][Math]::Round($clamped * 32767.0))
	}
	$writer.Flush()
	[IO.File]::WriteAllBytes($path, $stream.ToArray())
	$writer.Dispose()
	Write-Output ("wrote: " + $FileName + " (" + [Math]::Round($Buffer.Length / $sr, 2) + " s)")
}

# --- 平らになる（下がる「むにゅ」） ---
$buffer = New-SoundBuffer 0.18
Add-Tone -Buffer $buffer -Start 0 -Duration 0.18 -FreqStart 520 -FreqEnd 170 -Volume 0.5 -DecayRate 8
Add-Noise -Buffer $buffer -Start 0 -Duration 0.1 -Volume 0.12 -Smooth 0.08 -DecayRate 25
Write-Wave 'se_flatten.wav' $buffer

# --- 元に戻る（上がる「ぷるん」） ---
$buffer = New-SoundBuffer 0.22
Add-Tone -Buffer $buffer -Start 0 -Duration 0.22 -FreqStart 200 -FreqEnd 640 -Volume 0.5 -DecayRate 6 -VibratoHz 28 -VibratoDepth 0.06
Write-Wave 'se_restore.wav' $buffer

# --- トゲを食べる（「ぽぽっ」） ---
$buffer = New-SoundBuffer 0.17
Add-Tone -Buffer $buffer -Start 0 -Duration 0.06 -FreqStart 880 -FreqEnd 900 -Volume 0.22 -Wave 'square' -DecayRate 20
Add-Tone -Buffer $buffer -Start 0.075 -Duration 0.09 -FreqStart 1320 -FreqEnd 1400 -Volume 0.22 -Wave 'square' -DecayRate 18
Write-Wave 'se_eat.wav' $buffer

# --- 敵を取り込む（低く下がる「ごくん」） ---
$buffer = New-SoundBuffer 0.32
Add-Tone -Buffer $buffer -Start 0 -Duration 0.32 -FreqStart 380 -FreqEnd 85 -Volume 0.6 -DecayRate 5 -VibratoHz 16 -VibratoDepth 0.12
Add-Tone -Buffer $buffer -Start 0 -Duration 0.2 -FreqStart 760 -FreqEnd 170 -Volume 0.15 -Wave 'triangle' -DecayRate 12
Write-Wave 'se_absorb_enemy.wav' $buffer

# --- 爆弾を跳ね返す（弾む「ぼよーん」） ---
$buffer = New-SoundBuffer 0.38
Add-Tone -Buffer $buffer -Start 0 -Duration 0.38 -FreqStart 240 -FreqEnd 560 -Volume 0.55 -DecayRate 6 -VibratoHz 13 -VibratoDepth 0.22
Add-Noise -Buffer $buffer -Start 0 -Duration 0.03 -Volume 0.25 -Smooth 0.5 -DecayRate 60
Write-Wave 'se_bounce.wav' $buffer

# --- 爆発（こもったノイズ＋低い「どん」） ---
$buffer = New-SoundBuffer 0.75
Add-Noise -Buffer $buffer -Start 0 -Duration 0.75 -Volume 0.9 -Smooth 0.12 -DecayRate 6
Add-Tone -Buffer $buffer -Start 0 -Duration 0.5 -FreqStart 75 -FreqEnd 38 -Volume 0.6 -DecayRate 7
Write-Wave 'se_explosion.wav' $buffer

# --- スライムの被弾（濁った下がり音） ---
$buffer = New-SoundBuffer 0.26
Add-Tone -Buffer $buffer -Start 0 -Duration 0.26 -FreqStart 320 -FreqEnd 110 -Volume 0.28 -Wave 'square' -DecayRate 7
Add-Noise -Buffer $buffer -Start 0 -Duration 0.12 -Volume 0.2 -Smooth 0.4 -DecayRate 20
Write-Wave 'se_damage.wav' $buffer

# --- ボスに当たった（金属的な「がきん」） ---
$buffer = New-SoundBuffer 0.45
Add-Noise -Buffer $buffer -Start 0 -Duration 0.05 -Volume 0.5 -Smooth 0.7 -DecayRate 40
Add-Tone -Buffer $buffer -Start 0 -Duration 0.45 -FreqStart 523 -FreqEnd 515 -Volume 0.35 -Wave 'triangle' -DecayRate 9
Add-Tone -Buffer $buffer -Start 0 -Duration 0.45 -FreqStart 1245 -FreqEnd 1230 -Volume 0.2 -DecayRate 11
Add-Tone -Buffer $buffer -Start 0 -Duration 0.3 -FreqStart 130 -FreqEnd 90 -Volume 0.35 -DecayRate 10
Write-Wave 'se_boss_hit.wav' $buffer

# --- フェーズクリア（ドミソド） ---
$buffer = New-SoundBuffer 0.55
$notes = @(523.25, 659.25, 783.99)
for ($n = 0; $n -lt $notes.Count; $n++) {
	Add-Tone -Buffer $buffer -Start (0.09 * $n) -Duration 0.09 -FreqStart $notes[$n] -FreqEnd $notes[$n] -Volume 0.32 -Wave 'triangle' -DecayRate 6
}
Add-Tone -Buffer $buffer -Start 0.27 -Duration 0.28 -FreqStart 1046.5 -FreqEnd 1046.5 -Volume 0.32 -Wave 'triangle' -DecayRate 5
Write-Wave 'se_phase_clear.wav' $buffer

# --- ボスの警告（2音の警報を3回） ---
$buffer = New-SoundBuffer 0.95
for ($n = 0; $n -lt 6; $n++) {
	$freq = 440.0
	if ($n % 2 -eq 1) { $freq = 330.0 }
	Add-Tone -Buffer $buffer -Start (0.15 * $n) -Duration 0.14 -FreqStart $freq -FreqEnd $freq -Volume (0.22 * (1.0 - 0.1 * $n)) -Wave 'square' -Attack 0.01
}
Write-Wave 'se_boss_warning.wav' $buffer

Write-Output ("done: " + $OutDir)
